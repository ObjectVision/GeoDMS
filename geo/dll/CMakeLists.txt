# Geo - Geometry operators library
file(GLOB_RECURSE GEO_SOURCES src/*.cpp)
file(GLOB_RECURSE GEO_HEADERS src/*.h)

add_library(DmGeo SHARED ${GEO_SOURCES} ${GEO_HEADERS})

target_include_directories(DmGeo
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmGeo PRIVATE GEO_EXPORTS)

find_package(FFTW3 CONFIG)
find_package(CGAL CONFIG)
find_package(GEOS CONFIG)
find_package(GDAL)
find_package(PROJ CONFIG)

target_link_libraries(DmGeo PUBLIC DmRtc DmSym DmTic DmStx DmStg DmClc)
if(FFTW3_FOUND)
    target_link_libraries(DmGeo PRIVATE FFTW3::fftw3)
else()
    # Exclude FFTW3-dependent sources when FFTW3 is not available
    list(FILTER GEO_SOURCES EXCLUDE REGEX "(Potential|OperPot)\\.cpp$")
    set_property(TARGET DmGeo PROPERTY SOURCES ${GEO_SOURCES} ${GEO_HEADERS})
endif()
if(CGAL_FOUND)
    target_link_libraries(DmGeo PRIVATE CGAL::CGAL)
endif()
if(GEOS_FOUND)
    target_link_libraries(DmGeo PRIVATE GEOS::geos)
endif()
if(GDAL_FOUND)
    target_link_libraries(DmGeo PRIVATE GDAL::GDAL)
endif()
if(PROJ_FOUND)
    target_link_libraries(DmGeo PRIVATE PROJ::proj)
endif()

if(NOT WIN32)
    # GDAL (static) uses libjpeg/libpng/libtiff but they may not be pulled in by
    # --as-needed unless explicitly listed. Add them so unresolved refs from libgdal.a
    # are satisfied, then hide all three to prevent symbol conflicts with the system
    # Qt JPEG plugin (libqjpeg.so) which uses the system libjpeg.so.8.
    find_package(JPEG)
    find_package(PNG)
    if(JPEG_FOUND)
        target_link_libraries(DmGeo PRIVATE JPEG::JPEG)
    endif()
    if(PNG_FOUND)
        target_link_libraries(DmGeo PRIVATE PNG::PNG)
    endif()
    target_link_options(DmGeo PRIVATE
        -Wl,--exclude-libs,libjpeg.a
        -Wl,--exclude-libs,libtiff.a
        -Wl,--exclude-libs,libpng.a
        -Wl,--exclude-libs,libpng16.a
    )
endif()
