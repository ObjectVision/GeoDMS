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
REM Run from the repo root:  C:\dev\GeoDMS_2026>BuildSignAndCreateSetupCmake.bat

cls

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

REM Pull tst on the parallel checkout for the post-install regression run.
cd ..
md tst 2>nul
cd tst
git pull
cd %geodms_rootdir%


set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set BUILD_DIR=build\windows-x64-release

REM Mark script start so the post-build staleness guard can verify cmake
REM --build actually produced a fresh binary. cmake exits 0 on no-op
REM dep-tracker decisions; binaries don't carry FileVersion metadata, so
REM mtime is the only reliable signal. -5s fudge to absorb clock skew.
for /f "delims=" %%T in ('powershell -NoProfile -Command "[DateTime]::Now.AddSeconds(-5).ToString('o')"') do set BUILD_GATE_TIME=%%T

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo --- configuring %BUILD_DIR% ---
    %CMAKE% --preset windows-x64-release ^
        -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
        -DVCPKG_INSTALLED_DIR="%geodms_rootdir%/vcpkg_installed" ^
        -DCMAKE_PREFIX_PATH="C:/Qt/6.9.0/msvc2022_64"
    if errorlevel 1 goto :build_failed
)

REM Refuse to build if a prior bin\GeoDmsGuiQt.exe or bin\GeoDmsRun.exe is still
REM running -- a held file handle on Dm*.dll silently turns the build into a
REM no-op (worst case: ships a stale setup signed with today's version).
tasklist /FI "IMAGENAME eq GeoDmsGuiQt.exe" 2>nul | findstr /I /C:"GeoDmsGuiQt.exe" >nul
if not errorlevel 1 (
    echo *** ABORT: GeoDmsGuiQt.exe is running. Close it before building - it locks %BUILD_DIR%\bin\Dm*.dll ***
    goto :build_failed
)
tasklist /FI "IMAGENAME eq GeoDmsRun.exe" 2>nul | findstr /I /C:"GeoDmsRun.exe" >nul
if not errorlevel 1 (
    echo *** ABORT: GeoDmsRun.exe is running. Close it before building. ***
    goto :build_failed
)

echo --- building cmake-Release ---
%CMAKE% --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :build_failed

REM cmake --build exits 0 even when the dep tracker decided nothing needed
REM rebuilding -- which silently ships stale binaries. Binaries carry no
REM FileVersion, so use mtime. Check DmRtc.dll, not GeoDmsRun.exe:
REM GeoDmsVersion.cmd rewrites buildstamp.h on every run, which forces
REM DmRtc to relink unconditionally -- so DmRtc.dll fresher than script
REM start proves cmake actually executed. Downstream binaries may skip
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

echo === DONE: GeoDms%GeoDmsVersion%.%GeoDmsFlavor% built, signed, installed ===
echo Run regression with:    python full.py -version %GeoDmsVersion%.%GeoDmsFlavor%
endlocal
exit /B 0

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
