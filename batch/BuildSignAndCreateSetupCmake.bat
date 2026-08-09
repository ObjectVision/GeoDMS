@echo on
setlocal

REM BuildSignAndCreateSetupCmake.bat
REM Builds the cmake (c) flavor of GeoDMS, packages via NSIS, signs, and
REM silently installs to %ProgramFiles%\ObjectVision\GeoDms<ver>c so it
REM can be tested via:  python full.py -version <ver>c
REM
REM Sister script of BuildSignAndCreateSetup.bat (m flavor, msbuild) and
REM BuildSignAndCreateSetupLinux.bat (l flavor, WSL).
REM
REM Run from the repo root:  <repo-root>>BuildSignAndCreateSetupCmake.bat

cls

REM This script now lives in <repo-root>\batch; re-root to the repo root so all the
REM relative paths below (all22.sln, nsi, distr, ..\tst\batch, %cd% capture) resolve.
cd /d "%~dp0.."

REM Version comes from nsi\GeoDmsVersion.cmd (shared with the msbuild + linux
REM sister scripts). Bump the patch number there, not here.
call GeoDmsVersion.cmd
set GeoDmsFlavor=c

set geodms_rootdir=%cd%
set GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%

REM Share the vc_archives binary cache with the msbuild (.m) flavor — see
REM DmsDef.props VcpkgAdditionalInstallOptions. Without this, cmake falls
REM back to %LOCALAPPDATA%\vcpkg\archives and rebuilds every port from
REM source after a compiler / toolchain hash change.
set VCPKG_BINARY_SOURCES=clear;files,%geodms_rootdir%\vc_archives,readwrite
REM Share the in-repo vc_downloads tarball cache with .m too (DmsDef.props --downloads-root),
REM so .m and .c use identical vcpkg tool + binary cache + downloads.
set VCPKG_DOWNLOADS=%geodms_rootdir%\vc_downloads

REM Pull tst on the parallel checkout for the post-install regression run.
cd ..
md tst 2>nul
cd tst
git pull
cd %geodms_rootdir%


REM Drive the build with the cmake that vcpkg itself pins (vcpkg\scripts\vcpkg-tools.json,
REM i.e. whatever the submodule says), NOT the VS-bundled one. Both bit us on 2026-08-09:
REM   1) a VS update silently replaced its bundled cmake 4.2 -> 4.3 and deleted
REM      share\cmake-4.2; the CMAKE_ROOT cached in an existing build dir still pointed there,
REM      so every configure died on "CMakeSystem.cmake.in does not exist";
REM   2) the cmake VERSION is an ABI input for every vcpkg-cmake port, so a cmake that drifts
REM      out from under us invalidates all of vc_archives and costs a multi-hour rebuild.
REM `vcpkg fetch cmake` prints (downloading if needed) exactly the pinned tool, so this tracks
REM the submodule automatically -- no version literal to maintain here. Falls back to the VS
REM copy when vcpkg.exe is not bootstrapped yet (fresh clone; the toolchain bootstraps during
REM configure, and a later run picks up the pinned one).
set CMAKE=
for /f "usebackq delims=" %%C in (`"%geodms_rootdir%\vcpkg\vcpkg.exe" fetch cmake --x-stderr-status 2^>nul`) do set CMAKE="%%C"
if not defined CMAKE (
    echo --- 'vcpkg fetch cmake' unavailable; falling back to the VS-bundled cmake ---
    set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
echo --- cmake: %CMAKE%

REM Say up front how much vcpkg work this build implies. A full re-churn is the CORRECT
REM outcome of a baseline bump or a compiler re-pin, so this only warns -- but it turns an
REM unexplained 47-minute stall into an expected, attributable cost. CHOICE has a 30s
REM timeout defaulting to Yes so an unattended Build.bat still completes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\vcpkg-drift-check.ps1" -Triplet x64-windows-v145
if errorlevel 1 if not errorlevel 2 (
    choice /C YN /T 30 /D Y /M "Continue with this build"
    if errorlevel 2 goto :build_failed
)

set BUILD_DIR=build\windows-x64-release

REM Mark script start so the post-build staleness guard can verify cmake
REM --build actually produced a fresh binary. cmake exits 0 on no-op
REM dep-tracker decisions; binaries don't carry FileVersion metadata, so
REM mtime is the only reliable signal. -5s fudge to absorb clock skew.
for /f "delims=" %%T in ('powershell -NoProfile -Command "[DateTime]::Now.AddSeconds(-5).ToString('o')"') do set BUILD_GATE_TIME=%%T

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo --- configuring %BUILD_DIR% ---
    REM Toolchain (CMAKE_TOOLCHAIN_FILE) comes from the preset = tools/vcpkg-toolchain.cmake
    REM (the in-repo ./vcpkg), matching the .m flavour. Do NOT override it to the
    REM VS-bundled vcpkg (...\VC\vcpkg\...) — that is a different tool/ABI and forces a
    REM full re-churn of the shared vcpkg_installed every time .m and .c alternate.
    %CMAKE% --preset windows-x64-release ^
        -DVCPKG_INSTALLED_DIR="%geodms_rootdir%/vcpkg_installed" ^
        -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
    if errorlevel 1 goto :build_failed
)

