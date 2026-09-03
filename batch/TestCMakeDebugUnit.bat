echo on
cls

REM Re-root to the repo root (this script now lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

REM run_unit_suite.bat verifies the build exists, that unit.bat really started (both
REM otherwise fail silently) and that the new aggregate lists no FAILED line. See its header.
call "%~dp0run_unit_suite.bat" CD64 on build\windows-x64-debug\bin
if errorlevel 2 (
  echo *** UNIT SUITE FAILED - see the aggregate named further up ***
  exit /b 1
)
if errorlevel 1 (
  echo *** UNIT SUITE DID NOT RUN - see the message further up ***
  exit /b 1
)
exit /b 0
