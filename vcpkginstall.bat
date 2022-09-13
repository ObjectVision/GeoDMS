pause "Did this start from Visual Studio 2022 Command Prompt for x64 ? No->Ctrl-Break"
pause "cd is now in geodms folder ? No->Ctrl-Break"
cd ..

git clone https://github.com/Microsoft/vcpkg

cd vcpkg
git pull
bootstrap-vcpkg.bat
vcpkg integrate install
vcpkg install boost --triplet x64-windows
vcpkg install openssl --triplet x64-windows
vcpkg install gdal --triplet x64-windows
vcpkg install yasm-tool:x64-windows
vcpkg install cgal:x64-windows
pause OK ?
