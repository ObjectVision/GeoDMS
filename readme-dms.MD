This document describes most of the folder structure of source code of the GeoDMS and how to build the GeoDMS from it.

https://github.com/ObjectVision/GeoDMS

Version info: see [rtc/dll/src/RtcGeneratedVersion.h](https://github.com/ObjectVision/GeoDMS/blob/main/rtc/dll/src/RtcGeneratedVersion.h)

This folder contains all the code for building the executables and required libraries of the GeoDMS.

The C++ products can be compiled with MS Visual Studio 2023 (17.9.x)
Supported Platforms: x64 (Win32 still available but not recently tested)
Supported Configurations: Debug and Release

Open the VS solution file: all22.sln. This solution refers to projects in the following folders for building specific .dlls and .exes:

Folders which relate to Dynamic Link Libraries (DLLs) of the GeoDMS:
- rtc: Runtime Core Library
- sym: Lisp evaluator for processing parsed calculation rules
- tic: Tree Item Classes, Data Items and Units and related services.
- stx: configuration syntax parser and producer of an internal representation of a model and its calculation rules.
- stg: Storage Managers, providing a generic interface to various storage managers, such as LibTIFF and GDAL
- clc: implementation of most common operators and functions: arithmetic, aggregation, relational, logical, meta-scripting, string operations, overlay, ramp
- geo: implementation of geometric and network operators, such as impedance matrix functions, raster convolution, polygon and linestring operations
- shv: Viewer Components (TableView, MapView) and related Carets, Controllers, and Graphic Components.
- python: Python bindings to the GeoDMS

Folders which relate to Executables (EXEs) of the GeoDMS:
- qtgui
- run
- GeoDmsPython
- doc: folder that contains some technical and architectural working documents; not up to date, complete, correct or implemented.

Other product related folders:
- LoadConfigFromPython: a python component to load a GeoDms configuration and retrieve data from it.
- msbuild_linux
- nsi


Auxiliary folders:
- html: resources that are/were used for supporting the .adms implementation of ExportSettings.
- res: various resources, such as editor syntax files, the dms.ttf, RewriteExpr.lsp
- splash
- obj: folder that contains only intermediate products of the build process, such as .obj files, with sub folders $(Configuration)\$(Platform)
- bin: folder that contains only products of the build process with sub folders $(Configuration)\$(Platform)
- distr: folder that contains the results of running BuildSignAndCreateSetup.bat, that follows a successful build to generate a setup file for the GeoDMS.

For installing GDAL, OpenSLL, boost, etc., use vcpkg as described at https://github.com/ObjectVision/GeoDMS/wiki/Compiling-the-GeoDMS

The clc component optionally makes use of the Intel Integrated Performance Primitives for Windows.
This option is enabled by default in rtc/dll/src/RtcBase.h by inclusion of the line:

#define DMS_USE_INTEL_IPP

A developers version of this library
can be obtained at http://www.intel.com/software/products/ipp/index.htm

The Stg component makes use of a library for tiff file processing,
copyrighted by Sam Leffler and Silicon Graphics, Inc.
 