echo on
cls

REM Re-root to the repo root (this script now lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

REM run_unit_suite.bat verifies the build exists and that unit.bat really started;
REM both of those otherwise fail silently. See its header.
call "%~dp0run_unit_suite.bat" CR64 off build\windows-x64-release\bin
if errorlevel 1 (
  echo *** UNIT SUITE DID NOT RUN - see the message further up ***
  exit /b 1
)

REM Shipped-content release test (issue #1031), against the cmake output folder.
REM Running it for BOTH flavours is the point: it is what would have caught the
REM .c/.m library divergence, where the cmake bin carried an extra, older
REM Grid2Poly_ipoint.dms + grid_to_vector.dms pair that the msbuild bin did not.
call "%~dp0TestShippedContent.bat" "%geodms_rootdir%\build\windows-x64-release\bin"
if errorlevel 1 (
  echo *** SHIPPED CONTENT RELEASE TEST FAILED - see scratch\grid_to_polygon_release.log ***
  exit /b 1
)
exit /b 0
