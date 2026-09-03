echo on

REM Test launcher for the GLOBIO (.g) flavour, the counterpart of TestReleaseUnit.bat (.m)
REM and TestCMakeReleaseUnit.bat (.c); issue #1231.
REM
REM   TestGlobioReleaseUnit.bat            tests the dev tree bin_GLOBIO\Release\x64
REM   TestGlobioReleaseUnit.bat 20.19.1    tests the INSTALLED %ProgramFiles%\ObjectVision\GeoDms20.19.1.g
REM
REM The second form is what BuildSignAndCreateSetupGlobio.bat runs right after installing
REM the setup, and what re-tests a released .g without rebuilding or re-signing anything.
REM
REM Three steps, each against that one folder, each reported separately at the end:
REM   1. the tst unit suite through run_unit_suite.bat, which proves the suite really
REM      started and produced a NEW aggregate, and gates on a FAILED line in it;
REM   2. the typed-function testcases battery (testcases\run_testcases.bat), offline;
REM   3. TestShippedContent.bat: the shipped examples\testcases run from that folder, plus
REM      examples\grid_to_polygon.dms over the real CBS buurt map after renaming the
REM      geopackage, so the download path is walked on the GLOBIO GDAL stack as well.
REM No cls here, unlike the siblings: this script also runs inside the setup script, whose
REM console and teed log must keep the build output above it.

REM Re-root to the repo root (this script lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

set "G_VER=%~1"
if "%G_VER%"=="" (
  set "G_SELECTOR=GR64"
  set "G_BIN=%geodms_rootdir%\bin_GLOBIO\Release\x64"
) else (
  set "G_SELECTOR=%G_VER%"
  set "G_BIN=%ProgramFiles%\ObjectVision\GeoDms%G_VER%.g"
)
echo === GLOBIO release tests against %G_BIN% ===

REM The flavour argument must be g: unit_flagged.bat exports it as GeoDmsFlavor, which
REM Unit\CRS\cfg\reproject.dms and Unit\PythonTest.bat read to pick the GLOBIO GDAL/PROJ
REM expectations and the conda CPython. GR64 is the dev-tree selector that
REM tst\batch\generic\SetGeoDMSPlatform.bat maps to bin_GLOBIO\Release\x64.
set UNIT_FAILED=0
call "%~dp0run_unit_suite.bat" %G_SELECTOR% g "%G_BIN%"
if errorlevel 2 (set UNIT_FAILED=2) else if errorlevel 1 set UNIT_FAILED=1

REM Typed-function testcases battery (testcases\*.dms): positives must exit 0,
REM _neg/defcheck configs must exit nonzero, a Debug assert (exit 3) always fails.
set TC_FAILED=0
Call "%geodms_rootdir%\testcases\run_testcases.bat" "%G_BIN%\GeoDmsRun.exe"
if errorlevel 1 set TC_FAILED=1

REM Shipped-content release test (issue #1031). Goes to the internet and rasterises NL
REM at 25 m; see TestCMakeReleaseUnit.bat for why every Windows flavour runs it.
set SHIPPED_FAILED=0
call "%~dp0TestShippedContent.bat" "%G_BIN%"
if errorlevel 1 set SHIPPED_FAILED=1

echo.
echo === GLOBIO release tests: results for %G_BIN% ===
if "%UNIT_FAILED%"=="0" echo UNIT SUITE PASSED
if "%UNIT_FAILED%"=="1" echo *** UNIT SUITE DID NOT RUN - see the message further up ***
if "%UNIT_FAILED%"=="2" echo *** UNIT SUITE FAILED - see the aggregate named further up ***
if "%TC_FAILED%"=="1" (
  echo *** TESTCASES BATTERY FAILED - see table above and testcases\_out\ logs ***
) else (
  echo TESTCASES BATTERY PASSED
)
if "%SHIPPED_FAILED%"=="1" (
  echo *** SHIPPED CONTENT RELEASE TEST FAILED - see scratch\grid_to_polygon_release.log ***
) else (
  echo SHIPPED CONTENT RELEASE TEST PASSED
)

if not "%UNIT_FAILED%"=="0" exit /b 1
if "%TC_FAILED%"=="1" exit /b 1
if "%SHIPPED_FAILED%"=="1" exit /b 1
exit /b 0