REM Refuse to build only if a GeoDmsRun/GeoDmsGuiQt is running FROM the build output
REM (%geodms_rootdir%\%BUILD_DIR%\bin) -- a held handle on its Dm*.dll silently turns
REM the build into a no-op. A process from an INSTALLED build (e.g. full.py driving
REM GeoDms<ver>.c\GeoDmsRun.exe) does NOT lock this output, so it is ignored. Path is
REM derived from %geodms_rootdir%, so this works wherever the working copy lives.
powershell -NoProfile -Command "$b=(Resolve-Path -LiteralPath '%geodms_rootdir%\%BUILD_DIR%\bin' -EA SilentlyContinue); $hit=0; if($b){$p=($b.Path.TrimEnd('\')+'\').ToLower(); foreach($pr in (Get-Process GeoDmsRun,GeoDmsGuiQt -EA SilentlyContinue)){ if($pr.Path -and $pr.Path.ToLower().StartsWith($p)){$hit=1} } }; exit $hit"
if errorlevel 1 (
    echo *** ABORT: a GeoDmsRun/GeoDmsGuiQt is running from %BUILD_DIR%\bin - it locks the build's Dm*.dll. Close it. ***
    goto :build_failed
)

REM Refuse to proceed while ANOTHER build (MSBuild or cmake-driven) is running:
REM concurrent builds of this repo interleave writes into the packaged output,
REM so NSIS ships a TORN binary snapshot whose unit tests all fail (see the
REM 2026-07-20 20.9.0.m incident in the msbuild sister script). If it provably
REM targets another repo, the CHOICE allows continuing.
powershell -NoProfile -Command "exit ([int](((Get-Process MSBuild,cmake -EA SilentlyContinue) | Measure-Object).Count -gt 0))"
if errorlevel 1 (
    echo *** WARNING: an MSBuild.exe/cmake.exe is currently running. A concurrent build of THIS repo tears the setup contents. ***
    CHOICE /M "Continue anyway (only safe if that build targets ANOTHER repo)?"
    if ErrorLevel 2 goto :build_failed
)

REM Wipe the build OUTPUT folder (%BUILD_DIR%\bin -- the final DLLs/EXEs that
REM NSIS packages) before building, so obsolete binaries from before a component
REM rename/removal cannot linger and ship in the installer. CMakeCache.txt and
REM the CMakeFiles\ object tree live under %BUILD_DIR% (not bin), so configure is
REM skipped and compilation stays incremental -- only a relink/redeploy happens.
REM (Runs after the lock check above so no held handle can block the rmdir.)
if exist "%BUILD_DIR%\bin" rmdir /s /q "%BUILD_DIR%\bin"

echo --- building cmake-Release ---
%CMAKE% --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :build_failed

REM cmake --build exits 0 even when the dep tracker decided nothing needed
REM rebuilding -- which silently ships stale binaries. Binaries carry no
REM FileVersion, so use mtime. Check DmRtc.dll, not GeoDmsRun.exe:
REM GeoDmsVersion.cmd rewrites buildstamp.h once per build session (newer than
REM the previous session's binaries), which forces DmRtc to relink -- so
REM DmRtc.dll fresher than script start proves cmake actually executed.
REM Downstream binaries may skip
REM relink when DmRtc's ABI is unchanged (correct incremental optimization,
REM not staleness).
powershell -NoProfile -Command "if ((Get-Item '%BUILD_DIR%\bin\DmRtc.dll').LastWriteTime -ge [DateTime]'%BUILD_GATE_TIME%') { exit 0 } else { exit 1 }"
if errorlevel 1 (
    echo *** ABORT: %BUILD_DIR%\bin\DmRtc.dll was not rebuilt - cmake --build was a no-op against stale binaries. ***
    goto :build_failed
)

