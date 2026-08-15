@echo off
REM Shared guard rail for the Test*Unit.bat launchers.
REM
REM   %1 = version selector passed to tst\batch\unit.bat (R64 / D64 / CR64 / CD64)
REM   %2 = second argument passed through to unit.bat      (on / off)
REM   %3 = build output folder under the repo root that holds GeoDmsRun.exe
REM
REM This exists because the unit suite has two failure modes that produce NO error:
REM
REM  1. unit.bat and its helpers call each other by bare name, which cmd resolves via
REM     the current directory. A shell with NoDefaultCurrentDirectoryInExePath=1 cannot
REM     find them, prints "'unit.bat' is not recognized", and CARRIES ON -- the caller
REM     still exits 0, so a launcher that also runs the testcases battery reports a
REM     clean pass while the entire unit suite was skipped. Putting the folder on PATH
REM     makes the bare-name calls resolve regardless of that setting; the result-file
REM     check below confirms the suite actually ran rather than trusting an exit code
REM     (a failed CALL of a missing .bat reports errorlevel 1, indistinguishable from
REM     an ordinary test failure, so the exit code cannot carry this).
REM
REM  2. tst\batch\generic\SetGeoDMSPlatform.bat defaults geodms_rootdir to C:\dev\GeoDMS
REM     when it is unset, so the suite happily runs a GeoDmsRun.exe that does not exist
REM     and every test "passes". Verifying the executable up front turns that into a
REM     stop, and echoing the path lets a reader confirm which build was measured.

setlocal

if "%geodms_rootdir%"=="" (
  echo *** UNIT SUITE NOT RUN: geodms_rootdir is not set -- call this from a Test*Unit.bat ***
  exit /b 1
)

set "UNITEXE=%geodms_rootdir%\%3\GeoDmsRun.exe"
if not exist "%UNITEXE%" (
  echo *** UNIT SUITE NOT RUN: %UNITEXE% does not exist -- build that configuration first ***
  exit /b 1
)

for %%I in ("%geodms_rootdir%\..\tst\batch") do set "TSTBATCH=%%~fI"
if not exist "%TSTBATCH%\unit.bat" (
  echo *** UNIT SUITE NOT RUN: %TSTBATCH%\unit.bat not found -- is the tst tree checked out beside this repo? ***
  exit /b 1
)

set "PATH=%TSTBATCH%;%PATH%"

echo.
echo === unit suite: %1 %2 against %UNITEXE% ===
echo.

cd /d "%TSTBATCH%"

REM unit_flagged.bat renames its result.txt to v<selector>[.<flavor>]_<stamp>.txt when it
REM finishes, so a NEW newest aggregate is proof the suite ran to the end. Remember the
REM current one first; LocalDataDir comes from the same helper the suite itself uses.
call generic\SetLocalDataDir.bat
set "UNITRESULTS=%LocalDataDir%\GeoDMSTestResults\unit"
set "AGGBEFORE="
for /f "delims=" %%F in ('dir /b /o-d "%UNITRESULTS%\v%1*.txt" 2^>nul') do if not defined AGGBEFORE set "AGGBEFORE=%%F"

call unit.bat %1 %2
set "UNITERR=%ERRORLEVEL%"

set "AGGAFTER="
for /f "delims=" %%F in ('dir /b /o-d "%UNITRESULTS%\v%1*.txt" 2^>nul') do if not defined AGGAFTER set "AGGAFTER=%%F"
cd /d "%geodms_rootdir%"

if "%AGGAFTER%"=="%AGGBEFORE%" (
  echo.
  echo *** UNIT SUITE DID NOT RUN: no new result file appeared in %UNITRESULTS% ***
  echo *** unit.bat returned errorlevel %UNITERR%; check that it started at all ***
  exit /b 1
)

echo.
echo unit suite results: %UNITRESULTS%\%AGGAFTER%
echo   NB an EMPTY per-test .txt means that test PASSED; the aggregate lists failures only.
exit /b 0
