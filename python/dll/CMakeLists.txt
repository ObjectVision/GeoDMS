# geodms.pyd — Python binding for GeoDMS (Windows-only, pybind11)
# Mirrors DmsPython.vcxproj: single source Bindings.cpp -> geodms.pyd

if(NOT WIN32)
    return()
endif()

# Link against the SAME Python the msbuild build (DmsPython.vcxproj uses %PYTHON3_*%)
# and the test harness (batch/Unit/PythonTest.bat -> `py -3.13`) use -- NOT the
# vcpkg-provided python3 build-tool port. Under the active vcpkg toolchain,
# find_package(Python3) otherwise resolves to vcpkg's python3 (currently 3.12), so the
# resulting geodms.pyd is a cp312 module and fails to import into the 3.13 runtime
# interpreter ("Module use of python312.dll conflicts with this version of Python").
# See https://github.com/ObjectVision/GeoDMS/issues/1105 for the multi-ABI end state;
# this is the single-pinned-version (N=1) tactical step toward it.
if(DEFINED ENV{PYTHON3_INCLUDE_DIR})
    get_filename_component(_py3_root "$ENV{PYTHON3_INCLUDE_DIR}" DIRECTORY)
    set(Python3_ROOT_DIR   "${_py3_root}")
    set(Python3_EXECUTABLE "${_py3_root}/python.exe")
    set(Python3_FIND_STRATEGY LOCATION)
    set(Python3_FIND_REGISTRY NEVER)
    message(STATUS "geodms.pyd: pinning Python3 to ${_py3_root} (PYTHON3_INCLUDE_DIR)")
else()
    message(WARNING "geodms.pyd: PYTHON3_INCLUDE_DIR not set; find_package(Python3) may "
                    "resolve to the vcpkg python3 port and build an ABI that the test "
                    "harness/runtime interpreter cannot import.")
endif()

find_package(Python3 REQUIRED COMPONENTS Development)
find_package(pybind11 CONFIG REQUIRED)

# MODULE library builds as a .pyd (Python extension) on Windows
python3_add_library(geodms MODULE src/Bindings.cpp WITH_SOABI)

# Match DmsPython.vcxproj: output name is always "geodms.pyd" regardless of config.
# Suppress the Python debug ABI suffix (_d) and any CMake debug postfix.
set_target_properties(geodms PROPERTIES
    OUTPUT_NAME "geodms"
    SUFFIX ".pyd"
    PREFIX ""
    DEBUG_POSTFIX ""
)

target_include_directories(geodms PRIVATE
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
    DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo DmShv
)
