# Copyright (C) 1998-2026 Object Vision B.V.
# License: GNU GPL 3

cmake_minimum_required(VERSION 3.25)

# Version: parsed from the tracked header that the C++ code itself includes, so the
# cmake flavor can never disagree with the compiled binaries. Bump it there, not here.
# GeoDmsVersion.cmd parses the same file for the msbuild/NSIS side.
# (This used to be a hard-coded VERSION that had drifted to 15.0.0.)
set(DMS_VERSION_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/rtc/dll/src/RtcVersionNumbers.h")

foreach(_part MAJOR MINOR PATCH)
    file(STRINGS "${DMS_VERSION_HEADER}" _dms_line REGEX "^#define DMS_VERSION_${_part} +[0-9]+")
    if(NOT _dms_line)
        message(FATAL_ERROR
            "No '#define DMS_VERSION_${_part} <n>' in ${DMS_VERSION_HEADER}")
    endif()
    string(REGEX REPLACE "^#define DMS_VERSION_${_part} +([0-9]+).*$" "\\1"
           DMS_VERSION_${_part} "${_dms_line}")
endforeach()

project(GeoDMS
    VERSION ${DMS_VERSION_MAJOR}.${DMS_VERSION_MINOR}.${DMS_VERSION_PATCH}
    LANGUAGES CXX
)

# Re-configure when the version is bumped, so a cached build dir cannot go stale.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DMS_VERSION_HEADER}")

# buildstamp.h is genuinely per-build and stays generated + gitignored, but the
# batch scripts are the only thing that writes it. Seed it here when absent so a
# fresh clone configures and builds without having to run a .bat first. Only when
# missing: rewriting it every configure would force a needless Rtc relink.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rtc/dll/src/buildstamp.h")
    string(TIMESTAMP _dms_build_date "%Y-%m-%d")
    string(TIMESTAMP _dms_build_time "%H:%M:%S")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/rtc/dll/src/buildstamp.h"
         "#define DMS_BUILD_DATE \"${_dms_build_date}\"\n"
         "#define DMS_BUILD_TIME \"${_dms_build_time}\"\n")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Visual Studio is a multi-config generator: it appends /<Config> to output dirs.
# Each CMake preset already has its own binary dir, so flattening to bin/ here
# causes no collision and keeps resource deployment (file(COPY)) consistent
# with the single-config Ninja generator used on Linux.
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${_cfg} _cfg_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_upper} ${CMAKE_BINARY_DIR}/bin)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_cfg_upper} ${CMAKE_BINARY_DIR}/bin)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_cfg_upper} ${CMAKE_BINARY_DIR}/lib)
    endforeach()
endif()

# Build feedback
set(CMAKE_COLOR_DIAGNOSTICS ON)

# Global preprocessor definitions matching DmsDef.props
add_compile_definitions(
    _CRT_SECURE_NO_WARNINGS
    BOOST_SPIRIT_USE_OLD_NAMESPACE
    # NB: BOOST_SPIRIT_THREADSAFE intentionally NOT defined. Spirit Classic parsing is
    # serialized via GetSpiritGrammarMutex() (stx/dll/src/SpiritTools.h) instead, so stx
    # does not pull boost::thread_specific_ptr / the boost_thread runtime lib. MUST stay in
    # sync with DmsDef.props, which also dropped it -- otherwise the CMake (.c/.l) build links
    # boost_thread while the installer no longer ships it -> STATUS_DLL_NOT_FOUND at launch.
    # Marker so DMS_PLATFORM (rtc/dll/src/gen/General.cpp) can distinguish
    # cmake builds from msbuild builds in the GUI title bar.
    DMS_TOOLSET_CMAKE
)

# Platform-specific settings
if(MSVC)
    add_compile_options(/utf-8 /bigobj /W3 /EHsc /GR /fp:precise)
    # 16k MSF pages: raises the PDB size cap from 4 GB to 16 GB; Debug Clc.pdb (3.2 GB)
    # was hitting the 4 GB cap on incremental links (matches DmsDef.props on the msbuild side)
    add_link_options(/pdbpagesize:16384)
    # Build every module as a Unicode module, matching the msbuild side
    # (CharacterSet=Unicode in Directory.Build.props). GeoDMS is UTF-8 internally and uses
    # explicit -W Win32 calls at OS boundaries, so generic Win32/CRT macros resolving to the
    # wide variants is consistent. UNICODE is meaningless on Linux, hence MSVC-only here.
    add_compile_definitions(UNICODE _UNICODE)
