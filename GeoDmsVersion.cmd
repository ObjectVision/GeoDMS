@REM Single source of truth for the GeoDMS release version number.
@REM Update DMS_VERSION_PATCH (or MAJOR/MINOR for a larger bump) here ONLY;
@REM the three batch\BuildSignAndCreateSetup{,Cmake,Linux}.bat scripts (and
@REM Build.bat) `call` this file to pick up the same MAJOR.MINOR.PATCH.
@REM The .bat caller is responsible for setting its own GeoDmsFlavor
@REM (m / c / l) and for composing GeoDmsVersion=%MAJOR%.%MINOR%.%PATCH%
@REM after this call.

set DMS_VERSION_MAJOR=20
set DMS_VERSION_MINOR=3
set DMS_VERSION_PATCH=0

REM Refresh the generated version + buildstamp headers -- but only ONCE per
REM build session. Build.bat calls this file up-front; the guard below then
REM makes the subsequent per-flavor scripts (m/c/l) reuse the SAME header. Two
REM reasons:
REM   1) all three flavors report an identical build date/time;
REM   2) the header is written before any flavor builds, so it stays OLDER than
REM      the build outputs. A later F5 in MSVC therefore does NOT see
REM      RtcGeneratedVersion.h as changed, does NOT re-run msbuild, and does NOT
REM      emit fresh .pdb files that would no longer match the installed binaries
REM      (which makes debugging the installed version harder).
REM A standalone flavor run (GEODMS_VERSION_HEADER_DONE unset) still refreshes
REM the header itself, so its binaries report the current version. Because the
REM header is regenerated every session it stays newer than the PREVIOUS build's
REM DmRtc, so the per-flavor build-gate (relink check) still holds.
if not defined GEODMS_VERSION_HEADER_DONE (
	echo #define DMS_VERSION_MAJOR %DMS_VERSION_MAJOR% > "rtc/dll/src/RtcGeneratedVersion.h"
	echo #define DMS_VERSION_MINOR %DMS_VERSION_MINOR% >> "rtc/dll/src/RtcGeneratedVersion.h"
	echo #define DMS_VERSION_PATCH %DMS_VERSION_PATCH% >> "rtc/dll/src/RtcGeneratedVersion.h"
	echo #define DMS_BUILD_DATE "%DATE%" > "rtc/dll/src/buildstamp.h"
	echo #define DMS_BUILD_TIME "%TIME%" >> "rtc/dll/src/buildstamp.h"
	set GEODMS_VERSION_HEADER_DONE=1
)
