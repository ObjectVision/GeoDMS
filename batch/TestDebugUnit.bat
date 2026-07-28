echo on
cls

REM Re-root to the repo root (this script now lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

REM msbuild all22.sln -t:build -p:Configuration=Debug -p:Platform=x64

cd ..\tst\batch
Call unit.bat D64 on
cd %geodms_rootdir%

REM Typed-function testcases battery (testcases\*.dms): positives must exit 0,
REM _neg/defcheck configs must exit nonzero, a Debug assert (exit 3) always fails.
Call "%geodms_rootdir%\testcases\run_testcases.bat" "%geodms_rootdir%\bin\Debug\x64\GeoDmsRun.exe"
if errorlevel 1 (
  echo *** TESTCASES BATTERY FAILED - see table above and testcases\_out\ logs ***
) else (
  echo TESTCASES BATTERY PASSED
)
