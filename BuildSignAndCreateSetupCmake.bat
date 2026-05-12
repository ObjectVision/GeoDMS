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
call nsi\GeoDmsVersion.cmd
set GeoDmsFlavor=c

set geodms_rootdir=%cd%
set GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%

REM Pull tst on the parallel checkout for the post-install regression run.
cd ..
md tst 2>nul
cd tst
git pull
cd %geodms_rootdir%

REM Refresh generated headers (matches the msbuild script). Idempotent.
echo #define DMS_VERSION_MAJOR %DMS_VERSION_MAJOR% > "rtc\dll\src\RtcGeneratedVersion.h"
echo #define DMS_VERSION_MINOR %DMS_VERSION_MINOR% >> "rtc\dll\src\RtcGeneratedVersion.h"
echo #define DMS_VERSION_PATCH %DMS_VERSION_PATCH% >> "rtc\dll\src\RtcGeneratedVersion.h"

echo #define DMS_BUILD_DATE "%DATE%" > "rtc\dll\src\buildstamp.h"
echo #define DMS_BUILD_TIME "%TIME%" >> "rtc\dll\src\buildstamp.h"

set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set BUILD_DIR=build\windows-x64-release

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo --- configuring %BUILD_DIR% ---
    %CMAKE% --preset windows-x64-release ^
        -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
        -DVCPKG_INSTALLED_DIR="%geodms_rootdir%/vcpkg_installed" ^
        -DCMAKE_PREFIX_PATH="C:/Qt/6.9.0/msvc2022_64"
    if errorlevel 1 goto :build_failed
)

echo --- building cmake-Release ---
%CMAKE% --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :build_failed

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
