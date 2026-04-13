# Shv - Shared Visualization library
file(GLOB SHV_SOURCES src/*.cpp)
file(GLOB SHV_HEADERS src/*.h)

# Win32-only sources: exclude on other platforms
if(NOT WIN32)
    list(FILTER SHV_SOURCES EXCLUDE REGEX "(Win32ViewHost|GdiDrawContext|DcHandle)\\.cpp$")
endif()

add_library(DmShv SHARED ${SHV_SOURCES} ${SHV_HEADERS})

target_include_directories(DmShv
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmShv PRIVATE DM_SHV_EXPORTS)

# Qt6 for QRegion, QRect, QPoint (core + gui only, no widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
target_link_libraries(DmShv PUBLIC
    DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo
    Qt6::Core Qt6::Gui
)

if(WIN32)
    target_link_libraries(DmShv PRIVATE Shcore comctl32 msimg32)
endif()
