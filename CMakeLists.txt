# Copyright (C) 1998-2026 Object Vision B.V.
# License: GNU GPL 3

cmake_minimum_required(VERSION 3.25)

project(GeoDMS
    VERSION 15.0.0
    LANGUAGES CXX
)

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
    BOOST_SPIRIT_THREADSAFE
)

# Platform-specific settings
if(MSVC)
    add_compile_options(/utf-8 /bigobj /W3 /EHsc /GR)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -pthread)
    add_link_options(-pthread)
    # GCC/Clang Debug builds: define _DEBUG to match MSVC convention,
    # which triggers MG_DEBUG in RtcBase.h for debug-only diagnostics.
    add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
endif()

# Find third-party packages (via vcpkg or system)
find_package(Boost REQUIRED)

# Library projects (dependency order)
add_subdirectory(rtc/dll)
add_subdirectory(sym/dll)
add_subdirectory(tic/dll)
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

# Deploy resource files next to executables
file(COPY ${CMAKE_SOURCE_DIR}/res/RewriteExpr.lsp DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Deploy GDAL and PROJ geographic data files (needed at runtime by DmStg via GDAL/PROJ).
# Prefer VCPKG_INSTALLED_DIR (set when the repo-root vcpkg_installed/ is used, e.g. on
# Windows where msbuild already ran vcpkg). Fall back to the build-tree install used by
# vcpkg manifest mode on Linux.
if(DEFINED VCPKG_TARGET_TRIPLET)
    if(DEFINED VCPKG_INSTALLED_DIR AND IS_DIRECTORY ${VCPKG_INSTALLED_DIR})
        set(_vcpkg_share ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share)
    else()
        set(_vcpkg_share ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share)
    endif()
    if(IS_DIRECTORY ${_vcpkg_share}/gdal)
        file(COPY ${_vcpkg_share}/gdal/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gdaldata)
    endif()
    foreach(_proj_share proj proj4)
        if(IS_DIRECTORY ${_vcpkg_share}/${_proj_share})
            file(COPY ${_vcpkg_share}/${_proj_share}/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/proj4data)
        endif()
    endforeach()
endif()

# Deploy DMS script library and example configurations
foreach(_res_dir library examples)
    if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/CopyResources/${_res_dir})
        file(COPY ${CMAKE_SOURCE_DIR}/CopyResources/${_res_dir}
             DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()
endforeach()

# Deploy Python utility scripts
foreach(_py_script profiler.py regression.py)
    if(EXISTS ${CMAKE_SOURCE_DIR}/profiler/${_py_script})
        file(COPY ${CMAKE_SOURCE_DIR}/profiler/${_py_script}
             DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()
endforeach()
