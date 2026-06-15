@echo off
set "geodms_rootdir=%~dp0"
if "%geodms_rootdir:~-1%"=="\" set "geodms_rootdir=%geodms_rootdir:~0,-1%"
cd /d C:\dev\tst\batch
call "%CD%\unit_flagged.bat" CD64 S1 S2 S3
