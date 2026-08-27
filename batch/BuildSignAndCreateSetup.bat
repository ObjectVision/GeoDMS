echo on
cls

REM This script now lives in <repo-root>\batch; re-root to the repo root so all the
REM relative paths below (all22.sln, nsi, distr, ..\tst\batch, %cd% capture) resolve.
cd /d "%~dp0.."

REM Version comes from GeoDmsVersion.cmd in the repo root (shared with the cmake +
REM linux sister scripts). Bump the patch number there, not here.
REM Called by an explicit path, not by bare name: cmd resolves a bare name through the
REM current directory, which a shell with NoDefaultCurrentDirectoryInExePath=1 refuses --
REM and "not recognized" does not stop this script, so %GeoDmsVersion% would silently
REM stay empty and every path built from it below would be wrong.
call "%~dp0..\GeoDmsVersion.cmd"
if not defined DMS_VERSION_MAJOR (
    echo *** ABORT: GeoDmsVersion.cmd did not run - version unknown, cannot name the setup ***
    goto :build_failed
)

REM Flavor suffix appended to install dir + setup filename. Sister scripts:
REM   BuildSignAndCreateSetupCmake.bat (c)  /  BuildSignAndCreateSetupLinux.bat (l)
set GeoDmsFlavor=m

set geodms_rootdir=%cd%

set GeoDmsVersion=%DMS_VERSION_MAJOR%.%DMS_VERSION_MINOR%.%DMS_VERSION_PATCH%

set GeoDmsPythonVersions=
set /p GeoDmsPythonVersions=<python\PythonVersions.txt
if not defined GeoDmsPythonVersions (
    echo *** ABORT: python\PythonVersions.txt is empty - Python ABI matrix unknown ***
    goto :build_failed
)

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

REM Refuse to proceed while ANOTHER MSBuild is running: two concurrent builds of
REM this repo interleave writes into bin\Release\x64 and the obj dirs, so NSIS
REM packages a TORN binary snapshot -- the resulting install fails every unit
REM test and the :unit_failed cleanup then removes it again (this exact
REM collision produced a broken 20.9.0.m install on 2026-07-20: a background
REM CLI build was still writing DLLs while this script wiped, rebuilt and
REM packaged the same output folder). If the running MSBuild provably targets
REM another repo, the CHOICE allows continuing.
REM
REM Counting MSBuild.exe processes is NOT that test. MSBuild keeps its worker
REM nodes alive for 15 minutes after a build finishes (/nodemode:1 /nodeReuse:true)
REM so the next build can reuse them, and an open Visual Studio keeps a set parked
REM for its design-time builds -- so a developer shell on a machine with VS open
REM tripped this guard with no build running at all. Warning about idle leftovers
REM trains the answer "yes, continue", which is the one reflex this guard must not
REM create.
REM So: an MSBuild WITHOUT /nodemode: is an entry-point driver and always counts;
REM a /nodemode: worker counts when it is demonstrably working, which is what a
REM build driven by devenv.exe -- which has no MSBuild entry point of its own --
REM looks like. A process whose command line cannot be read counts, as unknown.
REM
REM "Working" is two signals, because neither alone is reliable: a node running a
REM compile has CHILD processes (cl.exe, link.exe, rc.exe), and a parked one has
REM none; and a node burns CPU of its own for scheduling and logging. The child
REM test carries the case where the node sits waiting on a long single-threaded
REM link, the CPU test the case between task spawns. Both are cheap, and the
REM 750 ms sample is only paid when nothing else already decided.
powershell -NoProfile -Command "$ps = @(Get-Process MSBuild -EA SilentlyContinue); if ($ps.Count -eq 0) { exit 0 }; $cim = @(Get-CimInstance Win32_Process -Filter (($ps.Id | ForEach-Object { 'ProcessId=' + $_ }) -join ' OR ') -EA SilentlyContinue); if (@($cim | Where-Object { $_.CommandLine -notmatch '[-/]nodemode:' }).Count -gt 0) { exit 1 }; if (@(Get-CimInstance Win32_Process -Filter (($ps.Id | ForEach-Object { 'ParentProcessId=' + $_ }) -join ' OR ') -EA SilentlyContinue).Count -gt 0) { exit 1 }; $t0 = @{}; foreach ($p in $ps) { $t0[$p.Id] = $p.TotalProcessorTime }; Start-Sleep -Milliseconds 750; foreach ($p in (Get-Process -Id $ps.Id -EA SilentlyContinue)) { if ($t0.ContainsKey($p.Id) -and (($p.TotalProcessorTime - $t0[$p.Id]).TotalMilliseconds -gt 50)) { exit 1 } }; exit 0"
if errorlevel 1 (
    echo *** WARNING: an MSBuild.exe is currently running. A concurrent build of THIS repo tears the setup contents. ***
    CHOICE /M "Continue anyway (only safe if that build targets ANOTHER repo)?"
    if ErrorLevel 2 goto :build_failed
)

REM Wipe the build OUTPUT folder (the final DLLs/EXEs/import-libs that NSIS
REM packages) before building, so obsolete binaries from before a component
REM rename/removal cannot linger in bin\Release\x64 and ship in the installer.
REM Intermediate .obj/.tlog live under each project's IntDir (not here), so this
REM stays a fast incremental compile + full relink/redeploy, not a full rebuild.
REM (Runs after the lock check above so no held handle can block the rmdir.)
if exist "bin\Release\x64" rmdir /s /q "bin\Release\x64"

