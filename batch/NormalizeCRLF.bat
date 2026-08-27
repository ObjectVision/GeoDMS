@echo off
REM ===========================================================================
REM  One-time working-tree line-ending repair for a Windows clone made before
REM  the "* text=auto" normalisation (GeoDMS 20.18.0).
REM
REM  Every text blob in this repository is stored with LF and converted to CRLF
REM  on checkout. A clone predating that commit keeps whatever its files already
REM  contained, including the 45 files that had drifted into a mix of CRLF and
REM  LF inside one file -- git could not repair those by itself, because it
REM  refuses to normalise a file whose index version already contains CR.
REM
REM  The repair is cosmetic: git cleans the working copy on `git add`, so those
REM  files already commit as clean LF. What this fixes is the working tree, so
REM  that what you edit matches what a fresh clone hands out.
REM
REM  Only files that actually differ from a checkout are rewritten, which keeps
REM  the follow-up rebuild small. The tree must be clean; the script refuses to
REM  run over uncommitted work.
REM
REM  Usage:
REM    batch\NormalizeCRLF.bat           list, ask, then repair
REM    batch\NormalizeCRLF.bat /scan     report only, write nothing
REM    batch\NormalizeCRLF.bat /y        repair without asking
REM ===========================================================================
setlocal
cd /d "%~dp0.."

set "PSARGS="
if /I "%~1"=="/scan" set "PSARGS=-Scan"
if /I "%~1"=="/y"    set "PSARGS=-Force"
if not "%~1"=="" if not defined PSARGS (
    echo Unknown option "%~1".
    echo Usage: batch\NormalizeCRLF.bat [/scan^|/y]
    exit /b 2
)

powershell -NoProfile -ExecutionPolicy Bypass -File tools\normalize-eol.ps1 %PSARGS%
exit /b %ERRORLEVEL%
