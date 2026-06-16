echo on
cls

REM Re-root to the repo root (this script now lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

cd ..\tst\batch
Call unit.bat CR64 off
cd %geodms_rootdir%
