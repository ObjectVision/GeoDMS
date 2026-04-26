# Clc - Calculation operators library
file(GLOB_RECURSE CLC_SOURCES src/*.cpp)
file(GLOB_RECURSE CLC_HEADERS src/*.h include/*.h)

add_library(DmClc SHARED ${CLC_SOURCES} ${CLC_HEADERS})

target_include_directories(DmClc
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(DmClc PRIVATE DMCLC_EXPORTS)

if(NOT WIN32)
    # GNU ld 2.42 has a bug where PT_DYNAMIC points to a stale copy of the dynamic section
    # when .dynstr exceeds ~51 MB (DmClc has ~54 MB of symbol names in release builds).
    # The gold linker does not have this bug and produces a correct PT_DYNAMIC.
    target_link_options(DmClc PRIVATE -fuse-ld=gold)
endif()

find_package(GDAL)
find_package(PROJ CONFIG)

target_link_libraries(DmClc PUBLIC DmRtc DmSym DmTic DmStx DmStg)
if(GDAL_FOUND)
    target_link_libraries(DmClc PRIVATE GDAL::GDAL)
endif()
if(PROJ_FOUND)
    target_link_libraries(DmClc PRIVATE PROJ::proj)
endif()
