echo on
cls

set geodms_rootdir=%cd%

REM msbuild all22.sln -t:build -p:Configuration=Debug -p:Platform=x64

cd ..\tst\batch
Call fast.bat D64 off
cd %geodms_rootdir%
echo on

pause "Klaar ?"