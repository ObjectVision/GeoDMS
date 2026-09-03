@echo off
REM Shared guard rail for the Test*Unit.bat launchers.
REM
REM   %1 = version selector passed to tst\batch\unit.bat: a dev-tree shortcut
REM        (R64 / D64 / CR64 / CD64 / GR64 / GD64) or an installed version such as 20.19.1
REM   %2 = flavour passed through to unit.bat (off / on for m and c, g for GLOBIO); it
REM        reaches the tests as GeoDmsFlavor and names the aggregate v<%1>.<%2>_<stamp>.txt
REM   %3 = folder holding GeoDmsRun.exe: relative to the repo root (bin\Release\x64) or
REM        absolute (an installed C:\Program Files\ObjectVision\GeoDms<ver>.<f>)
REM
REM Exit code: 0 the suite ran and its aggregate lists no FAILED line; 1 the suite did not
REM run at all (the message says why); 2 the suite ran and the aggregate lists FAILED.
REM
REM This exists because the unit suite has three failure modes that produce NO error:
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
REM
REM  3. unit.bat sets no errorlevel for a failing test; the verdict is only in the aggregate
REM     it writes. The setup scripts gate their install on a FAILED line in that file, and
REM     so does this script, so a launcher and a setup run report the same verdict. The
REM     before/after check that precedes the grep is what keeps a STALE aggregate from an
REM     earlier run from being graded as if this run had produced it.

setlocal

if "%geodms_rootdir%"=="" (
  echo *** UNIT SUITE NOT RUN: geodms_rootdir is not set -- call this from a Test*Unit.bat ***
  exit /b 1
)

if "%~3"=="" (
  echo *** UNIT SUITE NOT RUN: no build folder given as the third argument ***
  exit /b 1
)
REM A drive letter or a UNC prefix means an absolute folder (an installed build); anything
REM else is taken relative to the repo root.
set "UNITDIR=%~3"
if not "%UNITDIR:~1,1%"==":" if not "%UNITDIR:~0,2%"=="\\" set "UNITDIR=%geodms_rootdir%\%UNITDIR%"
set "UNITEXE=%UNITDIR%\GeoDmsRun.exe"
if not exist "%UNITEXE%" (
  echo *** UNIT SUITE NOT RUN: %UNITEXE% does not exist -- build that configuration first ***
  exit /b 1
)

for %%I in ("%geodms_rootdir%\..\tst\batch") do set "TSTBATCH=%%~fI"
if not exist "%TSTBATCH%\unit.bat" (
  echo *** UNIT SUITE NOT RUN: %TSTBATCH%\unit.bat not found -- is the tst tree checked out beside this repo? ***
  exit /b 1
)

REM Calling unit.bat by an explicit path below fixes only OUR hop: unit.bat in turn calls
REM unit_flagged.bat by bare name, and that file lives in the tst tree, which this repo has
REM no business editing. Putting the folder on PATH is what rescues that nested call, since
REM PATH is searched even when the current directory is not.
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

call "%TSTBATCH%\unit.bat" %1 %2
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

REM The same case-insensitive FAILED test as the setup scripts' install gate, so a launcher
REM and a setup run never disagree about the same aggregate.
findstr /I /C:"FAILED" "%UNITRESULTS%\%AGGAFTER%" >nul 2>&1
if not errorlevel 1 (
  echo.
  echo *** UNIT SUITE FAILED: %UNITRESULTS%\%AGGAFTER% lists ***
  findstr /I /C:"FAILED" "%UNITRESULTS%\%AGGAFTER%"
  exit /b 2
)

echo.
echo unit suite results: %UNITRESULTS%\%AGGAFTER% -- no FAILED line
echo   NB an EMPTY per-test .txt means that test PASSED; the aggregate lists failures only.
exit /b 0
