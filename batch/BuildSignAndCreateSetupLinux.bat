@echo on
setlocal

REM BuildSignAndCreateSetupLinux.bat
REM Builds the linux (l) flavor of GeoDMS via WSL2 (CMake), then runs
REM nsi/CreateLinuxSetup.sh to package + sign, then optionally installs the
REM .deb via dpkg inside WSL so it can be tested via:
REM    python full.py -version <ver>l
REM (the linux flavor of full.py invokes the binary via `wsl --` — see
REM the linux-handling extension in full.py's _resolve_local_build).
REM
REM Sister script of BuildSignAndCreateSetup.bat (m, msbuild) and
REM BuildSignAndCreateSetupCmake.bat (c, cmake).
REM
REM Prereqs:
REM   - WSL2 with Ubuntu 24.04 (or similar) installed
REM   - cmake, ninja, gcc-13, vcpkg available inside WSL
REM   - Linux build tree: build/linux-x64-release/  (configured separately)
REM   - dpkg-deb available in WSL (for .deb creation)
REM
REM Run from the repo root:  <repo-root>>BuildSignAndCreateSetupLinux.bat

cls

REM This script now lives in <repo-root>\batch; re-root to the repo root so the %cd%
REM capture below (and the WSL geodms_wsldir derived from it) points at the repo, and
REM the relative nsi/ + TestLinuxReleaseUnit.sh references resolve.
cd /d "%~dp0.."

REM Version comes from nsi\GeoDmsVersion.cmd (shared with the msbuild + cmake
REM sister scripts). Bump the patch number there, not here.
call GeoDmsVersion.cmd
set GeoDmsFlavor=l

set geodms_rootdir=%cd%
set GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%

REM WSL view of the repo root, derived from %cd% so this script is not tied to a
REM specific checkout path (was hardcoded to /mnt/c/dev/GeoDMS_2026). wslpath maps the
REM Windows path to /mnt/<drive>/...; used in the `wsl bash -c` invocations below.
for /f "usebackq delims=" %%i in (`wsl wslpath -a "%geodms_rootdir%"`) do set "geodms_wsldir=%%i"

REM Mark script start so we can verify cmake --build actually produced a fresh
REM binary. cmake exits 0 even when the dep tracker decided nothing needed
REM rebuilding -- which silently ships stale binaries inside the .deb.
REM -5s fudge to absorb any clock skew between WSL filesystem and Windows.
for /f "delims=" %%T in ('powershell -NoProfile -Command "[DateTime]::Now.AddSeconds(-5).ToString('o')"') do set BUILD_GATE_TIME=%%T

echo --- building linux-x64-release in WSL ---
REM Share the vc_archives binary cache with the msbuild (.m) and cmake (.c)
REM flavors. Only matters if the WSL build tree needs reconfiguring (which
REM re-triggers vcpkg install via the toolchain file); harmless for pure
REM --build. vcpkg keys archives by ABI hash, so Linux gcc entries coexist
REM with the Windows MSVC entries already in vc_archives.
REM Wipe the build OUTPUT folder (build/linux-x64-release/bin -- the .so libs +
REM ELF binaries CreateLinuxSetup.sh packages) before building, so obsolete
REM binaries from before a component rename/removal cannot linger and ship in
REM the .deb/.tar.gz. Object files live under CMakeFiles/ (not bin), so this
REM stays an incremental compile + relink, not a full rebuild.
wsl bash -c "export VCPKG_BINARY_SOURCES='clear;files,%geodms_wsldir%/vc_archives,readwrite' && cd %geodms_wsldir% && rm -rf build/linux-x64-release/bin && cmake --build build/linux-x64-release --config Release"
if errorlevel 1 goto :build_failed

REM Linux ELF has no Windows FileVersion -- use mtime: GeoDmsRun must be at
REM least as new as the script start, otherwise cmake --build was a no-op.
powershell -NoProfile -Command "if ((Get-Item 'build\linux-x64-release\bin\GeoDmsRun').LastWriteTime -ge [DateTime]'%BUILD_GATE_TIME%') { exit 0 } else { exit 1 }"
if errorlevel 1 (
    echo *** ABORT: build\linux-x64-release\bin\GeoDmsRun was not rebuilt - cmake --build was a no-op against stale binary. ***
    goto :build_failed
)

echo --- creating Linux setup (.tar.gz + .deb, signed via PowerShell .NET SignedCms) ---
wsl bash -c "cd %geodms_wsldir% && export GeoDmsVersion=%GeoDmsVersion% && bash nsi/CreateLinuxSetup.sh"
if errorlevel 1 goto :nsis_failed

set DEB=distr\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%-linux-x64.deb
set TARBALL=distr\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%-linux-x64.tar.gz
if not exist "%DEB%" (
    if not exist "%TARBALL%" (
        echo CreateLinuxSetup.sh produced neither %DEB% nor %TARBALL%
        goto :nsis_failed
    )
)

echo --- installing .deb in WSL (passwordless sudo required, or skipped) ---
wsl bash -c "if [ -f '%geodms_wsldir%/%DEB:\=/%' ] && sudo -n true 2>/dev/null; then sudo dpkg -i '%geodms_wsldir%/%DEB:\=/%'; else echo '** skipping dpkg install (no passwordless sudo) — install manually with:  wsl sudo dpkg -i %geodms_wsldir%/%DEB:\=/%'; fi"

REM Post-install unit tests (mirrors the unit.bat call in BuildSignAndCreateSetup.bat
REM (.m) and BuildSignAndCreateSetupCmake.bat (.c)). TestLinuxReleaseUnit.sh runs
REM tst/batch/unit_linux.sh against build/linux-x64-release and exits non-zero on
REM any FAILED test. On failure we REMOVE the installed package and the built setup
REM files: a warning echo is too easy to miss in the long build log, and a build
REM with failing unit tests must not remain installed or shippable.
echo --- running Linux unit tests ---
wsl bash -c "cd %geodms_wsldir% && bash TestLinuxReleaseUnit.sh"
if errorlevel 1 goto :unit_failed

echo === DONE: GeoDms%GeoDmsVersion%.%GeoDmsFlavor% built and packaged ===
if exist "%DEB%"     echo   .deb:     %DEB%
if exist "%TARBALL%" echo   tarball:  %TARBALL%
echo Run regression with:    python full.py -version %GeoDmsVersion%.%GeoDmsFlavor%
endlocal
exit /B 0

:build_failed
echo *** Linux cmake build failed ***
endlocal
exit /B 1

:nsis_failed
echo *** CreateLinuxSetup.sh failed ***
endlocal
exit /B 1

:unit_failed
echo *** Linux unit tests FAILED - removing installed package and setup files so a broken build cannot ship ***
REM dpkg -r removes the package (and the /opt/ObjectVision/GeoDms<ver>.l tree it
REM owns); the explicit rm -rf is belt-and-suspenders for the versioned dir.
wsl bash -c "sudo -n dpkg -r geodms 2>/dev/null; sudo -n rm -rf '/opt/ObjectVision/GeoDms%GeoDmsVersion%.%GeoDmsFlavor%'"
del /q "%DEB%" 2>nul
del /q "%TARBALL%" 2>nul
del /q "%TARBALL%.sha256" 2>nul
del /q "%TARBALL%.sha256.p7s" 2>nul
echo Removed install /opt/ObjectVision/GeoDms%GeoDmsVersion%.%GeoDmsFlavor% and distr setup files for %GeoDmsVersion%.%GeoDmsFlavor%.
endlocal
exit /B 1
