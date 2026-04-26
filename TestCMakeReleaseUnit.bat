echo on
cls

set geodms_rootdir=%cd%

cd ..\tst\batch
Call unit.bat CR64 off
cd %geodms_rootdir%
echo on

pause "Klaar ?"
