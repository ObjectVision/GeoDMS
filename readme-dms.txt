This document describes most of the folder structure of source code of the GeoDMS and how to build the GeoDMS from it.

Geographic Data & Model Server (GeoDMS) is a platform for developing Geographic Planning Support Systems. 
Copyright (C) 2006...    Object Vision BV. 

https://github.com/ObjectVision/GeoDMS

Version info: see rtc/dll/src/RtcGeneratedVersion.h

This folder contains all the code for the .DLL's and .Exe of the GeoDMS.

The C++ products can be compiled with MS Visual Studio 2023 (17.8.x)
Supported Platforms: x64 (Win32 still available but not recently tested)
Supported Configurations: Debug and Release

Open the workspace file: all22.sln. The workspace file refers to the following project files:

rtc: Runtime Core Library
tic: Tree Item Classes, Data Items and Units and related services.
stx: configuration syntax parser and producer of an internal representation of a model and its calculation rules.
sym: Lisp evaluator for processing parsed calculation rules
stg: Storage Managers, providing a generic interface to various storage managers, such as LibTIFF and GDAL
clc: implementation of common operators
geo: implementation of geometric and network operators, such as impedance matrix functions, raster convolution, polygon and arc operations
shv: Viewer Components (TableViewer, MapViewer) and related Carets, and Graphic Components.
GeoDmsGuiQT
geoDmsRun
GeoDmsPython

They all compiled to bin/%Config%/%platform%/%ProjName%.dll


For installing GDAL, OpenSLL, boost, etc., use vcpkg as described at https://github.com/ObjectVision/GeoDMS/wiki/Compiling-the-GeoDMS

The clc component optionally makes use of the Intel Integrated Performance Primitives for Windows.
This option is enabled by default in rtc/dll/src/RtcBase.h by inclusion of the line:

#define DMS_USE_INTEL_IPP

A developers version of this library
can be obtained at http://www.intel.com/software/products/ipp/index.htm

The Stg component makes use of a library for tiff file processing,
copyrighted by Sam Leffler and Silicon Graphics, Inc.
 