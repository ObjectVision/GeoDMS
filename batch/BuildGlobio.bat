@echo off
setlocal
cd /d "%~dp0.."

set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Debug"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo Usage: batch\BuildGlobio.bat [Debug^|Release]
    exit /b 2
)

if not defined GLOBIO_ENV_ROOT (
    echo GLOBIO_ENV_ROOT is not set. Point it at the environment locked by vcpkg-globio\environment.yml.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-vcpkg-manifests.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-globio-environment.ps1 -GlobioRoot "%GLOBIO_ENV_ROOT%"
if errorlevel 1 exit /b 1

set "PYTHON39_ROOT=%GLOBIO_ENV_ROOT%"
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" all22.sln -t:build -p:Configuration=%CONFIG% -p:Platform=x64 -p:GeoDmsGlobio=true
exit /b %ERRORLEVEL%
