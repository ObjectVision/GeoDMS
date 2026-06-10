echo on
cls

REM Version comes from nsi\GeoDmsVersion.cmd (shared with the cmake + linux
REM sister scripts). Bump the patch number there, not here.
call GeoDmsVersion.cmd

REM Flavor suffix appended to install dir + setup filename. Sister scripts:
REM   BuildSignAndCreateSetupCmake.bat (c)  /  BuildSignAndCreateSetupLinux.bat (l)
set GeoDmsFlavor=m

set geodms_rootdir=%cd%

set GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%

cd ..
md tst
cd tst
git pull
cd %geodms_rootdir%

REM Mark script start so the post-build staleness guard can verify msbuild
REM actually produced a fresh binary. msbuild exits 0 on no-op IsUpToDate
REM caches; binaries don't carry FileVersion metadata, so mtime is the
REM only reliable signal. -5s fudge to absorb clock skew.
for /f "delims=" %%T in ('powershell -NoProfile -Command "[DateTime]::Now.AddSeconds(-5).ToString('o')"') do set BUILD_GATE_TIME=%%T

REM Refuse to build only if a GeoDmsRun/GeoDmsGuiQt is running FROM the build output
REM (%geodms_rootdir%\bin\Release\x64) -- a held handle on its Dm*.dll silently turns
REM msbuild's link step into a skip. A process from an INSTALLED build (e.g. full.py
REM driving GeoDms<ver>.m\GeoDmsRun.exe) does NOT lock this output, so it is ignored.
REM Path is derived from %geodms_rootdir%, so this works wherever the working copy lives.
powershell -NoProfile -Command "$b=(Resolve-Path -LiteralPath '%geodms_rootdir%\bin\Release\x64' -EA SilentlyContinue); $hit=0; if($b){$p=($b.Path.TrimEnd('\')+'\').ToLower(); foreach($pr in (Get-Process GeoDmsRun,GeoDmsGuiQt -EA SilentlyContinue)){ if($pr.Path -and $pr.Path.ToLower().StartsWith($p)){$hit=1} } }; exit $hit"
if errorlevel 1 (
    echo *** ABORT: a GeoDmsRun/GeoDmsGuiQt is running from bin\Release\x64 - it locks the build's Dm*.dll. Close it. ***
    goto :build_failed
)

REM Always do an incremental build. If intermediates become funky, clean
REM from the MSVC IDE or `rmdir /s /q bin build` from the shell — no need
REM for a CHOICE inside this script.
:retryBuild
msbuild all22.sln -t:build -p:Configuration=Release -p:Platform=x64

CHOICE /M  "Built OK? Ready to create installation?"
if ErrorLevel 2 goto retryBuild

REM msbuild can exit 0 even when the IsUpToDate cache decided nothing needed
REM rebuilding -- which silently ships stale binaries. Binaries carry no
REM FileVersion, so use mtime: GeoDmsRun.exe must be at least as new as
REM the script start, otherwise the build was a no-op. (msbuild on the
REM handwritten .vcxproj solution auto-relinks dependents when a referenced
REM Dm*.dll changes, so checking the leaf binary is safe here -- unlike
REM the cmake-generated .vcxproj files, which need a DmRtc.dll check.)
powershell -NoProfile -Command "if ((Get-Item 'bin\Release\x64\GeoDmsRun.exe').LastWriteTime -ge [DateTime]'%BUILD_GATE_TIME%') { exit 0 } else { exit 1 }"
if errorlevel 1 (
    echo *** ABORT: bin\Release\x64\GeoDmsRun.exe was not rebuilt - msbuild was a no-op against stale binaries. ***
    goto :build_failed
)

:setupCreation

REM CHOICE /M  "Run setup creation %GeoDmsVersion%?"
REM if ErrorLevel 2 goto :afterNSIS

mkdir distr
cd nsi
"C:\Program Files (x86)\NSIS\makensis.exe" DmsSetupScriptX64.nsi
cd ..

CHOICE /M  "NSIS OK (more than  55Mb) and ready to sign Setup?"
if ErrorLevel 2 exit /B

:afterNSIS
set SIGNTOOL=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe
"%SIGNTOOL%" sign /debug /a /n "Object Vision" /fd SHA256 /tr http://timestamp.globalsign.com/tsa/r6advanced1 /td SHA256 "distr\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%-Setup-x64.exe"
CHOICE /M  "Signing OK? Ready to run installation?"
if ErrorLevel 2 goto afterNSIS

if exist "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%" CHOICE /M "Removed "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%" or accept testing with an overwritten folder ?"

"distr\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%-Setup-x64.exe" /S

del filelist%GeoDmsVersion%.%GeoDmsFlavor%.txt
FORFILES /P "C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%" /S /C "cmd /c echo @relpath" >> filelist%GeoDmsVersion%.%GeoDmsFlavor%.txt

cd ..\tst\batch
REM Pass the flavor separately so unit.bat -> unit_flagged.bat ->
REM SetGeoDMSPlatform.bat can compose the install dir as
REM GeoDms<ver>.<flavor> (e.g. GeoDms20.0.0.m). Without the flavor the
REM path becomes GeoDms<ver> which does not exist and every GeoDmsRun.exe
REM invocation reports a missing-file FAILED.
Call unit.bat %GeoDmsVersion% m off
cd %geodms_rootdir%
echo on


pause "Klaar ?"
exit /B 0

:build_failed
echo *** Build failed - NSIS, signing, install and unit tests skipped ***
exit /B 1