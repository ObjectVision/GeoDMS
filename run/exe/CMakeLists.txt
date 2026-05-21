# GeoDmsRun - command-line runner executable
add_executable(GeoDmsRun src/MainRun.cpp)

target_include_directories(GeoDmsRun PRIVATE src)

target_link_libraries(GeoDmsRun PRIVATE
    DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo
)

if(WIN32)
    set_target_properties(GeoDmsRun PROPERTIES WIN32_EXECUTABLE FALSE)
endif()
