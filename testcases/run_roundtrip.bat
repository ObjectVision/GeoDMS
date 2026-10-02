@echo off
rem =====================================================================
rem Round-trip completeness check for the DMS config-source serializer.
rem For every POSITIVE testcase config: dump it back to DMS syntax
rem (@dumpconfig), reload the DUMPED .dms, and require the item still
rem computes (exit 0). A passing reload means the dumped representation
rem captured everything relevant (the dumped IntegrityChecks re-verify).
rem
rem Usage:  run_roundtrip.bat [path\to\GeoDmsRun.exe] [-MaxCommitGB <GB>] [-TimeoutSec <s>]
rem Default exe: ..\bin\Release\x64\GeoDmsRun.exe (relative to this script); pass ""
rem to keep the default and still give options. Every run is held to a commit limit
rem (default 8 GB) and a wall-clock limit (default 300 s); see run_roundtrip.ps1.
rem Exit code: 0 if every positive round-trips, nonzero otherwise.
rem =====================================================================
setlocal
set "EXE=%~1"
if "%EXE%"=="" set "EXE=%~dp0..\bin\Release\x64\GeoDmsRun.exe"
if not exist "%EXE%" (
  echo ERROR: GeoDmsRun.exe not found at "%EXE%".
  echo Build the Release configuration first, or pass the exe path as the first argument.
  exit /b 2
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_roundtrip.ps1" -Exe "%EXE%" %2 %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%
