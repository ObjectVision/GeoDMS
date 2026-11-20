# Geo - Geometry operators library
file(GLOB_RECURSE GEO_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE GEO_HEADERS CONFIGURE_DEPENDS src/*.h)

add_library(DmGeo SHARED ${GEO_SOURCES} ${GEO_HEADERS})
target_precompile_headers(DmGeo PRIVATE src/GeoPCH.h)

target_include_directories(DmGeo
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# GEO_EXPORTS is defined by the DLL's PCH (src/GeoPCH.h). Defining it here as well
# caused C4005 (macro redefinition) in every TU; the .vcxproj relies on the PCH alone.

find_package(FFTW3 CONFIG)
find_package(FFTW3f CONFIG) # single-precision fftwf_ API, used by the Fft32 potential backends
find_package(CGAL CONFIG)
find_package(GEOS CONFIG)
find_package(GDAL)
find_package(PROJ CONFIG)

target_link_libraries(DmGeo PUBLIC DmRtc DmStx DmStg DmClc)
if(FFTW3_FOUND AND FFTW3f_FOUND)
    target_link_libraries(DmGeo PRIVATE FFTW3::fftw3 FFTW3::fftw3f)
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
    # GDAL's headers only: DmGeo calls GDAL through the functions of DmStg in gdal/gdal_base.h,
    # because DmStg holds the engine's only GDAL (#1381). Linking GDAL here as well gave DmGeo
    # a second, nearly complete copy of the static GDAL on Linux.
    target_include_directories(DmGeo SYSTEM PRIVATE $<TARGET_PROPERTY:GDAL::GDAL,INTERFACE_INCLUDE_DIRECTORIES>)
endif()
if(PROJ_FOUND)
    target_link_libraries(DmGeo PRIVATE PROJ::proj)
endif()