REM Mirror the vcpkg roots that DmsDef.props passes to msbuild as command-line options
REM (VcpkgAdditionalInstallOptions: --binarysource / --downloads-root) into the environment,
REM so the drift check below queries the SAME cache + tools the build will use. Querying the
REM machine-default roots would resolve a different cmake and report phantom drift. Same
REM values as the .c script sets, which is also what keeps the two flavors sharing one cache.
set VCPKG_BINARY_SOURCES=clear;files,%geodms_rootdir%\vc_archives,readwrite
set VCPKG_DOWNLOADS=%geodms_rootdir%\vc_downloads

REM Say up front how much vcpkg work this build implies. The .m flavor is the one that can
REM be BLIND to this: vcpkg.targets gates its manifest install on vcpkg.json's timestamp vs
REM a stamp file, so an ABI change that does not touch vcpkg.json (submodule bump, triplet
REM edit, MSVC re-pin) is skipped silently and .m links whatever happens to be installed.
REM Directory.Build.targets now widens that gate, and this check reports what it implies
REM before the build rather than after. Advisory: a full re-churn IS correct after a
REM baseline bump. CHOICE has a 30s timeout defaulting to Yes so Build.bat stays unattended.
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\vcpkg-drift-check.ps1" -Triplet x64-windows-v145
if errorlevel 1 if not errorlevel 2 (
    choice /C YN /T 30 /D Y /M "Continue with this build"
    if errorlevel 2 goto :eof
)

REM Re-apply the qtdeploy.targets MSB4023 fix if the Qt VS Tools extension has clobbered it
REM again. The extension re-extracts %LOCALAPPDATA%\QtMsBuild from its package not only on
REM updates but also on a plain Visual Studio start (observed 2026-08-09: package timestamp
REM 2026-04-14 restored, patch gone), and the unpatched file fails the GeoDmsGuiQt build at
REM the QtDeploy step on every "Skipping system library" line Qt 6.11's windeployqt prints.
REM Idempotent and cheap; exit 2 (no QtMsBuild on this machine) is fine and must not block.
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\patch-qtdeploy-targets.ps1"

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

REM Catch Python ABI/deployment drift before NSIS sees the output. This requires
REM all configured ABI-tagged modules and rejects any bundled Python runtime.
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\verify-python-binding.ps1" -OutputDir "%geodms_rootdir%\bin\Release\x64"
if errorlevel 1 goto :build_failed

REM Prove that each tagged module is selected and imports in its matching CPython.
powershell -NoProfile -ExecutionPolicy Bypass -File "%geodms_rootdir%\tools\test-python-bindings.ps1" -OutputDir "%geodms_rootdir%\bin\Release\x64"
if errorlevel 1 goto :build_failed

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
REM Explicit path for the same reason as GeoDmsVersion.cmd above; the PATH entry is
REM what rescues unit.bat's own bare-name call to unit_flagged.bat, which lives in the
REM tst tree. The result-file scan below already catches a suite that never ran.
set "SAVED_PATH=%PATH%"
set "PATH=%CD%;%PATH%"
Call "%CD%\unit.bat" %GeoDmsVersion% m off
set "PATH=%SAVED_PATH%"
cd %geodms_rootdir%
echo on

REM Harness unit-test failure. unit.bat sets no errorlevel, so scan the newest
REM result file (v<ver>.<flavor>_*.txt under %LocalDataDir%\GeoDMSTestResults\unit,
REM where unit.bat -> SetLocalDataDir.bat just set %LocalDataDir%) for a FAILED line.
REM On failure REMOVE the installed build and the setup file: a warning echo is too
REM easy to miss, and a build with failing unit tests must not stay installed/shippable.
powershell -NoProfile -Command "$d='%LocalDataDir%\GeoDMSTestResults\unit'; $f=Get-ChildItem (Join-Path $d 'v%GeoDmsVersion%.%GeoDmsFlavor%_*.txt') -EA SilentlyContinue | Sort-Object LastWriteTime -Desc | Select-Object -First 1; if(-not $f){Write-Host 'no unit result file found - treating as failure'; exit 1}; if(Select-String -Path $f.FullName -Pattern 'FAILED' -Quiet){Write-Host ('unit FAILED: '+$f.Name); exit 1} else {Write-Host ('unit OK: '+$f.Name); exit 0}"
if errorlevel 1 goto :unit_failed

pause "Klaar ?"
exit /B 0

:unit_failed
echo *** Unit tests FAILED - removing installed build and setup file so a broken build cannot ship ***
set "INSTALL_DIR=C:\Program Files\ObjectVision\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%"
if exist "%INSTALL_DIR%\uninstaller.exe" "%INSTALL_DIR%\uninstaller.exe" /S _?=%INSTALL_DIR%
if exist "%INSTALL_DIR%" rmdir /s /q "%INSTALL_DIR%"
del /q "distr\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%-Setup-x64.exe" 2>nul
echo Removed "%INSTALL_DIR%" and distr\GeoDms%GeoDmsVersion%.%GeoDmsFlavor%-Setup-x64.exe
exit /B 1

:build_failed
echo *** Build failed - NSIS, signing, install and unit tests skipped ***
exit /B 1
