@echo off
rem =====================================================================
rem Run the typed higher-order-function testcase suite (testcases\*.dms)
rem through GeoDmsRun and classify each case as pass/fail.
rem
rem Usage:  run_testcases.bat [path\to\GeoDmsRun.exe]
rem Default exe: ..\bin\Release\x64\GeoDmsRun.exe (relative to this script).
rem Exit code: 0 if every case matched its expected outcome, nonzero otherwise.
rem =====================================================================
setlocal
set "EXE=%~1"
if "%EXE%"=="" set "EXE=%~dp0..\bin\Release\x64\GeoDmsRun.exe"
if not exist "%EXE%" (
  echo ERROR: GeoDmsRun.exe not found at "%EXE%".
  echo Build the Release configuration first, or pass the exe path as the first argument.
  exit /b 2
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_testcases.ps1" -Exe "%EXE%"
exit /b %ERRORLEVEL%
