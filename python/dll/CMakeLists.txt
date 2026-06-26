# geodms.pyd — Python binding for GeoDMS (Windows-only, pybind11)
# Mirrors DmsPython.vcxproj: single source Bindings.cpp -> geodms.pyd

if(NOT WIN32)
    return()
endif()

# Build against the SAME Python the msbuild build (DmsPython.vcxproj uses %PYTHON3_*%) and
# the test harness (batch/Unit/PythonTest.bat -> `py -3.13`) use, by consuming the include
# dir and import library directly from %PYTHON3_INCLUDE_DIR% / %PYTHON3_LIBRARIES% — exactly
# like DmsPython.vcxproj (%PYTHON3_INCLUDE_DIR% + %PYTHON3_LIBRARIES%/*.lib).
#
# We deliberately do NOT call find_package(Python3 ... COMPONENTS Development): under the
# vcpkg toolchain the python3 cmake-wrapper (share/python3/vcpkg-cmake-wrapper.cmake) forces
# vcpkg's own python3 (currently 3.12) for the Development component, regardless of any cache
# pin, so geodms.pyd would link cp312 and fail to import in the 3.13 runtime ("Module use of
# python312.dll conflicts with this version of Python"). See issue #1105 for the multi-ABI
# end state; this is the single-pinned-version (N=1) tactical step toward it.
if(NOT DEFINED ENV{PYTHON3_INCLUDE_DIR} OR NOT DEFINED ENV{PYTHON3_LIBRARIES})
    message(WARNING "geodms.pyd: PYTHON3_INCLUDE_DIR / PYTHON3_LIBRARIES not set; skipping the "
                    "Python binding. Set them to the target CPython (e.g. 3.13) install, as the "
                    "msbuild build and the py launcher used by the test harness do.")
    return()
endif()

set(_py3_inc "$ENV{PYTHON3_INCLUDE_DIR}")
# %PYTHON3_LIBRARIES% is the libs directory; pick the version-stamped import lib (python3XX.lib,
# not the _d debug variant).
file(GLOB _py3_libs "$ENV{PYTHON3_LIBRARIES}/python3??.lib")
if(NOT _py3_libs)
    message(FATAL_ERROR "geodms.pyd: no python3??.lib found under $ENV{PYTHON3_LIBRARIES}")
endif()
list(GET _py3_libs 0 _py3_lib)
message(STATUS "geodms.pyd: using Python include ${_py3_inc} and lib ${_py3_lib}")

find_package(pybind11 CONFIG REQUIRED)

# MODULE library builds as a .pyd (Python extension) on Windows. We create it directly
# (not via python3_add_library) to avoid find_package(Python3) and the vcpkg wrapper.
add_library(geodms MODULE src/Bindings.cpp)

# Match DmsPython.vcxproj: output name is always "geodms.pyd" regardless of config.
set_target_properties(geodms PROPERTIES
    OUTPUT_NAME "geodms"
    SUFFIX ".pyd"
    PREFIX ""
    DEBUG_POSTFIX ""
)

target_include_directories(geodms PRIVATE
    ${_py3_inc}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/sym/dll/src
    ${CMAKE_SOURCE_DIR}/tic/dll/src
    ${CMAKE_SOURCE_DIR}/rtc/dll/src
    ${CMAKE_SOURCE_DIR}/geo/dll/src
    ${CMAKE_SOURCE_DIR}/clc/dll/include
    ${CMAKE_SOURCE_DIR}/stx/dll/src
    ${CMAKE_SOURCE_DIR}/shv/dll/src
    ${CMAKE_SOURCE_DIR}/stg/dll/src
)

target_link_libraries(geodms PRIVATE
    pybind11::headers
    ${_py3_lib}
    DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo DmShv
)
