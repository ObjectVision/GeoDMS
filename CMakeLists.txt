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

# Deploy resource files next to executables
file(COPY ${CMAKE_SOURCE_DIR}/res/RewriteExpr.lsp DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Deploy GDAL and PROJ geographic data files (needed at runtime by DmStg via GDAL/PROJ)
# vcpkg installs these into vcpkg_installed/<triplet>/share/ during cmake configure.
if(DEFINED VCPKG_TARGET_TRIPLET)
    set(_vcpkg_share ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share)
    if(IS_DIRECTORY ${_vcpkg_share}/gdal)
        file(COPY ${_vcpkg_share}/gdal/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gdaldata)
    endif()
    if(IS_DIRECTORY ${_vcpkg_share}/proj)
        file(COPY ${_vcpkg_share}/proj/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/proj4data)
    endif()
endif()

# Deploy DMS script library and example configurations
foreach(_res_dir library examples)
    if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/CopyResources/${_res_dir})
        file(COPY ${CMAKE_SOURCE_DIR}/CopyResources/${_res_dir}
             DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()
endforeach()
