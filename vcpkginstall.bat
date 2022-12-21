echo off
echo "Did this start from Visual Studio 2022 Command Prompt for x64 ? No->Ctrl-Break"
echo "CurrDir should now be in the geodms folder ! No ? - >Ctrl-Break"
echo "Don't forget to install dev/tst, Intel IPP, and NSIS >= 3.08 too !"
pause
cd ..

git clone https://github.com/Microsoft/vcpkg

cd vcpkg
git pull
call bootstrap-vcpkg.bat
vcpkg integrate install

set VCPKG_DEFAULT_TRIPLET=x64-windows

vcpkg install boost boost-locale ms-gsl openssl openssl-windows sqlite3[rtree] gdal
echo "Base packages installed ? Continue to install GeoDmsGui packages ?"
pause

vcpkg install imgui[docking-experimental] glfw3 opengl stb glew
echo "GeoDmsGui packages installed ? Continue to install yasm and cgal package ?
pause

vcpkg install yasm-tool cgal
