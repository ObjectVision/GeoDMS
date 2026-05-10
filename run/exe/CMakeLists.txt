# GeoDmsRun - command-line runner executable
add_executable(GeoDmsRun src/MainRun.cpp)

target_include_directories(GeoDmsRun PRIVATE src)

target_link_libraries(GeoDmsRun PRIVATE
    DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo
)

if(WIN32)
    set_target_properties(GeoDmsRun PROPERTIES WIN32_EXECUTABLE FALSE)
    # Match the msbuild build's stack-reserve setting (DmsDef.props sets
    # StackReserveSize=67108864 / 64 MB). Without this, the cmake binary
    # gets the MSVC default of 1 MB which makes the meta-thread baton
    # transfer (RemainingStackSpace <= 327680, see #1102) fire ~64x more
    # readily than the msbuild binary on the same cfg.
    target_link_options(GeoDmsRun PRIVATE "/STACK:67108864,8192")
endif()
