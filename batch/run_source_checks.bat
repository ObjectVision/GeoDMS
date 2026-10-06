@echo off
REM Syntactic checks over the source tree: about forty seconds, no build, no data.
REM
REM   tools\check-lock-ceilings.ps1     every call from a function that declares a lock ceiling
REM                                     (DMS_ENTERS) to one that declares another, directly or through
REM                                     undeclared functions, is admitted by Allow (doc\deadlocks.md 3.9)
REM   tools\check-lock-across-sink.ps1  no token-registry usage (...Lock()) spans a format sink or a
REM                                     lock-taking callee (doc\deadlocks.md B6, R1)
REM   tools\check-ptr-discipline.ps1    no second control block for a tree-owned object (PLN-A06)
REM   tools\check-format-args.ps1       every format call with a literal format has one argument per
REM                                     placeholder (std::vformat drops a surplus argument silently)
REM
REM Until 2026-10-06 the first three ran only inside analyze.bat, which forces a full /analyze
REM rebuild, and no launcher, setup script or skill called analyze.bat. So nothing ran them: the
REM first run of this gate found a ceiling that the #1284 commit of that day had declared over a
REM whole function, where it was meant for the last section only. The Test*Unit.bat launchers, the
REM four setup scripts and analyze.bat call this script now.
REM
REM Exit code: 0 every check passes; 1 a check reported a site, or could not run (its own output
REM above says which).

setlocal
cd /d "%~dp0.."

set "SC_FAILED="
echo.
echo === source checks ===
call :run check-lock-ceilings
call :run check-lock-across-sink
call :run check-ptr-discipline
call :run check-format-args
echo.
if defined SC_FAILED (
  echo *** SOURCE CHECKS FAILED:%SC_FAILED% ***
  endlocal & exit /b 1
)
echo SOURCE CHECKS PASSED
endlocal & exit /b 0

:run
echo --- tools\%1.ps1 ---
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\%1.ps1"
if errorlevel 1 set "SC_FAILED=%SC_FAILED% %1"
exit /b 0
