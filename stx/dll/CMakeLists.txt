# Stx - Syntax / parsing library
file(GLOB_RECURSE STX_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE STX_HEADERS CONFIGURE_DEPENDS src/*.h)

add_library(DmStx SHARED ${STX_SOURCES} ${STX_HEADERS})
target_precompile_headers(DmStx PRIVATE src/StxPch.h)

target_include_directories(DmStx
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmStx PRIVATE DMSTX_EXPORTS)

find_package(Boost REQUIRED COMPONENTS thread)
target_link_libraries(DmStx PUBLIC DmRtc DmSym DmTic)
target_link_libraries(DmStx PRIVATE Boost::thread)
