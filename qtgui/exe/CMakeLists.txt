# GeoDmsGuiQt - Qt GUI application
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

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
