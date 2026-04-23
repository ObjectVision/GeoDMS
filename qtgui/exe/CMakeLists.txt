# GeoDmsGuiQt - Qt GUI application
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/res/ui)

file(GLOB QTGUI_SOURCES src/*.cpp)
file(GLOB QTGUI_HEADERS src/*.h)
file(GLOB QTGUI_UI      res/ui/*.ui)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(GeoDmsGuiQt
    ${QTGUI_SOURCES}
    ${QTGUI_HEADERS}
    ${QTGUI_UI}
    GeoDmsGuiQt.qrc
)

target_include_directories(GeoDmsGuiQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}          # AUTOUIC generated ui_*.h
)

target_link_libraries(GeoDmsGuiQt PRIVATE
    DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo DmShv
    Qt6::Core Qt6::Gui Qt6::Widgets
)

if(WIN32)
    target_link_libraries(GeoDmsGuiQt PRIVATE Shcore shell32)
    set_target_properties(GeoDmsGuiQt PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Deploy Qt plugins alongside the executable so the build tree is self-contained.
# On Linux with a system Qt installation the plugins are found automatically via
# the system plugin path, but copying them here makes the bin/ dir deployable
# without a full Qt installation on the target machine.
if(NOT WIN32)
    get_target_property(_qt_bin_dir Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir ${_qt_bin_dir} DIRECTORY)
    get_filename_component(_qt_prefix  ${_qt_bin_dir} DIRECTORY)
    set(_qt_plugins ${_qt_prefix}/lib/qt6/plugins)
    if(NOT IS_DIRECTORY ${_qt_plugins})
        # fallback for distributions that put plugins directly under lib
        set(_qt_plugins ${_qt_prefix}/lib/x86_64-linux-gnu/qt6/plugins)
    endif()
    foreach(_plugin_dir platforms imageformats xcbglintegrations tls)
        if(IS_DIRECTORY ${_qt_plugins}/${_plugin_dir})
            file(COPY ${_qt_plugins}/${_plugin_dir}
                 DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        endif()
    endforeach()
endif()

# Deploy font files next to the executable
file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/res/fonts/dmstext.ttf
    DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/misc/fonts
)
if(EXISTS ${CMAKE_SOURCE_DIR}/res/fonts/dms.ttf)
    file(COPY
        ${CMAKE_SOURCE_DIR}/res/fonts/dms.ttf
        DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/misc/fonts
    )
endif()
