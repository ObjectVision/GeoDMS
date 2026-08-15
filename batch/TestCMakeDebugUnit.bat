echo on
cls

REM Re-root to the repo root (this script now lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

REM run_unit_suite.bat verifies the build exists and that unit.bat really started;
REM both of those otherwise fail silently. See its header.
call "%~dp0run_unit_suite.bat" CD64 on build\windows-x64-debug\bin
if errorlevel 1 (
  echo *** UNIT SUITE DID NOT RUN - see the message further up ***
  exit /b 1
)
exit /b 0
