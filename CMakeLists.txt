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

# Global preprocessor definitions matching DmsDef.props
add_compile_definitions(
    _CRT_SECURE_NO_WARNINGS
    BOOST_SPIRIT_USE_OLD_NAMESPACE
    BOOST_SPIRIT_THREADSAFE
)

# Platform-specific settings
if(MSVC)
    add_compile_options(/utf-8 /bigobj /W3 /EHsc /GR)
    add_compile_options($<$<CONFIG:Debug>:/Od /Zi>)
    add_compile_options($<$<CONFIG:Release>:/O2 /Ob2 /DNDEBUG>)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options($<$<CONFIG:Debug>:-O0 -g>)
    add_compile_options($<$<CONFIG:Release>:-O2 -DNDEBUG>)
    add_link_options(-pthread)
endif()

# Find third-party packages (via vcpkg or system)
find_package(Boost REQUIRED)
find_package(GDAL REQUIRED)
find_package(FFTW3 CONFIG REQUIRED)
find_package(CGAL CONFIG REQUIRED)
find_package(GEOS CONFIG REQUIRED)

# Library projects (dependency order)
add_subdirectory(rtc/dll)
add_subdirectory(sym/dll)
add_subdirectory(tic/dll)
add_subdirectory(stx/dll)
add_subdirectory(stg/dll)
add_subdirectory(clc/dll)
add_subdirectory(geo/dll)

# Executables
add_subdirectory(run/exe)
