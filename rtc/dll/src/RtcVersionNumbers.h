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
// It used to work the other way round: GeoDmsVersion.cmd WROTE a generated,
// gitignored RtcGeneratedVersion.h. That made the version a build artefact of
// the batch scripts, so a plain `msbuild all22.sln`, an F5 in Visual Studio, a
// CMake build or CI compiled against whatever version the last batch run
// happened to leave behind -- and a fresh clone did not compile at all. The
// direction is now inverted, because C++ is the only consumer that cannot read
// a plain data file without a generation step, while cmd and CMake parse
// `#define NAME value` in one line each.
//
// Keep the format simple: three #defines, decimal, one space before the number.
// The findstr and file(STRINGS) patterns match on `#define DMS_VERSION_<PART> `.

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_VERSIONNUMBERS_H)
#define __RTC_VERSIONNUMBERS_H

#define DMS_VERSION_MAJOR 20
#define DMS_VERSION_MINOR 12
#define DMS_VERSION_PATCH 0

#endif // __RTC_VERSIONNUMBERS_H
