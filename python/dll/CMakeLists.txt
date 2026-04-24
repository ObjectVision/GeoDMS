# geodms.pyd — Python binding for GeoDMS (Windows-only, pybind11)
# Mirrors DmsPython.vcxproj: single source Bindings.cpp -> geodms.pyd

if(NOT WIN32)
    return()
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
