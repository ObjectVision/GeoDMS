@REM Exports the GeoDMS release version to the batch build scripts.
@REM
@REM The version itself is NOT set here -- it lives in
@REM   rtc\dll\src\RtcVersionNumbers.h
@REM which is tracked, hand-edited and read by the C++ code directly. Bump it
@REM there; this file only parses it back out so that Build.bat, the four
@REM batch\BuildSignAndCreateSetup{,Cmake,Globio,Linux}.bat scripts and (via
@REM %GeoDmsVersion%) the NSIS setup scripts all see the same numbers as the
@REM compiled binaries. CMakeLists.txt parses the same header independently.
@REM
@REM Before, this file WROTE a generated RtcGeneratedVersion.h, which made the
@REM version a build artefact of the batch scripts: any build that skipped them
@REM (msbuild all22.sln, F5 in MSVC, CMake, CI) picked up a stale version, and a
@REM fresh clone had no header at all.
@REM
@REM The .bat caller is responsible for setting its own GeoDmsFlavor (m / c / g / l)
@REM and for composing GeoDmsVersion=%MAJOR%.%MINOR%.%PATCH% after this call.

set "DMS_VERSION_HEADER=%~dp0rtc\dll\src\RtcVersionNumbers.h"
set "DMS_BUILDSTAMP_HEADER=%~dp0rtc\dll\src\buildstamp.h"

if not exist "%DMS_VERSION_HEADER%" (
	echo *** GeoDmsVersion.cmd: %DMS_VERSION_HEADER% not found. ***
	exit /B 1
)

set "DMS_VERSION_MAJOR="
set "DMS_VERSION_MINOR="
set "DMS_VERSION_PATCH="

for /f "tokens=3" %%v in ('findstr /b /c:"#define DMS_VERSION_MAJOR " "%DMS_VERSION_HEADER%"') do set "DMS_VERSION_MAJOR=%%v"
for /f "tokens=3" %%v in ('findstr /b /c:"#define DMS_VERSION_MINOR " "%DMS_VERSION_HEADER%"') do set "DMS_VERSION_MINOR=%%v"
for /f "tokens=3" %%v in ('findstr /b /c:"#define DMS_VERSION_PATCH " "%DMS_VERSION_HEADER%"') do set "DMS_VERSION_PATCH=%%v"

if not defined DMS_VERSION_MAJOR goto :parse_failed
if not defined DMS_VERSION_MINOR goto :parse_failed
if not defined DMS_VERSION_PATCH goto :parse_failed

REM Refresh the buildstamp header -- but only ONCE per build session. Build.bat
REM calls this file up-front; the guard below then makes the subsequent
REM per-flavor scripts (m/c/g/l) reuse the SAME header. Two reasons:
REM   1) all four flavours report an identical build date/time;
REM   2) the header is written before any flavor builds, so it stays OLDER than
REM      the build outputs. A later F5 in MSVC therefore does NOT see
REM      buildstamp.h as changed, does NOT re-run msbuild, and does NOT emit
REM      fresh .pdb files that would no longer match the installed binaries
REM      (which makes debugging the installed version harder).
REM A standalone flavor run (GEODMS_VERSION_HEADER_DONE unset) still refreshes
REM the stamp itself. Note this concern no longer applies to the version numbers:
REM RtcVersionNumbers.h only changes when a human bumps it.
REM Builds that never run a .bat get a buildstamp.h from the generate-if-missing
REM hooks in Directory.Build.targets (msbuild) and CMakeLists.txt (cmake); those
REM write ISO 8601, and :write_buildstamp below matches them.
if not defined GEODMS_VERSION_HEADER_DONE call :write_buildstamp

exit /B 0

REM ---------------------------------------------------------------------------
REM Write buildstamp.h with an ISO 8601 date and time (yyyy-MM-dd / HH:mm:ss).
REM
REM NOT %DATE% / %TIME%: those are locale-dependent (dd-MM-yyyy vs MM/dd/yyyy,
REM some locales prefix the weekday) and %TIME% adds centiseconds and pads the
REM hour with a space before 10:00. That made the stamp differ per machine and
REM disagree with the msbuild/cmake fallbacks. PowerShell formats it culture-
REM invariantly; it is already a build dependency (tools\ensure-vcpkg.ps1).
REM
REM A subroutine, not an inline block: %VAR% set inside a parenthesized block is
REM expanded at parse time, and delayed expansion is not an option here because
REM setlocal would discard the DMS_VERSION_* exports this file exists to provide.
REM ---------------------------------------------------------------------------
:write_buildstamp
set "_dms_stamp_date="
set "_dms_stamp_time="
for /f "usebackq delims=" %%d in (`powershell -NoProfile -Command "(Get-Date).ToString('yyyy-MM-dd')"`) do set "_dms_stamp_date=%%d"
for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "(Get-Date).ToString('HH:mm:ss')"`) do set "_dms_stamp_time=%%t"
if not defined _dms_stamp_date goto :stamp_failed
if not defined _dms_stamp_time goto :stamp_failed
REM No space before the redirection: cmd would echo it into the file, and the
REM msbuild/cmake fallbacks do not emit one. Byte-identical output means
REM alternating between build paths does not dirty the header and force a relink.
echo #define DMS_BUILD_DATE "%_dms_stamp_date%"> "%DMS_BUILDSTAMP_HEADER%"
echo #define DMS_BUILD_TIME "%_dms_stamp_time%">> "%DMS_BUILDSTAMP_HEADER%"
set "_dms_stamp_date="
set "_dms_stamp_time="
set GEODMS_VERSION_HEADER_DONE=1
goto :eof

:stamp_failed
echo *** GeoDmsVersion.cmd: could not format the build timestamp via powershell. ***
exit /B 1

:parse_failed
echo *** GeoDmsVersion.cmd: could not parse DMS_VERSION_MAJOR/MINOR/PATCH from ***
echo *** %DMS_VERSION_HEADER% -- expected lines like "#define DMS_VERSION_MAJOR 20". ***
exit /B 1
