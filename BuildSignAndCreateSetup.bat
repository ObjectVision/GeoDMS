echo on
cls

set DMS_VERSION_MAJOR=8
set DMS_VERSION_MINOR=6
set DMS_VERSION_PATCH=1

set geodms_rootdir=%cd%

set GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%
cd ..
md tst
cd tst
git pull
cd %geodms_rootdir%

CHOICE /M "Update RtcGeneratedVersion.h?"
if ErrorLevel 2 goto :startBuild
echo #define DMS_VERSION_MAJOR %DMS_VERSION_MAJOR% > rtc/dll/src/RtcGeneratedVersion.h
echo #define DMS_VERSION_MINOR %DMS_VERSION_MINOR% >> rtc/dll/src/RtcGeneratedVersion.h
echo #define DMS_VERSION_PATCH %DMS_VERSION_PATCH% >> rtc/dll/src/RtcGeneratedVersion.h


:startBuild
CHOICE /M "Build GeoDms?"
if ErrorLevel 2 goto afterBuild

set MS_VERB=rebuild
CHOICE /M "Rebuild all Release x64 (requires start from Tools..Cmd line)?"
if ErrorLevel 2 set MS_VERB=build
del /s /q "bin\debug\x64"
del /s /q "bin\release\x64"

msbuild all22.sln -t:%MS_VERB% -p:Configuration=Debug -p:Platform=x64
msbuild all22.sln -t:%MS_VERB% -p:Configuration=Release -p:Platform=x64
REM 2nd build to complete copy actions.
set MS_VERB=build
msbuild all22.sln -t:%MS_VERB% -p:Configuration=Debug -p:Platform=x64
msbuild all22.sln -t:%MS_VERB% -p:Configuration=Release -p:Platform=x64

ROBOCOPY exe\gui\misc bin\debug\x64\misc /v /fft /E /njh /njs
IF %ErrorLevel%==1 EXIT 0
IF %ErrorLevel%==8 ECHO SEVERAL FILES DID NOT COPY

ROBOCOPY exe\gui\misc bin\release\x64\misc /v /fft /E /njh /njs
IF %ErrorLevel%==1 EXIT 0
IF %ErrorLevel%==8 ECHO SEVERAL FILES DID NOT COPY

:afterBuild
REM svn update ..\tst
CHOICE /M "bin\release\x64 compiled and ..\tst updated? Ready to execute the x64-fast-debug-test?"
if ErrorLevel 2 goto :afterD64FastTest

cd ..\tst\batch
Call sync_src.bat
Call fast.bat D64 off
cd %geodms_rootdir%
echo on

:afterD64FastTest
CHOICE /M "Ready to execute the x64-unit-debug-test?"
if ErrorLevel 2 goto :afterD64UnitTest

cd ..\tst\batch
Call unit.bat D64 off
cd %geodms_rootdir%
echo on
:afterD64UnitTest

set SIGNTOOL=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\x64\signtool.exe
REM CHOICE /M  "Sign binaries? No running instances that lock them?"
REM if ErrorLevel 2 goto :afterSign1
REM "%SIGNTOOL%" sign /debug /a /n "Object Vision" /fd SHA256 /tr http://timestamp.globalsign.com/tsa/r6advanced1 /td SHA256 "bin\release\x64\GeoDmsRun.exe" "bin\release\x64\GeoDmsGui.exe" "bin\release\x64\GeoDmsCaller.exe" "bin\release\x64\rtc.dll" "bin\release\x64\tic.dll" "bin\release\x64\sym.dll" "bin\release\x64\stx.dll" "bin\release\x64\geo.dll" "bin\release\x64\stg.dll" "bin\release\x64\stgimpl.dll" "bin\release\x64\clc1.dll" "bin\release\x64\clc2.dll" "bin\release\x64\Shv.dll"
REM :afterSign1

CHOICE /M  "Run setup creation?"
if ErrorLevel 2 goto :afterNSIS

cd nsi
"C:\Program Files (x86)\NSIS\makensis.exe" DmsSetupScriptX64.nsi
cd ..
:afterNSIS

CHOICE /M  "NSIS OK (more than 39Mb, proj.db) and Sign Setup?"
if ErrorLevel 2 goto :afterSign2
"%SIGNTOOL%" sign /debug /a /n "Object Vision" /fd SHA256 /tr http://timestamp.globalsign.com/tsa/r6advanced1 /td SHA256 "distr\GeoDms%GeoDmsVersion%-Setup-x64.exe"
CHOICE /M  "Signing OK? Ready to run installation?"
if ErrorLevel 2 exit /B
goto startInstallation

:afterSign2
CHOICE /M  "Ready to run installation (without signing)?"
if ErrorLevel 2 exit /B

:startInstallation
del /s /q "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%\gdaldata"
del /s /q "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%\proj4data"
del /s /q "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%"
rmdir "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%\gdaldata"
rmdir "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%\proj4data"
rmdir "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%"
if exist "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%" pause

"distr\GeoDms%GeoDmsVersion%-Setup-x64.exe"

del filelist%GeoDmsVersion%.txt
FORFILES /P "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%" /S /C "cmd /c echo @relpath" >> filelist%GeoDmsVersion%.txt

fc filelist8031.txt filelist%GeoDmsVersion%.txt
if %ErrorLevel% == 1 pause


cd ..\tst\batch
REM Call fast.bat %GeoDmsVersion% off
Call unit.bat %GeoDmsVersion% off
cd %geodms_rootdir%
echo on

CHOICE /M  "Install test OK ? Ready to copy to OVSRV05?"
if ErrorLevel 2 exit /B

copy "distr\GeoDms%GeoDmsVersion%-Setup-x64.exe" "\\ovsrv05\SourceData\distr"

pause "Klaar ?"