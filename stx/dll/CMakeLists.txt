# Stx - Syntax / parsing library
file(GLOB_RECURSE STX_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE STX_HEADERS CONFIGURE_DEPENDS src/*.h)

add_library(DmStx SHARED ${STX_SOURCES} ${STX_HEADERS})
target_precompile_headers(DmStx PRIVATE src/StxPCH.h)

target_include_directories(DmStx
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmStx PRIVATE DMSTX_EXPORTS)

# Spirit parsing is header-only and serialized via GetSpiritGrammarMutex() -> no boost_thread.
# Boost headers come transitively from DmRtc's PUBLIC Boost::boost (header-only umbrella),
# so DmStx needs no boost find_package / link of its own. (Was: COMPONENTS thread + Boost::thread,
# a leftover from before boost_thread was eliminated -- it also broke narrowing the boost manifest.)
target_link_libraries(DmStx PUBLIC DmRtc)