else()
    # Warning policy (GCC/Clang). GeoDMS is developed primarily against MSVC, so a full
    # -Wall -Wextra -Wpedantic surfaces ~67k mostly-benign cross-compiler / style warnings on
    # Linux, drowning out the few genuine ones. We keep the bug-catching warnings (switch,
    # strict-aliasing, maybe-uninitialized, class-memaccess, overflow, return-type,
    # parentheses, deprecated-declarations, ...) and silence the high-volume / low-signal
    # categories below. -Wpedantic is dropped entirely: it only flags deliberate GCC/MSVC
    # extensions used by design (flexible array members, macro trailing ';').
    add_compile_options(-Wall -Wextra -pthread
        -fno-fast-math
        -ffp-contract=off
        -fexcess-precision=standard
    )
    add_compile_options(
        -Wno-unused-parameter        # ~21k: pervasive in virtual/overload signatures
        -Wno-deprecated-copy         # ~17k: user dtor/copy-ctor + implicit copy-assign idiom
        -Wno-unused-variable         # ~4k
        -Wno-unused-but-set-variable # ~2k
        -Wno-reorder                 # ~1.5k: member-init order, harmless here
        -Wno-unknown-pragmas         # ~0.9k: MSVC #pragma warning / region across the tree
        # -Wsign-compare is intentionally NOT suppressed: the tree is clean under it and it
        # catches real signed/unsigned comparison bugs (kept enabled as a ratchet).
        -Wno-endif-labels            # ~0.5k: "#endif FOO" comment style
        -Wno-unused-local-typedefs   # ~0.2k
        -Wno-deprecated-declarations # third-party API deprecations (libtiff uint16, Qt, GDAL, boost) used in .cpp
        -Wno-overflow                # intentional undefined-value sentinels (UndefinedValue() = all-bits/sign-bit, Undefined.h)
        -Wno-switch                  # ~2k: codebase idiom = switch handles the relevant
                                     # subset, with the "default" written as code after the
                                     # switch (often assert-guarded); not forgotten cases
    )
    add_link_options(-pthread)
    # GCC/Clang Debug builds: define _DEBUG to match MSVC convention,
    # which triggers MG_DEBUG in RtcBase.h for debug-only diagnostics.
    add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
endif()

# Find third-party packages (via vcpkg or system)
find_package(Boost REQUIRED)

# Library projects (dependency order)
# rtc/dll is the merged runtime-core DLL (rtc + sym + tic); the sym/tic sources live under rtc/dll/src/{sym,tic}.
add_subdirectory(rtc/dll)
add_subdirectory(stx/dll)
add_subdirectory(stg/dll)
add_subdirectory(clc/dll)
add_subdirectory(geo/dll)
add_subdirectory(shv/dll)

# Executables
add_subdirectory(run/exe)
add_subdirectory(qtgui/exe)

# Python binding (Windows-only; guarded inside its own CMakeLists.txt)
add_subdirectory(python/dll)

# Deploy resource files next to the executables. This MUST run at BUILD time,
# not only at configure time: BuildSignAndCreateSetup{Cmake,Linux}.bat wipe the
# bin\ output dir before each build, and an incremental `cmake --build` does not
# re-run configure -- so a plain file(COPY) here would never restore the wiped
# RewriteExpr.lsp / gdaldata / library / examples / *.py and the installer (and
# the unit tests) would fail. The deploy_resources target below runs the copy
# script on every build instead. See tools/DeployResources.cmake for details.
#
# Resolve the (configure-time) vcpkg share dir here and pass it to the script.
# Prefer VCPKG_INSTALLED_DIR (set when the repo-root vcpkg_installed/ is used,
# e.g. on Windows where msbuild already ran vcpkg). Fall back to the build-tree
# install used by vcpkg manifest mode on Linux.
set(_vcpkg_share "")
if(DEFINED VCPKG_TARGET_TRIPLET)
    if(DEFINED VCPKG_INSTALLED_DIR AND IS_DIRECTORY ${VCPKG_INSTALLED_DIR})
        set(_vcpkg_share ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share)
    else()
        set(_vcpkg_share ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share)
    endif()
endif()

# Resolve the Microsoft C/C++ runtime redistributable DLLs, so deploy_resources
# can put them next to the executables. The msbuild flavour gets these for free
# (DmsRelease.props sets CopyCppRuntimeToOutputDir); cmake has no equivalent, so
# GeoDms<ver>.c shipped WITHOUT any CRT and silently fell back to whatever
# VC++ redistributable happened to be installed on the target machine. Anything
# older than 14.40 lacks msvcp140_atomic_wait.dll's __std_calloc_crt, which
# arrow.dll (pulled in by gdal.dll) imports -- so the GUI died at startup with
# "the procedure entry point __std_calloc_crt could not be located in ...
# arrow.dll" on user machines while working fine on the build servers. See #1186.
#
# _SKIP suppresses the module's own install() rule: the DLLs are deployed by
# deploy_resources (build time, survives the bin\ wipe), not by `cmake --install`.
set(_msvc_runtime_libs "")
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    set(_msvc_runtime_libs ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
    if(NOT _msvc_runtime_libs)
        message(WARNING "No MSVC runtime redistributable found -- the .c setup will depend on the VC++ redistributable installed on the target machine (see #1186).")
    endif()
endif()

add_custom_target(deploy_resources ALL
    COMMAND ${CMAKE_COMMAND}
            -DSRC_DIR=${CMAKE_SOURCE_DIR}
            -DRUNTIME_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            -DVCPKG_SHARE=${_vcpkg_share}
            "-DMSVC_RUNTIME_LIBS=${_msvc_runtime_libs}"
            -P ${CMAKE_SOURCE_DIR}/tools/DeployResources.cmake
    COMMENT "Deploying runtime resources into ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    VERBATIM
)
