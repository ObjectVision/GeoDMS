# Tic - Tree Item Core library
file(GLOB_RECURSE TIC_SOURCES src/*.cpp)
file(GLOB_RECURSE TIC_HEADERS src/*.h)

add_library(DmTic SHARED ${TIC_SOURCES} ${TIC_HEADERS})

target_include_directories(DmTic
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmTic PRIVATE DMTIC_EXPORTS)

target_link_libraries(DmTic PUBLIC DmRtc DmSym)
