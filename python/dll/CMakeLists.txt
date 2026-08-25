# geodms.cp3XY-win_amd64.pyd -- Python binding for GeoDMS (Windows-only, pybind11)
# CPython minors come from ../PythonVersions.txt, shared with DmsPython.vcxproj.

if(NOT WIN32)
    return()
endif()

# Build the same ABI matrix as DmsPython.vcxproj. Each interpreter is supplied
# by PYTHON3XY_ROOT (for example PYTHON39_ROOT / PYTHON314_ROOT), or by matching
# *_INCLUDE_DIR + *_LIBRARIES variables. Generic PYTHON3_* variables are not
# accepted because they do not identify which matrix ABI they describe.
#
# We deliberately do NOT call find_package(Python3 ... COMPONENTS Development): under the
# vcpkg toolchain the python3 cmake-wrapper (share/python3/vcpkg-cmake-wrapper.cmake) forces
# vcpkg's own python3 (currently 3.12) for the Development component, regardless of any cache
# pin, so every target would silently link cp312 regardless of the intended ABI.
# Resolve the exact headers/import library ourselves instead. See issue #1105.
set(_geodms_python_version_file "${CMAKE_CURRENT_SOURCE_DIR}/../PythonVersions.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_geodms_python_version_file}")
file(READ "${_geodms_python_version_file}" _geodms_python_versions_text ENCODING UTF-8)
string(STRIP "${_geodms_python_versions_text}" _geodms_python_versions_text)
set(GEODMS_PYTHON_VERSIONS ${_geodms_python_versions_text})
if(NOT GEODMS_PYTHON_VERSIONS)
    message(FATAL_ERROR "python/PythonVersions.txt must contain a semicolon-separated ABI list")
endif()
foreach(_python_version IN LISTS GEODMS_PYTHON_VERSIONS)
    if(NOT _python_version MATCHES "^3\\.[0-9]+$")
        message(FATAL_ERROR "Invalid CPython minor '${_python_version}' in python/PythonVersions.txt")
    endif()
endforeach()
find_package(pybind11 CONFIG REQUIRED)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_py3_platform_tag "win_amd64")
else()
    set(_py3_platform_tag "win32")
endif()

function(add_geodms_python_binding python_version)
    string(REPLACE "." "" python_abi "${python_version}")
    string(REGEX REPLACE "^([0-9]+)\\..*$" "\\1" python_major "${python_version}")
    string(REGEX REPLACE "^[0-9]+\\.([0-9]+)$" "\\1" python_minor "${python_version}")

    set(root_var "GEODMS_PYTHON${python_abi}_ROOT")
    set(${root_var} "" CACHE PATH "CPython ${python_version} installation root for the GeoDMS binding")
    set(python_root "${${root_var}}")
    set(env_root_var "PYTHON${python_abi}_ROOT")
    set(env_inc_var "PYTHON${python_abi}_INCLUDE_DIR")
    set(env_lib_var "PYTHON${python_abi}_LIBRARIES")
    if(NOT python_root AND DEFINED ENV{${env_root_var}})
        set(python_root "$ENV{${env_root_var}}")
    endif()

    if(DEFINED ENV{${env_inc_var}})
        set(python_include "$ENV{${env_inc_var}}")
    elseif(python_root)
        set(python_include "${python_root}/include")
    endif()

    if(DEFINED ENV{${env_lib_var}})
        set(python_libraries "$ENV{${env_lib_var}}")
    elseif(python_root)
        set(python_libraries "${python_root}/libs")
    endif()

    set(python_import_lib "${python_libraries}/python${python_abi}.lib")
    if(NOT EXISTS "${python_include}/Python.h" OR NOT EXISTS "${python_import_lib}")
        message(FATAL_ERROR
            "CPython ${python_version} development files not found. Set ${root_var} at configure time "
            "or ${env_root_var} in the environment (expected include/Python.h and libs/python${python_abi}.lib); "
            "alternatively set ${env_inc_var} and ${env_lib_var}.")
    endif()
    message(STATUS "GeoDMS Python binding cp${python_abi}: include ${python_include}, lib ${python_import_lib}")

    # Create one independently compiled module per CPython ABI. All have the
    # same PyInit_geodms entry point; Python selects the matching tagged file.
    set(target_name "geodms_cp${python_abi}")

    # vcpkg wraps add_library() with app-local dependency deployment. Disable
    # that wrapper just for these extension targets, otherwise cp312 causes
    # vcpkg's own python312.dll to be copied into the setup output. The running
    # interpreter supplies this dependency; GeoDMS DLLs are deployed separately.
    set(_saved_vcpkg_applocal_deps "${VCPKG_APPLOCAL_DEPS}")
    set(VCPKG_APPLOCAL_DEPS OFF)
    add_library(${target_name} MODULE src/Bindings.cpp)
    set(VCPKG_APPLOCAL_DEPS "${_saved_vcpkg_applocal_deps}")
    set_target_properties(${target_name} PROPERTIES
        OUTPUT_NAME "geodms.cp${python_abi}-${_py3_platform_tag}"
        SUFFIX ".pyd"
        PREFIX ""
        DEBUG_POSTFIX ""
    )
    target_include_directories(${target_name} PRIVATE
        ${python_include}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/rtc/dll/src/sym
        ${CMAKE_SOURCE_DIR}/rtc/dll/src/tic
        ${CMAKE_SOURCE_DIR}/rtc/dll/src
        ${CMAKE_SOURCE_DIR}/geo/dll/src
        ${CMAKE_SOURCE_DIR}/clc/dll/include
        ${CMAKE_SOURCE_DIR}/stx/dll/src
        ${CMAKE_SOURCE_DIR}/shv/dll/src
        ${CMAKE_SOURCE_DIR}/stg/dll/src
    )
    target_compile_definitions(${target_name} PRIVATE
        GEODMS_PYTHON_MAJOR=${python_major}
        GEODMS_PYTHON_MINOR=${python_minor}
    )
    target_link_libraries(${target_name} PRIVATE
        pybind11::headers
        ${python_import_lib}
        DmRtc DmStx DmStg DmClc DmGeo DmShv
    )
endfunction()

foreach(_python_version IN LISTS GEODMS_PYTHON_VERSIONS)
    add_geodms_python_binding("${_python_version}")
endforeach()
