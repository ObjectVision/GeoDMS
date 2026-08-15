echo on
cls

REM Re-root to the repo root (this script now lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

REM msbuild all22.sln -t:build -p:Configuration=Release -p:Platform=x64

REM run_unit_suite.bat verifies the build exists and that unit.bat really started;
REM both of those otherwise fail silently. See its header.
set UNIT_FAILED=0
call "%~dp0run_unit_suite.bat" R64 off bin\Release\x64
if errorlevel 1 set UNIT_FAILED=1

REM Typed-function testcases battery (testcases\*.dms): positives must exit 0,
REM _neg/defcheck configs must exit nonzero, a Debug assert (exit 3) always fails.
set TC_FAILED=0
Call "%geodms_rootdir%\testcases\run_testcases.bat" "%geodms_rootdir%\bin\Release\x64\GeoDmsRun.exe"
if errorlevel 1 set TC_FAILED=1

echo.
if "%TC_FAILED%"=="1" (
  echo *** TESTCASES BATTERY FAILED - see table above and testcases\_out\ logs ***
) else (
  echo TESTCASES BATTERY PASSED
)
if "%UNIT_FAILED%"=="1" echo *** UNIT SUITE DID NOT RUN - see the message further up ***

if "%UNIT_FAILED%"=="1" exit /b 1
if "%TC_FAILED%"=="1" exit /b 1
exit /b 0
