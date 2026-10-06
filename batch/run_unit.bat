@echo off
REM The tst unit suite alone, against one build of this tree: no source checks, no testcases
REM battery, no XML round trip and no shipped-content test, which the Test*Unit.bat launchers add.
REM
REM   run_unit.bat           the CMake Debug build, build\windows-x64-debug\bin (as before)
REM   run_unit.bat <sel>     another build of this tree, by its dev-tree selector:
REM                          R64  bin\Release\x64                D64  bin\Debug\x64
REM                          CR64 build\windows-x64-release\bin  CD64 build\windows-x64-debug\bin
REM                          GR64 bin_GLOBIO\Release\x64         GD64 bin_GLOBIO\Debug\x64
REM
REM It goes through batch\run_unit_suite.bat, as the launchers do: that proves the suite ran and
REM grades its aggregate (exit 0 passed, 1 did not run, 2 a failing test). Run it in a console.
REM
REM Until 2026-10-06 (BAT-A17) this script set geodms_rootdir to the batch folder instead of the
REM repo root, so the CD64 build it looked for did not exist, and it called tst's
REM unit_flagged.bat directly with S1 in the flavour slot of the calling convention of 2026-05-12.

setlocal
cd /d "%~dp0.."
set "geodms_rootdir=%cd%"

set "SEL=%~1"
if "%SEL%"=="" set "SEL=CD64"
set "BINDIR="
if /I "%SEL%"=="R64"  (
  set "BINDIR=bin\Release\x64"
  set "FLAVOUR=off"
)
if /I "%SEL%"=="D64"  (
  set "BINDIR=bin\Debug\x64"
  set "FLAVOUR=on"
)
if /I "%SEL%"=="CR64" (
  set "BINDIR=build\windows-x64-release\bin"
  set "FLAVOUR=off"
)
if /I "%SEL%"=="CD64" (
  set "BINDIR=build\windows-x64-debug\bin"
  set "FLAVOUR=on"
)
if /I "%SEL%"=="GR64" (
  set "BINDIR=bin_GLOBIO\Release\x64"
  set "FLAVOUR=g"
)
if /I "%SEL%"=="GD64" (
  set "BINDIR=bin_GLOBIO\Debug\x64"
  set "FLAVOUR=g"
)
if not defined BINDIR (
  echo *** run_unit.bat: unknown selector "%SEL%"; use R64, D64, CR64, CD64, GR64 or GD64 ***
  endlocal & exit /b 1
)

call "%~dp0run_unit_suite.bat" %SEL% %FLAVOUR% %BINDIR%
endlocal & exit /b %ERRORLEVEL%
