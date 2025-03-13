echo on
cls

set DMS_VERSION_MAJOR=17
set DMS_VERSION_MINOR=0
set DMS_VERSION_PATCH=2

set geodms_rootdir=%cd%

set GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%

cd ..
md tst
cd tst
git pull
cd %geodms_rootdir%

CHOICE /M "Update RtcGeneratedVersion.h?"
if ErrorLevel 2 goto :startBuild

echo #define DMS_VERSION_MAJOR %DMS_VERSION_MAJOR% > "rtc/dll/src/RtcGeneratedVersion.h"
echo #define DMS_VERSION_MINOR %DMS_VERSION_MINOR% >> "rtc/dll/src/RtcGeneratedVersion.h"
echo #define DMS_VERSION_PATCH %DMS_VERSION_PATCH% >> "rtc/dll/src/RtcGeneratedVersion.h"

echo #define DMS_BUILD_DATE "%DATE%" > "rtc/dll/src/buildstamp.h"
echo #define DMS_BUILD_TIME "%TIME%" >> "rtc/dll/src/buildstamp.h"

:startBuild
REM CHOICE /M "Build GeoDms %GeoDmsVersion%?"
REM if ErrorLevel 2 goto :setupCreation

set MS_VERB=build
CHOICE /M "Rebuild all Release x64 (requires start from Tools..Cmd line)?"
if ErrorLevel 2 goto :retryBuild

set MS_VERB=rebuild
del /s /q "bin\release\x64"

:retryBuild
REM msbuild all22.sln -t:%MS_VERB% -p:Configuration=Debug -p:Platform=x64
msbuild all22.sln -t:%MS_VERB% -p:Configuration=Release -p:Platform=x64
REM 2nd build to complete copy actions.
REM set MS_VERB=build
REM msbuild all22.sln -t:%MS_VERB% -p:Configuration=Debug -p:Platform=x64
REM msbuild all22.sln -t:%MS_VERB% -p:Configuration=Release -p:Platform=x64

CHOICE /M  "Built OK? Ready to create installation?"
set MS_VERB=build
if ErrorLevel 2 goto retryBuild

:setupCreation

REM CHOICE /M  "Run setup creation %GeoDmsVersion%?"
REM if ErrorLevel 2 goto :afterNSIS

cd nsi
"C:\Program Files (x86)\NSIS\makensis.exe" DmsSetupScriptX64.nsi
cd ..

CHOICE /M  "NSIS OK (more than 140Mb) and ready to sign Setup?"
if ErrorLevel 2 exit /B

:afterNSIS
set SIGNTOOL=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe
"%SIGNTOOL%" sign /debug /a /n "Object Vision" /fd SHA256 /tr http://timestamp.globalsign.com/tsa/r6advanced1 /td SHA256 "distr\GeoDms%GeoDmsVersion%-Setup-x64.exe"
CHOICE /M  "Signing OK? Ready to run installation?"
if ErrorLevel 2 goto afterNSIS

if exist "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%" CHOICE /M "Removed "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%" or accept testing with an overwritten folder ?"

"distr\GeoDms%GeoDmsVersion%-Setup-x64.exe" /S

del filelist%GeoDmsVersion%.txt
FORFILES /P "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%" /S /C "cmd /c echo @relpath" >> filelist%GeoDmsVersion%.txt

cd ..\tst\batch
Call unit.bat %GeoDmsVersion% off
cd %geodms_rootdir%
echo on


pause "Klaar ?"