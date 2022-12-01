pause "Did this start from Visual Studio 2022 Command Prompt for x64 ? No->Ctrl-Break"
pause "CurrDir should now be in the geodms folder ! No ? - >Ctrl-Break"
cd ..

git clone https://github.com/Microsoft/vcpkg

cd vcpkg
git pull
bootstrap-vcpkg.bat
vcpkg integrate install

set VCPKG_DEFAULT_TRIPLET=x64-windows

vcpkg install boost boost-locale ms-gsl openssl openssl-windows sqlite3[rtree] gdal --triplet x64-windows
pause Base packages installed ? Continue to install GeoDmsGui packages ?

 vcpkg install imgui[docking-experimental] glfw3 opengl stb glew --triplet x64-windows
pause GeoDmsGui packages installed ? Continue to install cgal packages ?

vcpkg install yasm-tool cgal --triplet x64-windows
