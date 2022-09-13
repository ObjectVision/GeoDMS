This document describes the folder structure of source code of the GeoDMS and how to build the GeoDMS from it.

Geographic Data & Model Server (GeoDMS) is a platform for developing Geographic Planning Support Systems. 
Copyright (C) 1998-2005  YUSE GSO Object Vision BV. 
Copyright (C) 2006...    Object Vision BV. 

http://http://www.objectvision.nl/geodms

Version info: see rtc/dll/src/RtcVersion.h

This folder contains all the code for the .DLL's and .Exe of the GeoDMS.

The C++ products can be compiled with MS Visual Studio 2019 (version 16.9.2). 
Supported Platforms: Win32 and x64
Supported Configurations: Debug and Release

If you just got this source code, make sure you installed the third party source code in the following sub-folders:
- gdal-1.9.1
- gdal 2.3
- proj-4.8
Then check that in these sub-folders, the following files should be updated from our svn-repository:
proj-4.8/makeproj.vcxproj

Also, install the following folders by downloading and if neccesary obtain licenses
C:\SourceData\ext\boost\boost_1_53_0 (download and expand source code here from boost.org)
C:\SourceData\ext\Intel\IPP\7.0 ( or disable the use of Intel\IPP in   geo\dll\src\UseIpp.h )
C:\SourceData\ext\FileGDB\FileGDB_API_VS2010_1_1 (closed-source license obtainable at ESRI for no costs).
C:\SourceData\ext\Xerces\xerces-c-3.1.1
C:\SourceData\ext\Xerces\xerces-c-3.1.1-x86-windows-vc-10.0
C:\SourceData\ext\Xerces\xerces-c-3.1.1-x86_64-windows-vc-10.0
C:\Program Files (x86)\PostgreSQL\9.2\include
C:\Program Files (x86)\PostgreSQL\9.2\bin -> C:\SourceData\ext\PostgreSQL (x86)\9.2\bin
C:\Program Files\PostgreSQL\9.2\bin       -> C:\SourceData\ext\PostgreSQL (x64)\9.2\bin


You need to run BoostBuild.bat which builds the boost libraries for the supported variants.

Open the workspace file: all15.sln. The workspace file refers to the following project files:

rtc: Runtime Core Library
tic: Tree Item Classes, Data Items and Units and related services.
stx: configuration syntax parser and producer of an internal representation of a model and its calculation rules.
sym: Lisp / Prolog evaluator for processing parsed calculation rules
stg: Storage Managers, providing a generic interface to StgImpl and GDAL
StgImpl: Storage Implementations, including native .shp and .dbf management, the TIFF lib, and ODBC connection management.
gdal: library for reading and writing many GIS raster and vector formats as well as database tables.
proj-4.8.0: library for processing and converting geographic projections.
clc1: implementation of common operators
clc2: more operators
geo: implementation of geometric and network operators, such as Dijkstra functions, raster convolution, polygon and arc operations
shv: Viewer Components (TableViewer, MapViewer) and related Carets, and Graphic Components.
TicTst  = Various test code modules.

They all compile to bin/%Config%/%platform%/%ProjName%.dll

For building GDAL, install GDAL 1.9.1 into gdal-1.9.1 folder and don't overwrite the provided files. 
Load makegdal90.vcxproj into all10.sln and compile to the desired config and/or platform. 
The provided GDAL build settings include building bindings with FileGdbApi.dll and Xerces for GML support.
See/edit the changed settings for proper paths to these external components.

The C++ code includes parts of the boost library. 
See the include path setting in the project files for the lastest used boost version.
Make sure that this is installed in c:\SourceData\ext/boost/<version>
or change the include path settings in the project files if you have placed the boost library elsewhere.

The clc component optionally makes use of the Intel Integrated Performance Primitives 7.0 for Windows.
This option is enabled by default in rtc/dll/src/RtcBase.h by inclusion of the line:

#define DMS_USE_INTEL_IPP

An evaluation or developers version of this library
can be obtained at http://www.intel.com/software/products/ipp/index.htm

The StgImpl component makes use of a library for tiff file processing,
copyrighted by Sam Leffler and Silicon Graphics, Inc.
 
GeoDmsClient.exe is a GUI interface to the functions in the GeoDMS.
The XE2 sub-folder contains the Deplhi XE2 source code for building the GeoDmsGui.exe

The html folder contains .html code of dialogs that the GeoDmsGui uses to interact with the engine.
