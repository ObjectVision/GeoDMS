# Clc - Calculation operators library
file(GLOB_RECURSE CLC_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE CLC_HEADERS CONFIGURE_DEPENDS src/*.h include/*.h)

add_library(DmClc SHARED ${CLC_SOURCES} ${CLC_HEADERS})
target_precompile_headers(DmClc PRIVATE src/ClcPCH.h)

target_include_directories(DmClc
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(DmClc PRIVATE DMCLC_EXPORTS)

find_package(GDAL)
find_package(PROJ CONFIG)

target_link_libraries(DmClc PUBLIC DmRtc DmStx DmStg)
if(GDAL_FOUND)
    # GDAL's headers only: DmClc calls GDAL through the functions of DmStg in gdal/gdal_base.h,
    # because DmStg holds the engine's only GDAL (#1381).
    target_include_directories(DmClc SYSTEM PRIVATE $<TARGET_PROPERTY:GDAL::GDAL,INTERFACE_INCLUDE_DIRECTORIES>)
endif()
if(PROJ_FOUND)
    target_link_libraries(DmClc PRIVATE PROJ::proj)
endif()
