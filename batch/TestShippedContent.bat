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
rem   1. the shipped examples\testcases battery, run through its own
rem      run_testcases.bat from the output folder, exactly as a user would
rem      run it from <install>\examples\testcases;
rem
rem   2. releasetests\grid_to_polygon_release.dms, which drives the shipped
rem      examples\grid_to_polygon.dms over the real CBS buurt map.
rem
rem Before step 2 the CBS geopackage is renamed to <name>.bak (deleting an
rem older .bak first), so RegioIndelingen.dms has to download it again. That
rem download path is user-facing and silent when it breaks, so every release
rem should walk it. The original stays as .bak; nothing is destroyed.
rem
rem Usage:  TestShippedContent.bat [bin\Release\x64] [cbs-year] [source-data-dir]
rem   source-data-dir defaults to the SourceDataDir the engine itself would use;
rem   pass it to point the geopackage step at a scratch folder, which is how the
rem   rename logic gets tested without touching the real source data.
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

rem --- Resolve SourceDataDir exactly the way GetGeoDmsRegKey() does, or the rename
rem     below silently touches nothing and the download path is never walked.
rem
rem     Note the trap: RegistryHandleLocalMachineRO is NOT HKLM. Registry.cpp builds
rem     its path as HKEY_CURRENT_USER\Software\ObjectVision\<COMPUTERNAME>\GeoDMS --
rem     the per-machine settings live under HKCU. That key is consulted first, and a
rem     value of #DELETED# there means "unset" and does NOT fall through to
rem     Software\ObjectVision\DMS. Then comes that DMS key, then the C:\SourceData
rem     default. An explicit third argument wins over all of it.
set "SOURCEDATADIR=%~3"
if defined SOURCEDATADIR goto :have_sourcedatadir

set "SDD_LM="
for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\ObjectVision\%COMPUTERNAME%\GeoDMS" /v SourceDataDir 2^>nul ^| find "SourceDataDir"') do set "SDD_LM=%%b"
if defined SDD_LM (
  if /i not "%SDD_LM%"=="#DELETED#" set "SOURCEDATADIR=%SDD_LM%"
  goto :sourcedatadir_default
)
for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\ObjectVision\DMS" /v SourceDataDir 2^>nul ^| find "SourceDataDir"') do set "SOURCEDATADIR=%%b"

:sourcedatadir_default
if not defined SOURCEDATADIR set "SOURCEDATADIR=C:\SourceData"
:have_sourcedatadir

set "GPKG=%SOURCEDATADIR%\RegionalUnits\cbs\gebiedsindelingen%CBSYEAR%.gpkg"

echo.
echo === Shipped content release test ===
echo bin           : %BINDIR%
echo SourceDataDir : %SOURCEDATADIR%
echo CBS geopackage: %GPKG%
echo.

rem --- Step 1: the shipped testcases battery, from the shipped copy ---------
echo --- 1/2: shipped examples\testcases battery ---
set "TC_FAILED=0"
call "%BINDIR%\examples\testcases\run_testcases.bat" "%BINDIR%\GeoDmsRun.exe"
if errorlevel 1 set "TC_FAILED=1"

rem --- Step 2: force a fresh download, then run grid_to_polygon ------------
echo.
echo --- 2/2: examples\grid_to_polygon.dms over the real CBS buurt map ---
if exist "%GPKG%" (
  if exist "%GPKG%.bak" (
    echo Removing the previous backup "%GPKG%.bak"
    del /q "%GPKG%.bak"
    if exist "%GPKG%.bak" (
      echo *** could not delete "%GPKG%.bak" - is it open? ***
      exit /b 2
    )
  )
  echo Renaming "%GPKG%" to "%GPKG%.bak" so it has to be downloaded again
  ren "%GPKG%" "gebiedsindelingen%CBSYEAR%.gpkg.bak"
  if exist "%GPKG%" (
    echo *** could not rename "%GPKG%" - is it open? ***
    exit /b 2
  )
) else (
  echo No geopackage present; the run below downloads it for the first time.
)

rem The example is run from the OUTPUT folder and carries its own /checks, so this
rem is the same command the header of that file invites a user to type. It must be
rem the top-level container of its own config -- it resolves the buurt map as the
rem absolute path /CBS/Y2025/Buurt -- which is why nothing wraps it here.
set "G2P_LOG=%GEODMS_ROOT%\scratch\grid_to_polygon_release.log"
if not exist "%GEODMS_ROOT%\scratch" mkdir "%GEODMS_ROOT%\scratch"

set "G2P_FAILED=0"
"%BINDIR%\GeoDmsRun.exe" "/L%G2P_LOG%" "%BINDIR%\examples\grid_to_polygon.dms" /checks
if errorlevel 1 set "G2P_FAILED=1"

echo.
echo === Shipped content release test: results ===
if "%TC_FAILED%"=="1" (
  echo *** SHIPPED TESTCASES BATTERY FAILED ***
) else (
  echo shipped testcases battery PASSED
)
if "%G2P_FAILED%"=="1" (
  echo *** grid_to_polygon FAILED - see "%G2P_LOG%" ***
  echo     the measured counts and areas are on the 'grid_to_polygon:' line in that log
) else (
  echo grid_to_polygon PASSED
)
if exist "%GPKG%.bak" echo the previous geopackage is kept as "%GPKG%.bak"

if "%TC_FAILED%"=="1" exit /b 1
if "%G2P_FAILED%"=="1" exit /b 1
exit /b 0
