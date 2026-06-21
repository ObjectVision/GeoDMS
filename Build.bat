@echo off
cls

REM ============================================================================
REM  Build.bat -- build, sign, create + install GeoDMS setups for all flavors.
REM
REM  Runs the per-flavor setup scripts SEQUENTIALLY (never in parallel: each is a
REM  full build and concurrent builds exhaust RAM). Each sub-script is interactive
REM  (CHOICE prompts + the SafeNet signing PIN) -- drive them at the console.
REM
REM    m = msbuild (all22.sln)   c = cmake   l = linux (WSL)
REM
REM  The individual flavor scripts live in .\batch and can be run on their own:
REM    batch\BuildSignAndCreateSetup.bat       (m)
REM    batch\BuildSignAndCreateSetupCmake.bat  (c)
REM    batch\BuildSignAndCreateSetupLinux.bat  (l)
REM  The version is bumped in GeoDmsVersion.cmd (repo root).
REM ============================================================================

REM Generate the version + buildstamp headers ONCE here, before any flavor
REM builds, so all three (m/c/l) share a single header with one timestamp.
REM GeoDmsVersion.cmd sets GEODMS_VERSION_HEADER_DONE; the per-flavor scripts
REM still `call` it (for the version numbers) but the guard makes those calls
REM reuse this header instead of rewriting it -- which stops a later F5 in MSVC
REM from re-running msbuild and emitting .pdb files that no longer match the
REM installed binaries. (cd to repo root: the header paths are relative to it.)
cd /d "%~dp0"
set "GEODMS_VERSION_HEADER_DONE="
call GeoDmsVersion.cmd

echo === [m] msbuild setup ===
call "%~dp0batch\BuildSignAndCreateSetup.bat"
if errorlevel 1 goto :failed

echo === [c] cmake setup ===
call "%~dp0batch\BuildSignAndCreateSetupCmake.bat"
if errorlevel 1 goto :failed

echo === [l] linux setup ===
call "%~dp0batch\BuildSignAndCreateSetupLinux.bat"
if errorlevel 1 goto :failed

echo === All flavor setups (m, c, l) built, signed, installed ===
exit /B 0

:failed
echo *** A flavor setup FAILED - stopping (remaining flavors skipped). ***
exit /B 1
