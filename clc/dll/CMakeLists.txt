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

target_link_libraries(DmClc PUBLIC DmRtc DmSym DmTic DmStx DmStg)
