# Stg - Storage library
file(GLOB_RECURSE STG_SOURCES src/*.cpp)
file(GLOB_RECURSE STG_HEADERS src/*.h)

add_library(DmStg SHARED ${STG_SOURCES} ${STG_HEADERS})

target_include_directories(DmStg
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmStg PRIVATE DMSTGDLL_EXPORTS)

target_link_libraries(DmStg PUBLIC DmRtc DmTic)
target_link_libraries(DmStg PRIVATE GDAL::GDAL)

if(WIN32)
    target_link_libraries(DmStg PRIVATE odbc32 odbccp32)
endif()