echo --- creating NSIS installer ---
mkdir distr 2>nul
cd nsi
"C:\Program Files (x86)\NSIS\makensis.exe" DmsSetupScriptX64-cmake.nsi
if errorlevel 1 (
    cd ..
    goto :nsis_failed
)
cd ..

set INSTALLER=distr\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%-Setup-x64.exe
if not exist "%INSTALLER%" (
    echo NSIS produced no installer at %INSTALLER%
    goto :nsis_failed
)

echo --- signing %INSTALLER% ---
set SIGNTOOL=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe
"%SIGNTOOL%" sign /debug /a /n "Object Vision" /fd SHA256 ^
    /tr http://timestamp.globalsign.com/tsa/r6advanced1 /td SHA256 "%INSTALLER%"
if errorlevel 1 goto :sign_failed

set INSTALL_DIR=%ProgramFiles%\ObjectVision\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%
if exist "%INSTALL_DIR%" (
    echo --- silent uninstall of previous %INSTALL_DIR% ---
    if exist "%INSTALL_DIR%\uninstaller.exe" "%INSTALL_DIR%\uninstaller.exe" /S _?=%INSTALL_DIR%
)

echo --- silent install ---
"%INSTALLER%" /S
echo Installed to: %INSTALL_DIR%

del filelist%GeoDmsVersion%.%GeoDmsFlavor%.txt 2>nul
FORFILES /P "%INSTALL_DIR%" /S /C "cmd /c echo @relpath" >> filelist%GeoDmsVersion%.%GeoDmsFlavor%.txt 2>nul

REM Post-install unit tests (mirrors BuildSignAndCreateSetup.bat for the .m
REM flavor). Flavor passed separately so unit_flagged.bat ->
REM SetGeoDMSPlatform.bat composes the install dir as GeoDms<ver>.<flavor>.
cd ..\tst\batch
Call unit.bat %GeoDmsVersion% c off
cd %geodms_rootdir%

REM Harness unit-test failure. unit.bat sets no errorlevel, so scan the newest
REM result file (v<ver>.<flavor>_*.txt under %LocalDataDir%\GeoDMSTestResults\unit,
REM where unit.bat -> SetLocalDataDir.bat just set %LocalDataDir%) for a FAILED line.
REM On failure REMOVE the installed build and the signed setup file: a warning echo
REM is too easy to miss, and a build with failing unit tests must not stay installed/shippable.
powershell -NoProfile -Command "$d='%LocalDataDir%\GeoDMSTestResults\unit'; $f=Get-ChildItem (Join-Path $d 'v%GeoDmsVersion%.%GeoDmsFlavor%_*.txt') -EA SilentlyContinue | Sort-Object LastWriteTime -Desc | Select-Object -First 1; if(-not $f){Write-Host 'no unit result file found - treating as failure'; exit 1}; if(Select-String -Path $f.FullName -Pattern 'FAILED' -Quiet){Write-Host ('unit FAILED: '+$f.Name); exit 1} else {Write-Host ('unit OK: '+$f.Name); exit 0}"
if errorlevel 1 goto :unit_failed

echo === DONE: GeoDms%GeoDmsVersion%.%GeoDmsFlavor% built, signed, installed ===
echo Run regression with:    python full.py -version %GeoDmsVersion%.%GeoDmsFlavor%
endlocal
exit /B 0

:unit_failed
echo *** Unit tests FAILED - removing installed build and signed setup file so a broken build cannot ship ***
if exist "%INSTALL_DIR%\uninstaller.exe" "%INSTALL_DIR%\uninstaller.exe" /S _?=%INSTALL_DIR%
if exist "%INSTALL_DIR%" rmdir /s /q "%INSTALL_DIR%"
del /q "%INSTALLER%" 2>nul
echo Removed "%INSTALL_DIR%" and "%INSTALLER%"
endlocal
exit /B 1

:build_failed
echo *** cmake build failed ***
endlocal
exit /B 1

:nsis_failed
echo *** NSIS step failed ***
endlocal
exit /B 1

:sign_failed
echo *** signing failed ***
endlocal
exit /B 1
