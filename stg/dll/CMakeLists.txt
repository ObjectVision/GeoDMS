# Stg - Storage library
# Source list matches StgDll.vcxproj
# Excluded: cfs/CompoundStorageManager.cpp (Windows COM/OLE Structured Storage)
#           e00/ sources (not used)
set(STG_SOURCES
    src/bmp/BmpImp.cpp
    src/bmp/BmpStorageManager.cpp
    src/dbf/dbfImp.cpp
    src/dbf/dbfStorageManager.cpp
    src/DllMain.cpp
    src/fss/FileSystemStorageManager.cpp
    src/gdal/gdal_base.cpp
    src/gdal/gdal_grid.cpp
    src/gdal/gdal_vect.cpp
    src/GridStoragemanager.cpp
    src/ImplMain.cpp
    src/odbc/OdbcImp.cpp
    src/odbc/OdbcStorageManager.cpp
    src/shp/ShpImp.cpp
    src/shp/ShpStorageManager.cpp
    src/str/StrStorageManager.cpp
    src/tif/TifImp.cpp
    src/tif/TifStorageManager.cpp
    src/xdb/XdbImp.cpp
    src/xdb/XdbStorageManager.cpp
)
file(GLOB_RECURSE STG_HEADERS src/*.h)

add_library(DmStg SHARED ${STG_SOURCES} ${STG_HEADERS})

target_include_directories(DmStg
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/e00
            ${CMAKE_CURRENT_SOURCE_DIR}/src/bmp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/shp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/tif
            ${CMAKE_CURRENT_SOURCE_DIR}/src/dbf
            ${CMAKE_CURRENT_SOURCE_DIR}/src/xdb
            ${CMAKE_CURRENT_SOURCE_DIR}/src/odbc
            ${CMAKE_CURRENT_SOURCE_DIR}/src/str
            ${CMAKE_CURRENT_SOURCE_DIR}/src/fss
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cfs
            ${CMAKE_CURRENT_SOURCE_DIR}/src/gdal
)

target_compile_definitions(DmStg PRIVATE DMSTGDLL_EXPORTS)

find_package(GDAL)
find_package(TIFF)
find_package(PROJ CONFIG)
target_link_libraries(DmStg PUBLIC DmRtc DmTic)
if(GDAL_FOUND)
    target_link_libraries(DmStg PRIVATE GDAL::GDAL)
else()
    message(WARNING "GDAL not found - DmStg will not link against GDAL")
endif()
if(TIFF_FOUND)
    target_link_libraries(DmStg PRIVATE TIFF::TIFF)
endif()
if(PROJ_FOUND)
    target_link_libraries(DmStg PRIVATE PROJ::proj)
endif()

if(WIN32)
    target_link_libraries(DmStg PRIVATE odbc32 odbccp32 ole32)
else()
    target_link_libraries(DmStg PRIVATE odbc)
    # Hide static libjpeg/libtiff/libpng symbols from leaking into the global namespace.
    # GDAL links these statically; without this, their symbols shadow the system versions
    # used by Qt's image plugins (libqjpeg.so), causing ABI mismatches at runtime.
    target_link_options(DmStg PRIVATE
        -Wl,--exclude-libs,libjpeg.a
        -Wl,--exclude-libs,libtiff.a
        -Wl,--exclude-libs,libpng.a
        -Wl,--exclude-libs,libpng16.a
    )
endif()
