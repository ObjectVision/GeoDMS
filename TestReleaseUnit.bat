echo on
cls

set geodms_rootdir=%cd%

REM msbuild all22.sln -t:build -p:Configuration=Release -p:Platform=x64

cd ..\tst\batch
Call unit.bat R64 off
cd %geodms_rootdir%
echo on

pause "Klaar ?"