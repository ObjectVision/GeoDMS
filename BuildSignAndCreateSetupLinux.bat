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
REM Run from the repo root:  C:\dev\GeoDMS_2026>BuildSignAndCreateSetupLinux.bat

cls

REM Version comes from nsi\GeoDmsVersion.cmd (shared with the msbuild + cmake
REM sister scripts). Bump the patch number there, not here.
call GeoDmsVersion.cmd
set GeoDmsFlavor=l

set geodms_rootdir=%cd%
set GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%

echo --- building linux-x64-release in WSL ---
wsl bash -c "cd /mnt/c/dev/GeoDMS_2026 && cmake --build build/linux-x64-release --config Release"
if errorlevel 1 goto :build_failed

echo --- creating Linux setup (.tar.gz + .deb, signed via PowerShell .NET SignedCms) ---
wsl bash -c "cd /mnt/c/dev/GeoDMS_2026 && export GeoDmsVersion=%GeoDmsVersion% && bash nsi/CreateLinuxSetup.sh"
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
wsl bash -c "if [ -f '/mnt/c/dev/GeoDMS_2026/%DEB:\=/%' ] && sudo -n true 2>/dev/null; then sudo dpkg -i '/mnt/c/dev/GeoDMS_2026/%DEB:\=/%'; else echo '** skipping dpkg install (no passwordless sudo) — install manually with:  wsl sudo dpkg -i /mnt/c/dev/GeoDMS_2026/%DEB:\=/%'; fi"

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
