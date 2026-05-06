echo on
cls

set geodms_rootdir=%cd%

cd ..\tst\batch
Call unit.bat CD64 on
cd %geodms_rootdir%
