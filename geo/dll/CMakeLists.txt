# Geo - Geometry operators library
file(GLOB_RECURSE GEO_SOURCES src/*.cpp)
file(GLOB_RECURSE GEO_HEADERS src/*.h)

add_library(DmGeo SHARED ${GEO_SOURCES} ${GEO_HEADERS})

target_include_directories(DmGeo
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmGeo PRIVATE GEO_EXPORTS)

target_link_libraries(DmGeo PUBLIC DmRtc DmSym DmTic DmStx DmStg)
target_link_libraries(DmGeo PRIVATE FFTW3::fftw3 CGAL::CGAL GEOS::geos)
