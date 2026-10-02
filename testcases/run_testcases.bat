@echo off
rem =====================================================================
rem Run the typed higher-order-function testcase suite (testcases\*.dms)
rem through GeoDmsRun and classify each case as pass/fail.
rem
rem Usage:  run_testcases.bat [path\to\GeoDmsRun.exe] [-MaxCommitGB <GB>] [-TimeoutSec <s>]
rem Default exe: ..\bin\Release\x64\GeoDmsRun.exe (relative to this script); pass ""
rem to keep the default and still give options. Every case is held to a commit limit
rem (default 8 GB) and a wall-clock limit (default 300 s); see run_testcases.ps1.
rem Exit code: 0 if every case matched its expected outcome, nonzero otherwise.
rem =====================================================================
setlocal
set "EXE=%~1"
REM Default exe: repo layout (testcases\ beside bin\), else the INSTALLED layout
REM (this suite ships as <install>\examples\testcases, two levels below GeoDmsRun.exe).
if "%EXE%"=="" set "EXE=%~dp0..\bin\Release\x64\GeoDmsRun.exe"
if not exist "%EXE%" if exist "%~dp0..\..\GeoDmsRun.exe" set "EXE=%~dp0..\..\GeoDmsRun.exe"
if not exist "%EXE%" (
  echo ERROR: GeoDmsRun.exe not found at "%EXE%".
  echo Build the Release configuration first, or pass the exe path as the first argument.
  exit /b 2
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_testcases.ps1" -Exe "%EXE%" %2 %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%
