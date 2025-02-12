echo off
echo "Did this start from Visual Studio 2022 Command Prompt for x64? No? -> Ctrl-Break"
echo "CurrDir should now be in the geodms folder ! No ? - >Ctrl-Break"
echo "Don't forget to install dev/tst, Intel IPP, and NSIS >= 3.08 too !"
pause

set VCPKG_BINARY_SOURCES=clear;files,C:\dev\vc_archives,readwrite
set VCPKG_DOWNLOADS=c:\dev\vc_downloads

echo about to delete sub-folder vcpkg_installed
pause "No? -> Ctrl-Break"

del /s /q "vcpkg_installed"
vcpkg install

