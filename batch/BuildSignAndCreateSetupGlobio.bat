@echo off
setlocal
cls

REM Build, package, sign, install, and test the GLOBIO-compatible G flavour.
REM It uses the normal all22.sln with /p:GeoDmsGlobio=true, which redirects
REM outputs and dependencies to bin_GLOBIO/obj_GLOBIO/vcpkg_installed_GLOBIO.
cd /d "%~dp0.."
set "geodms_rootdir=%CD%"

call "%geodms_rootdir%\GeoDmsVersion.cmd"
if not defined DMS_VERSION_MAJOR goto :version_failed
set "GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%"
set "GeoDmsFlavor=g"
set "OUTPUT_DIR=%geodms_rootdir%\bin_GLOBIO\Release\x64"
set "INSTALLER=%geodms_rootdir%\distr\GeoDms%GeoDmsVersion%.g-Setup-x64.exe"
set "INSTALL_DIR=%ProgramFiles%\ObjectVision\GeoDms%GeoDmsVersion%.g"

if not defined GLOBIO_ENV_ROOT (
    echo *** ABORT: GLOBIO_ENV_ROOT is not set. Point it at the environment created from vcpkg-globio\environment.yml. ***
    goto :build_failed
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\verify-vcpkg-manifests.ps1"
if errorlevel 1 goto :build_failed
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\verify-globio-environment.ps1" -GlobioRoot "%GLOBIO_ENV_ROOT%"
if errorlevel 1 goto :build_failed

REM The G binding must compile and test against this environment's CPython 3.9,
REM not an unrelated system PYTHON39_ROOT.
set "PYTHON39_ROOT=%GLOBIO_ENV_ROOT%"
set "VCPKG_BINARY_SOURCES=clear;files,%geodms_rootdir%\vc_archives,readwrite"
set "VCPKG_DOWNLOADS=%geodms_rootdir%\vc_downloads"

cd ..
if not exist tst md tst
cd tst
git pull
if errorlevel 1 goto :build_failed
cd /d "%geodms_rootdir%"

for /f "delims=" %%T in ('powershell -NoProfile -Command "[DateTime]::Now.AddSeconds(-5).ToString('o')"') do set "BUILD_GATE_TIME=%%T"

powershell -NoProfile -Command "$b=(Resolve-Path -LiteralPath '%OUTPUT_DIR%' -EA SilentlyContinue); $hit=0; if($b){$p=($b.Path.TrimEnd('\')+'\').ToLower(); foreach($pr in (Get-Process GeoDmsRun,GeoDmsGuiQt -EA SilentlyContinue)){if($pr.Path -and $pr.Path.ToLower().StartsWith($p)){$hit=1}}}; exit $hit"
if errorlevel 1 (
    echo *** ABORT: GeoDmsRun/GeoDmsGuiQt is running from %OUTPUT_DIR%. Close it first. ***
    goto :build_failed
)

powershell -NoProfile -Command "$ps=@(Get-Process MSBuild -EA SilentlyContinue); if($ps.Count -eq 0){exit 0}; $c=@(Get-CimInstance Win32_Process -Filter (($ps.Id|%%{'ProcessId='+$_}) -join ' OR ') -EA SilentlyContinue); if(@($c|?{$_.CommandLine -notmatch '[-/]nodemode:'}).Count){exit 1}; exit 0"
if errorlevel 1 (
    echo *** WARNING: another MSBuild driver is running; concurrent builds can tear the G output. ***
    choice /M "Continue only if it targets another repository"
    if errorlevel 2 goto :build_failed
)

if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"

powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\vcpkg-drift-check.ps1" -Triplet x64-windows-v145 -ManifestRoot "%geodms_rootdir%\vcpkg-globio" -InstallRoot "%geodms_rootdir%\vcpkg_installed_GLOBIO"
if errorlevel 1 if not errorlevel 2 (
    choice /C YN /T 30 /D Y /M "Continue with the G build"
    if errorlevel 2 goto :build_failed
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\patch-qtdeploy-targets.ps1"

:retry_build
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" all22.sln -t:build -p:Configuration=Release -p:Platform=x64 -p:GeoDmsGlobio=true
if errorlevel 1 (
    choice /M "G build failed. Retry"
    if not errorlevel 2 goto :retry_build
    goto :build_failed
)

powershell -NoProfile -Command "if((Get-Item '%OUTPUT_DIR%\GeoDmsRun.exe').LastWriteTime -ge [DateTime]'%BUILD_GATE_TIME%'){exit 0}else{exit 1}"
if errorlevel 1 (
    echo *** ABORT: GeoDmsRun.exe was not freshly rebuilt. ***
    goto :build_failed
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\deploy-globio-runtime.ps1" -OutputDir "%OUTPUT_DIR%" -GlobioRoot "%GLOBIO_ENV_ROOT%"
if errorlevel 1 goto :build_failed
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\verify-python-binding.ps1" -OutputDir "%OUTPUT_DIR%" -VersionsFile "%geodms_rootdir%\python\PythonVersionsGlobio.txt"
if errorlevel 1 goto :build_failed
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\test-python-bindings.ps1" -OutputDir "%OUTPUT_DIR%" -VersionsFile "%geodms_rootdir%\python\PythonVersionsGlobio.txt"
if errorlevel 1 goto :build_failed
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\test-globio-coexistence.ps1" -OutputDir "%OUTPUT_DIR%" -GlobioRoot "%GLOBIO_ENV_ROOT%"
if errorlevel 1 goto :build_failed

if not exist distr md distr
cd nsi
"C:\Program Files (x86)\NSIS\makensis.exe" DmsSetupScriptX64-globio.nsi
if errorlevel 1 (
    cd ..
    goto :build_failed
)
cd ..
if not exist "%INSTALLER%" goto :setup_missing

choice /M "G setup created. Ready to sign"
if errorlevel 2 goto :build_failed
set "SIGNTOOL=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
"%SIGNTOOL%" sign /debug /a /n "Object Vision" /fd SHA256 /tr http://timestamp.globalsign.com/tsa/r6advanced1 /td SHA256 "%INSTALLER%"
if errorlevel 1 goto :build_failed

if exist "%INSTALL_DIR%\uninstaller.exe" "%INSTALL_DIR%\uninstaller.exe" /S _?=%INSTALL_DIR%
"%INSTALLER%" /S
if errorlevel 1 goto :unit_failed

cd ..\tst\batch
set "SAVED_PATH=%PATH%"
set "PATH=%CD%;%PATH%"
call "%CD%\unit.bat" %GeoDmsVersion% g off
set "PATH=%SAVED_PATH%"
cd /d "%geodms_rootdir%"
powershell -NoProfile -Command "$d='%LocalDataDir%\GeoDMSTestResults\unit'; $f=Get-ChildItem (Join-Path $d 'v%GeoDmsVersion%.g_*.txt') -EA SilentlyContinue|Sort-Object LastWriteTime -Descending|Select-Object -First 1; if(-not $f -or (Select-String -Path $f.FullName -Pattern 'FAILED' -Quiet)){exit 1}"
if errorlevel 1 goto :unit_failed

echo === DONE: GeoDms%GeoDmsVersion%.g built, signed, installed, and tested ===
endlocal
exit /b 0

:unit_failed
echo *** G unit tests failed; removing the installed build and setup. ***
if exist "%INSTALL_DIR%\uninstaller.exe" "%INSTALL_DIR%\uninstaller.exe" /S _?=%INSTALL_DIR%
if exist "%INSTALL_DIR%" rmdir /s /q "%INSTALL_DIR%"
if exist "%INSTALLER%" del /q "%INSTALLER%"
endlocal
exit /b 1

:version_failed
echo *** ABORT: GeoDmsVersion.cmd did not provide a version. ***
goto :build_failed

:setup_missing
echo *** ABORT: NSIS produced no %INSTALLER%. ***

:build_failed
echo *** G build failed; packaging/install tests did not complete. ***
endlocal
exit /b 1
