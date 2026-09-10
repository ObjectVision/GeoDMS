echo on

REM Debug counterpart of TestGlobioReleaseUnit.bat (issue #1231): the tst unit suite and
REM the testcases battery against the dev tree bin_GLOBIO\Debug\x64, as built by
REM batch\BuildGlobio.bat Debug. No shipped-content test and no installed-version form,
REM since setups are Release only. TestGlobioReleaseUnit.bat explains the g flavour argument.

REM Re-root to the repo root (this script lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

set UNIT_FAILED=0
call "%~dp0run_unit_suite.bat" GD64 g bin_GLOBIO\Debug\x64
if errorlevel 2 (set UNIT_FAILED=2) else if errorlevel 1 set UNIT_FAILED=1

REM Typed-function testcases battery (testcases\*.dms): positives must exit 0,
REM _neg/defcheck configs must exit nonzero, a Debug assert (exit 3) always fails.
set TC_FAILED=0
Call "%geodms_rootdir%\testcases\run_testcases.bat" "%geodms_rootdir%\bin_GLOBIO\Debug\x64\GeoDmsRun.exe"
if errorlevel 1 set TC_FAILED=1

REM The XML round-trip battery (#1261, testcases\run_xml_roundtrip.bat) is NOT called here, only from
REM the Release launchers. It runs @dumpconfig, and a Debug @dumpconfig stops on a lock-ceiling
REM assertion before it writes anything: RangeProp<T>::GetRawValueAsSharedStr declares the
REM IndexedString ceiling (rtc\dll\src\tic\UnitClassReg.h, and the same shape in the base
REM PropDef<>::GetRawValueAsSharedStr) and then calls GetRawValue, whose RangeProp<T>::GetValue
REM takes an interest on the unit and so enters ItemRegister, ord 74, under ord 90. Every
REM configuration with a ranged unit hits it. That predates the round trip; put the call back once
REM the ceiling is sorted out.

echo.
if "%TC_FAILED%"=="1" (
  echo *** TESTCASES BATTERY FAILED - see table above and testcases\_out\ logs ***
) else (
  echo TESTCASES BATTERY PASSED
)
if "%UNIT_FAILED%"=="1" echo *** UNIT SUITE DID NOT RUN - see the message further up ***
if "%UNIT_FAILED%"=="2" echo *** UNIT SUITE FAILED - see the aggregate named further up ***

if not "%UNIT_FAILED%"=="0" exit /b 1
if "%TC_FAILED%"=="1" exit /b 1
exit /b 0
