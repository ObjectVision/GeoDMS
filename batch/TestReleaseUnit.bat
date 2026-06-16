echo on
cls

REM Re-root to the repo root (this script now lives in <root>\batch) so ..\tst\batch resolves.
cd /d "%~dp0.."
set geodms_rootdir=%cd%

REM msbuild all22.sln -t:build -p:Configuration=Release -p:Platform=x64

cd ..\tst\batch
Call unit.bat R64 off
cd %geodms_rootdir%
