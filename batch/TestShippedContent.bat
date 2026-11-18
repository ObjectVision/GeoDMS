@echo off
rem =====================================================================
rem Release test for the .dms content the installer SHIPS (issue #1031).
rem
rem Everything here runs against bin\<Config>\x64\, i.e. against the copies
rem CopyResources put there and NSIS packages -- not against the source tree
rem beside them. That is the point: it is what lands on a user's machine.
rem
rem Two steps:
rem
rem   1. TestShippedDms.bat, the offline part (GeoDMS-Test #24): the shipped
rem      examples\testcases battery run through its own run_testcases.bat from
rem      the output folder, exactly as a user would run it from
rem      <install>\examples\testcases, with the shipped_library.dms case that
rem      covers library\; examples\function.dms and examples\convex_hull.dms; a
rem      load of grid_to_polygon.dms; and a check that every shipped .dms is
rem      covered. The setup scripts run that part on its own before NSIS;
rem
rem   2. the shipped examples\grid_to_polygon.dms over the real CBS buurt map.
rem
rem Step 2 runs with a temporary SourceDataDir, an empty folder that this script
rem makes and removes again, given to GeoDmsRun as GEODMS_directories_SourceDataDir
rem (#1360). So RegioIndelingen.dms never finds the geopackage and always has to
rem download it with curl, and no existing SourceData is read or touched. That
rem download path is user-facing and silent when it breaks, so every release
rem should walk it. The batch\TestShippedContent.sh twin does the same in WSL.
rem
rem Usage:  TestShippedContent.bat [bin\Release\x64] [cbs-year] [temp-parent]
rem   temp-parent is the folder in which the temporary SourceDataDir is made,
rem   %TEMP% when not given. The geopackage takes some 170 MB there while the
rem   test runs.
rem Exit code: 0 when both steps pass.
rem
rem NOT part of testcases\run_testcases.bat: that battery must stay offline
rem and cheap. This one downloads a few hundred MB and rasterises the whole
rem of the Netherlands at 25 m.
rem =====================================================================
setlocal EnableDelayedExpansion

cd /d "%~dp0.."
set "GEODMS_ROOT=%cd%"

set "BINDIR=%~1"
if "%BINDIR%"=="" set "BINDIR=%GEODMS_ROOT%\bin\Release\x64"

set "CBSYEAR=%~2"
rem Must track the year examples\grid_to_polygon.dms asks for (/CBS/Y<year>/Buurt).
if "%CBSYEAR%"=="" set "CBSYEAR=2025"

set "TMP_PARENT=%~3"
if "%TMP_PARENT%"=="" set "TMP_PARENT=%TEMP%"

if not exist "%BINDIR%\GeoDmsRun.exe" (
  echo *** GeoDmsRun.exe not found in "%BINDIR%" - build first, or pass the bin folder ***
  exit /b 2
)
if not exist "%BINDIR%\examples\testcases\run_testcases.bat" (
  echo *** "%BINDIR%\examples\testcases" is missing - the CopyResources step did not run ***
  exit /b 2
)
if not exist "%BINDIR%\examples\grid_to_polygon.dms" (
  echo *** "%BINDIR%\examples\grid_to_polygon.dms" is missing - the CopyResources step did not run ***
  exit /b 2
)
if not exist "%TMP_PARENT%\" (
  echo *** the folder "%TMP_PARENT%" for the temporary SourceDataDir does not exist ***
  exit /b 2
)

echo.
echo === Shipped content release test ===
echo bin           : %BINDIR%
echo temporary SourceDataDir under: %TMP_PARENT%
echo.

rem --- Step 1: the offline part: shipped battery, library, examples --------
echo --- 1/2: shipped .dms content, offline (TestShippedDms.bat) ---
set "TC_FAILED=0"
call "%~dp0TestShippedDms.bat" "%BINDIR%"
if errorlevel 1 set "TC_FAILED=1"

rem --- Step 2: grid_to_polygon over a geopackage downloaded into an empty SourceDataDir
echo.
echo --- 2/2: examples\grid_to_polygon.dms over the real CBS buurt map ---
:pick_sdd
set "SDD=%TMP_PARENT%\geodms_shipped_content_%RANDOM%%RANDOM%"
if exist "%SDD%" goto :pick_sdd
mkdir "%SDD%"
if not exist "%SDD%\" (
  echo *** could not make the temporary SourceDataDir "%SDD%" ***
  exit /b 2
)
set "GPKG=%SDD%\RegionalUnits\cbs\gebiedsindelingen%CBSYEAR%.gpkg"
echo SourceDataDir : %SDD% ^(new and empty, removed after the run^)
echo CBS geopackage: %GPKG%

rem The example is run from the OUTPUT folder and carries its own /checks, so this
rem is the same command the header of that file invites a user to type. It must be
rem the top-level container of its own config -- it resolves the buurt map as the
rem absolute path /CBS/Y2025/Buurt -- which is why nothing wraps it here. GeoDMS
rem takes the environment variable before the SourceDataDir setting, and uses it as
rem given, so it gets forward slashes.
set "G2P_LOG=%GEODMS_ROOT%\scratch\grid_to_polygon_release.log"
if not exist "%GEODMS_ROOT%\scratch" mkdir "%GEODMS_ROOT%\scratch"
if exist "%G2P_LOG%" del /q "%G2P_LOG%"

set "G2P_FAILED=0"
set "GEODMS_directories_SourceDataDir=%SDD:\=/%"
"%BINDIR%\GeoDmsRun.exe" "/L%G2P_LOG%" "%BINDIR%\examples\grid_to_polygon.dms" /checks
if errorlevel 1 set "G2P_FAILED=1"
set "GEODMS_directories_SourceDataDir="

set "GPKG_INFO=was not downloaded (see the message of curl in the log; is there no network?)"
if exist "%GPKG%" for %%F in ("%GPKG%") do set "GPKG_INFO=was downloaded: %%~zF bytes"

rmdir /s /q "%SDD%"
set "SDD_INFO=removed"
if exist "%SDD%" set "SDD_INFO=NOT removed; delete it by hand"

echo.
echo === Shipped content release test: results ===
if "%TC_FAILED%"=="1" (
  echo *** SHIPPED .DMS CONTENT ^(offline part^) FAILED - see the TestShippedDms.bat table above ***
) else (
  echo shipped .dms content ^(offline part^) PASSED
)
if "%G2P_FAILED%"=="1" (
  echo *** grid_to_polygon FAILED - see "%G2P_LOG%" ***
  echo     the measured counts and areas are on the 'grid_to_polygon:' line in that log
) else (
  echo grid_to_polygon PASSED
)
echo the CBS geopackage !GPKG_INFO!
echo the temporary SourceDataDir "%SDD%" is !SDD_INFO!

if "%TC_FAILED%"=="1" exit /b 1
if "%G2P_FAILED%"=="1" exit /b 1
exit /b 0
