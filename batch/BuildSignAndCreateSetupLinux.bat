@echo on
setlocal

REM BuildSignAndCreateSetupLinux.bat
REM Builds the linux (l) flavor of GeoDMS via WSL2 (CMake), then runs
REM nsi/CreateLinuxSetup.sh to package + sign (that script stages ONCE on the
REM WSL-native fs, builds .deb and .tar.gz from the same tree, and FAILS if
REM their payloads diverge or a critical runtime file -- e.g. the profiler/
REM sampler scripts the .l regression needs -- is missing), then optionally
REM installs the .deb via dpkg inside WSL so it can be tested via:
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
REM   - Linux build tree build/linux-x64-release/ is auto-configured on first run
REM     (cmake --preset linux-x64-release) if its CMakeCache.txt is absent
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

REM Configure the Linux build tree if it isn't configured yet (mirrors the
REM windows-x64-release configure guard in BuildSignAndCreateSetupCmake.bat). Without
REM this, a fresh checkout or a wiped build/ makes `cmake --build` fail cryptically with
REM "build/linux-x64-release is not a directory". The committed preset
REM (CMakePresets.json > linux-x64-release) sets Ninja + Release + triplet x64-linux + the
REM in-repo ./vcpkg toolchain (tools/vcpkg-toolchain.cmake); no extra -D flags are needed
REM (the Windows preset's VCPKG_INSTALLED_DIR / Qt CMAKE_PREFIX_PATH overrides are
REM Windows-only). The FIRST configure triggers a full vcpkg install of the Linux deps --
REM long unless vc_archives already holds matching gcc entries (shared via
REM VCPKG_BINARY_SOURCES, same as the build + package steps).
REM Say up front how much vcpkg work this build implies (x64-linux counterpart of the check
REM in the .m/.c scripts). Advisory: a full re-churn IS correct after a baseline bump or a
REM gcc upgrade in this distro. CHOICE has a 30s timeout defaulting to Yes so Build.bat
REM stays unattended. Exit 2 means the query itself could not run (vcpkg not bootstrapped
REM for Linux yet, e.g. a fresh checkout) -- that must not block the build that bootstraps it.
echo --- checking how many vcpkg ports this build would rebuild ---
wsl bash -c "export VCPKG_BINARY_SOURCES='clear;files,%geodms_wsldir%/vc_archives,readwrite' && export VCPKG_DOWNLOADS='%geodms_wsldir%/vc_downloads' && bash %geodms_wsldir%/tools/vcpkg-drift-check.sh %geodms_wsldir% x64-linux"
if errorlevel 1 if not errorlevel 2 (
    choice /C YN /T 30 /D Y /M "Continue with this build"
    if errorlevel 2 goto :build_failed
)

echo --- configuring linux-x64-release (skipped if already configured) ---
wsl bash -c "export VCPKG_BINARY_SOURCES='clear;files,%geodms_wsldir%/vc_archives,readwrite' && export VCPKG_DOWNLOADS='%geodms_wsldir%/vc_downloads' && cd %geodms_wsldir% && if [ ! -f build/linux-x64-release/CMakeCache.txt ]; then cmake --preset linux-x64-release; fi"
if errorlevel 1 goto :build_failed

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
REM VCPKG_DOWNLOADS shares the in-repo vc_downloads tool cache with .m and .c, exactly as
REM BuildSignAndCreateSetupCmake.bat does. Without it the WSL side resolves vcpkg's tools
REM (cmake, ninja, ...) from the distro-default root, and their VERSIONS are ABI inputs for
REM every port: the moment the two roots disagree, the whole x64-linux half of vc_archives
REM goes stale and this build silently turns into a multi-hour from-source rebuild. That is
REM not hypothetical -- it is exactly what the .c flavor cost on 2026-08-09 (cmake 3.31.10
REM vs 4.3.2, 47 minutes) before it got the same export. Linux tool dirs are named per
REM platform (cmake-4.3.2-linux), so they coexist with the Windows ones in vc_downloads.
wsl bash -c "export VCPKG_BINARY_SOURCES='clear;files,%geodms_wsldir%/vc_archives,readwrite' && export VCPKG_DOWNLOADS='%geodms_wsldir%/vc_downloads' && cd %geodms_wsldir% && rm -rf build/linux-x64-release/bin && cmake --build build/linux-x64-release --config Release"
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
