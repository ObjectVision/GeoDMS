# Shv - Shared Visualization library
file(GLOB SHV_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB SHV_HEADERS CONFIGURE_DEPENDS src/*.h)

# Win32-only sources: exclude on other platforms
if(NOT WIN32)
    list(FILTER SHV_SOURCES EXCLUDE REGEX "(Win32ViewHost|GdiDrawContext|DcHandle)\\.cpp$")
endif()

add_library(DmShv SHARED ${SHV_SOURCES} ${SHV_HEADERS})

target_include_directories(DmShv
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# DM_SHV_EXPORTS is defined by the DLL's PCH (src/ShvDllPch.h). Defining it here as
# well caused C4005 (macro redefinition) in every TU; the .vcxproj relies on the PCH
# alone, so we do the same.

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
    # DmShv is now a Unicode module (ShvDLL.vcxproj CharacterSet=Unicode): all its
    # string-carrying Win32 calls use the explicit -W variants (TextOutW/DrawTextW,
    # CreateFontW/CreateFontIndirectW, CreateWindowExW), so it inherits UNICODE/_UNICODE
    # from Qt without the old /U override (which also removes the D9025 warning).
endif()
