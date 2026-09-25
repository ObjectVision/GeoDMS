@echo off
rem =====================================================================
rem Offline test of the .dms content the installer SHIPS, before it is packaged
rem (GeoDMS-Test #24). Runs against bin\<Config>\x64\ or any other output folder,
rem i.e. against the copies CopyResources / DeployResources put there and NSIS
rem packages -- not against the source tree beside them. That is the point: it is
rem what lands on a user's machine.
rem
rem The installer ships examples\*.dms, library\**\*.dms and the examples\testcases
rem battery. Three steps, each reported separately at the end:
rem
rem   1. the shipped copy of the battery is current: every testcases\*.dms, the item
rem      map and the runners are in <bin>\examples\testcases and not older than the
rem      source. A stale copy would run a battery that is not the one that ships.
rem
rem   2. every shipped .dms is reached from a battery case: from the shipped
rem      shipped_*.dms cases every #include is followed, and the files reached must
rem      cover every examples\*.dms and library\**\*.dms that ships. That is what
rem      keeps the set of shipped and the set of tested files equal: a new shipped
rem      file that no case includes fails here, with its name. The cases in
rem      testcases\shipped_*.dms include from %exeDir% and check the units, the RD
rem      domains, the WMTS tile matrix, the grid domains and their cell areas, the
rem      CBS year table and its instantiation, two grid2poly fixtures, and
rem      examples\function.dms and examples\convex_hull.dms with their own
rem      /checks; grid_to_polygon.dms is loaded and its includes resolved, since
rem      its data is a download.
rem
rem   3. the shipped battery, run through its own run_testcases.bat from the output
rem      folder, exactly as a user would from <install>\examples\testcases.
rem
rem Usage:  TestShippedDms.bat [bin\Release\x64]
rem Exit code: 0 when every step passes. Offline; about a minute on a Release build.
rem
rem batch\BuildSignAndCreateSetup{,Cmake,Globio}.bat run this right before NSIS; the
rem Test*Unit.bat launchers reach it through TestShippedContent.bat, which adds the
rem real-data step. Its bash twin, TestShippedDms.sh, guards the Linux setup.
rem =====================================================================
setlocal

cd /d "%~dp0.."
set "GEODMS_ROOT=%cd%"

set "BINDIR=%~1"
if "%BINDIR%"=="" set "BINDIR=%GEODMS_ROOT%\bin\Release\x64"
set "EXE=%BINDIR%\GeoDmsRun.exe"

if not exist "%EXE%" (
  echo *** GeoDmsRun.exe not found in "%BINDIR%" - build first, or pass the bin folder ***
  exit /b 2
)
if not exist "%BINDIR%\examples\testcases\run_testcases.bat" (
  echo *** "%BINDIR%\examples\testcases\run_testcases.bat" is missing - the resource copy step did not run ***
  exit /b 2
)
if not exist "%BINDIR%\library" (
  echo *** "%BINDIR%\library" is missing - the resource copy step did not run ***
  exit /b 2
)

echo.
echo === Shipped .dms content, offline (GeoDMS-Test #24) ===
echo bin: %BINDIR%
echo.

rem --- 1 and 2: the copy is current, and every shipped .dms is reached -----
echo --- 1/3 and 2/3: the shipped battery is current, and reaches every shipped .dms ---
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0TestShippedDms.ps1" -Bin "%BINDIR%" -Source "%GEODMS_ROOT%\testcases"
set "COV_RC=%ERRORLEVEL%"

rem --- 3: the shipped battery, from the shipped copy -----------------------
echo.
echo --- 3/3: shipped examples\testcases battery ---
call "%BINDIR%\examples\testcases\run_testcases.bat" "%EXE%"
set "BAT_RC=%ERRORLEVEL%"

echo.
echo === Shipped .dms content, offline: results for %BINDIR% ===
set "ANY_FAILED=0"
if "%COV_RC%"=="0" (echo battery copy current and every shipped .dms reached: PASSED) else (echo *** BATTERY COPY STALE OR A SHIPPED .DMS UNREACHED - see the lines above *** & set "ANY_FAILED=1")
if "%BAT_RC%"=="0" (echo shipped testcases battery PASSED) else (echo *** SHIPPED TESTCASES BATTERY FAILED ^(exit %BAT_RC%^) - see the table above *** & set "ANY_FAILED=1")

if "%ANY_FAILED%"=="1" exit /b 1
exit /b 0
