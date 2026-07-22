@echo off
rem =====================================================================
rem Round-trip completeness check for the DMS config-source serializer.
rem For every POSITIVE testcase config: dump it back to DMS syntax
rem (@dumpconfig), reload the DUMPED .dms, and require the item still
rem computes (exit 0). A passing reload means the dumped representation
rem captured everything relevant (the dumped IntegrityChecks re-verify).
rem
rem Usage:  run_roundtrip.bat [path\to\GeoDmsRun.exe]
rem Default exe: ..\bin\Release\x64\GeoDmsRun.exe (relative to this script).
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
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_roundtrip.ps1" -Exe "%EXE%"
exit /b %ERRORLEVEL%
