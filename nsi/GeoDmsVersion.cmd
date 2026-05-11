@REM Single source of truth for the GeoDMS release version number.
@REM Update DMS_VERSION_PATCH (or MAJOR/MINOR for a larger bump) here ONLY;
@REM the three BuildSignAndCreateSetup{,Cmake,Linux}.bat scripts at the
@REM repo root `call` this file to pick up the same MAJOR.MINOR.PATCH.
@REM The .bat caller is responsible for setting its own GeoDmsFlavor
@REM (m / c / l) and for composing GeoDmsVersion=%MAJOR%.%MINOR%.%PATCH%
@REM after this call.

set DMS_VERSION_MAJOR=20
set DMS_VERSION_MINOR=0
set DMS_VERSION_PATCH=0
