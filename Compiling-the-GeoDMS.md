This page describes how to compile GeoDmsRun.exe from source on Windows. The process is bound to change through an ongoing effort to make the source code more accessible to the open source community. Compilation is performed using Microsoft Visual Studio (2022) which can be downloaded [here](https://visualstudio.microsoft.com/vs/community/). Make sure to also install the latest versions of additional components: C++ MFC and C++ ATL. This document is written as a chronological guide.

## Clone GeoDMS from Github

Clone GeoDMS from the [Github repository](https://github.com/ObjectVision/GeoDMS).

## setting up external dependencies

###### **vcpkg: openssl, gdal and boost**

Dependencies are easily installed using the [[open-source package manager vcpkg|https://vcpkg.io/en/index.html]]. Open a Developer Command Prompt, cd to the desired location for vcpkg and use the following commands to install and integrate vcpkg into Microsoft Visual Studio (2022):

<pre>
git clone https://github.com/Microsoft/vcpkg
cd vcpkg
bootstrap-vcpkg.bat
vcpkg integrate install
</pre>

The last command is optional, but recommended for user-wide integration.
TODO: add vcpkg paths

Now vcpkg is installed the following three commands will install necessary GeoDMS dependencies: openssl (https requests for wms background layers), gdal and boost.

In case multiple Microsoft Visual Studio versions are installed, you can open a Developer Command Prompt from the Microsoft Visual Studio (2022)
menu in Tools > Command line > Developer Command Prompt to ensure vcpkg is hooked to the right environment, and, assuming you build for x64:

<pre>
set VCPKG_DEFAULT_TRIPLET=x64-windows
</pre>

<pre>
vcpkg install boost boost-locale openssl-windows sqlite3[rtree] gdal ms-gsl --triplet x64-windows
</pre>

The vcpkg triplet you use may vary depending on the platform specifics.

Make sure to restart Microsoft Visual Studio (2022) for changes to take effect. To update further down the line use git pull from within the
vcpkg folder and ./vcpkg update or ./vcpkg upgrade. 

Known issues:

-   Error: Building package atlmfc:x64-windows failed with: BUILD_FAILED -> Solution: <https://github.com/microsoft/vcpkg/issues/4257>

#### Intel IPP

Download the Intel® oneAPI Base Toolkit [here](https://www.intel.com/content/www/us/en/developer/tools/oneapi/base-toolkit-download.html).
From setup select the latest Integrated Performance Primitives (IPP) library. Open the Microsoft Visual Studio (2022) project file located in
/geo/dll/GeoDLL.vcxproj and set the IPP paths on lines 49, 55 and 59.

#### GeoDMSGui

Install Qt version 6.5.1 or later.

## building the GeoDMS DLLs

Open the solution file "geodms/trunk/all22.sln" in Microsoft Visual Studio (2022). You see that GeoDMS consists of various projects from
which we will create .dll files. There are four build configurations: a Release and Debug version in both x64 and Win32. Use ctrl-shift-b to
build the whole solution, or alternatively follow these steps as roadmap to build GeoDmsRun.exe:

1. Rtc (Runtime Core Library)
2. Sym (configuration syntax parser and producer of an internal representation of a model and its calculation rules.)
3. Tic (Tree Item Classes, Data Items and Units and related services.)
4. Stx (configuration syntax parser and producer of an internal representation of a model and its calculation rules.)
5. Stg (Storage Managers, providing a generic interface to GDAL, Proj, and native Storage Implementations including .shp and .dbf the TIFF lib, and ODBC connections.)
6. Clc (implementation of common operators)
7. Geo (implementation of geometric and network operators, such as Dijkstra functions, raster convolution, polygon and arc operations)
8. Shv (Viewer Components (TableViewer, MapViewer) and related Carets, and Graphic Components.)
9. And finally: GeoDmsRun.exe, and GeoDmsGuiQt.exe

All build products will be placed in the bin folder depending on the chosen build configuration, for instance the "GeoDmsRun.exe" file may reside in the: geodms/bin/Release/x64 folder.

## Python bindings (work in progress)
Python bindings are being developed in the main development git branch (currently: v14). The GeoDmsPython project produces the required Python3 bindings using pybin11.
First make sure python 3.10 or above is installed on your system, which can be downloaded [here](https://www.python.org/downloads/). Next make sure pybind11 is installed, using command:

<pre>
vcpkg install python3 --triplet x64-windows
vcpkg install pybind11 --triplet x64-windows
</pre>

Then in the GeoDmsPython project properties make sure that the include path refers correctly to your vcpkg python locations
- C/C++ -> Additional Include Directories: 
![image](https://github.com/ObjectVision/GeoDMS/assets/2284361/43b39724-032c-47cd-af08-3dcd142d85f5)

If all is setup, GeoDmsPython project can be build without errors, the output will be named geodms.pyd in the output folder (python dynamic module).

Now open a terminal in the build folder where geodms.pyd is located and type "python" which will start up python in command line mode.
To test the geodms module import the version function:

```python
from geodms import version
```

And evaluate:

```python
version()
```

Which should return a proper version string, which depends on the build time and type:
![image](https://github.com/ObjectVision/GeoDMS/assets/96182097/549fd537-293a-42de-8152-c5f3d6f81067)

Additionally, you can run LoadConfigFromPython.py, located in the LoadConfigFromPython project folder.

## Native Linux compilation (work in progress)
Native Linux compilation is being prepared in the linux_gcc branch. 

Steps that i did to get somewhere are:
* Install WSL (Ubuntu) and set ssh on port 23
```
sudo apt update
sudo apt upgrade
sudo apt install net-tools
cd /etc/ssh/sshd_config
vi sshd_config ;
sudo service ssh restart
```

* install gcc and vcpkg

```
sudo apt install g++ gdb make ninja-build rsync zip
sudo apt install vcpkg
cd /mnt/c/vcpkg/scripts/buildsystems
./bootstrap-vcpkg.sh
vcpkg install boost --triplet x64-linux
vcpkg integrate install
```



