echo on
cls

REM Re-root to the repo root (this script now lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

REM msbuild all22.sln -t:build -p:Configuration=Debug -p:Platform=x64

REM run_unit_suite.bat verifies the build exists, that unit.bat really started (both
REM otherwise fail silently) and that the new aggregate lists no FAILED line. See its header.
set UNIT_FAILED=0
call "%~dp0run_unit_suite.bat" D64 on bin\Debug\x64
if errorlevel 2 (set UNIT_FAILED=2) else if errorlevel 1 set UNIT_FAILED=1

REM Typed-function testcases battery (testcases\*.dms): positives must exit 0,
REM _neg/defcheck configs must exit nonzero, a Debug assert (exit 3) always fails.
set TC_FAILED=0
Call "%geodms_rootdir%\testcases\run_testcases.bat" "%geodms_rootdir%\bin\Debug\x64\GeoDmsRun.exe"
if errorlevel 1 set TC_FAILED=1

REM XML round-trip battery (#1261): every testcases configuration dumped in DMS syntax and in
REM XML, the XML read back and dumped in DMS syntax again, and the two DMS dumps compared. A
REM difference is something the XML notation lost on the way out or on the way back in.
REM Offline; three GeoDmsRun runs per configuration, so about twice the battery above. Known
REM differences and their reason live in testcases\xml_roundtrip_known_diff.txt.
REM In a Debug build this is also the lock-ceiling check of the dump path: a raw property
REM accessor that takes an interest or prepares under the IndexedString ceiling stops here with
REM exit 3, which is how #1268 (RangeProp<T>::GetRawValue) was found and is kept fixed.
set RT_FAILED=0
Call "%geodms_rootdir%\testcases\run_xml_roundtrip.bat" "%geodms_rootdir%\bin\Debug\x64\GeoDmsRun.exe"
if errorlevel 1 set RT_FAILED=1

echo.
if "%TC_FAILED%"=="1" (
  echo *** TESTCASES BATTERY FAILED - see table above and testcases\_out\ logs ***
) else (
  echo TESTCASES BATTERY PASSED
)
if "%RT_FAILED%"=="1" (
  echo *** XML ROUNDTRIP BATTERY FAILED - see table above and testcases\_out_xml_roundtrip\ ***
) else (
  echo XML ROUNDTRIP BATTERY PASSED
)
if "%UNIT_FAILED%"=="1" echo *** UNIT SUITE DID NOT RUN - see the message further up ***
if "%UNIT_FAILED%"=="2" echo *** UNIT SUITE FAILED - see the aggregate named further up ***

if not "%UNIT_FAILED%"=="0" exit /b 1
if "%TC_FAILED%"=="1" exit /b 1
if "%RT_FAILED%"=="1" exit /b 1
exit /b 0
