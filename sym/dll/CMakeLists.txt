# Sym - Symbol library
file(GLOB_RECURSE SYM_SOURCES src/*.cpp)
file(GLOB_RECURSE SYM_HEADERS src/*.h)

add_library(DmSym SHARED ${SYM_SOURCES} ${SYM_HEADERS})

target_include_directories(DmSym
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmSym PRIVATE DMSYM_EXPORTS)

target_link_libraries(DmSym PUBLIC DmRtc)
