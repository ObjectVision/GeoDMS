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
find_package(OpenSSL REQUIRED)
find_package(GDAL)
if(GDAL_FOUND)
    target_include_directories(DmShv SYSTEM PRIVATE ${GDAL_INCLUDE_DIRS})
endif()
target_link_libraries(DmShv PUBLIC
    DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo
    Qt6::Core Qt6::Gui
    OpenSSL::SSL OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(DmShv PRIVATE Shcore comctl32 msimg32)
    # ShvDLL.vcxproj uses CharacterSet=MultiByte with UNICODE/_UNICODE undefined.
    # DmShv uses narrow-char Win32 APIs (strncpy into WCHAR[], CreateWindowEx with
    # char literals, CreateFont with char literal). Match that setting here.
    target_compile_options(DmShv PRIVATE /UUNICODE /U_UNICODE)
endif()
