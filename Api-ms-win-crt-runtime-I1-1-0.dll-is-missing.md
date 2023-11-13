After installing and starting the GeoDMS, the following error may occur:

[[images/KB2999226.png]]

Cause: GeoDms uses an older version of GDAL, which uses Visual C Runtime version 1.0, which may not be installed on newer versions of Windows especially when few other programs are installed.

## solution

See <https://docs.microsoft.com/en-US/cpp/windows/latest-supported-vc-redist?view=msvc-160> for more info and redistributables for CRT x64.