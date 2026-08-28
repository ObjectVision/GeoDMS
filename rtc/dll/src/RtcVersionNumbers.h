// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// SINGLE SOURCE OF TRUTH for the GeoDMS release version. Bump it here, nowhere
// else, and commit the change -- this file is tracked, not generated.
//
// Everything else reads these three numbers back out of this header:
//   - C++      : gen/General.cpp includes it (DMS_GetVersionNumber and friends)
//   - Windows  : GeoDmsVersion.cmd parses it with findstr and exports
//                DMS_VERSION_MAJOR/MINOR/PATCH for Build.bat, the three
//                batch\BuildSignAndCreateSetup*.bat scripts and, through
//                %GeoDmsVersion%, the NSIS setup scripts
//   - CMake    : CMakeLists.txt parses it with file(STRINGS) into project(VERSION)
//
// Do NOT invert this by having a script generate the header instead: that makes
// the version a build artefact, so a plain `msbuild all22.sln`, an F5 in Visual
// Studio, a CMake build or CI compiles against whatever the last script run left
// behind, and a fresh clone does not compile at all. C++ is the only consumer
// that cannot read a plain data file without a generation step, while cmd and
// CMake each parse `#define NAME value` in one line.
//
// Keep the format simple: three #defines, decimal, one space before the number.
// The findstr and file(STRINGS) patterns match on `#define DMS_VERSION_<PART> `.

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_VERSIONNUMBERS_H)
#define __RTC_VERSIONNUMBERS_H

#define DMS_VERSION_MAJOR 20
#define DMS_VERSION_MINOR 19
#define DMS_VERSION_PATCH 0

#endif // __RTC_VERSIONNUMBERS_H
