# Rtc - Runtime Core library (merged DLL: rtc + sym + tic)
file(GLOB_RECURSE RTC_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE RTC_HEADERS CONFIGURE_DEPENDS src/*.h src/*.hpp src/*.ipp)
file(GLOB_RECURSE SYM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../sym/dll/src/*.cpp)
file(GLOB_RECURSE SYM_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../sym/dll/src/*.h)
file(GLOB_RECURSE TIC_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../tic/dll/src/*.cpp)
file(GLOB_RECURSE TIC_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../tic/dll/src/*.h)

add_library(DmRtc SHARED ${RTC_SOURCES} ${RTC_HEADERS} ${SYM_SOURCES} ${SYM_HEADERS} ${TIC_SOURCES} ${TIC_HEADERS})
target_precompile_headers(DmRtc PRIVATE src/RtcPCH.h)
# sym/tic units include their own SymPCH.h/TicPCH.h first; don't force-include RtcPCH.h into them
set_source_files_properties(${SYM_SOURCES} ${TIC_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_include_directories(DmRtc
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../sym/dll/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../tic/dll/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../../sym/dll/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../../tic/dll/src
)

target_compile_definitions(DmRtc PRIVATE DMRTC_EXPORTS DMSYM_EXPORTS DMTIC_EXPORTS)

target_link_libraries(DmRtc PUBLIC Boost::boost)

if(WIN32)
    target_link_libraries(DmRtc PRIVATE psapi)
else()
    target_link_libraries(DmRtc PRIVATE dl pthread)
endif()
