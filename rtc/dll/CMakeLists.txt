# Rtc - Runtime Core library (merged DLL: rtc + sym + tic)
file(GLOB_RECURSE RTC_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE RTC_HEADERS CONFIGURE_DEPENDS src/*.h src/*.hpp src/*.ipp)
# sym/dll/src and tic/dll/src now live under src/sym and src/tic, so the recursive
# RTC_SOURCES/RTC_HEADERS globs above already collect them. Gather the sym/tic .cpp
# separately ONLY to opt them out of RtcPCH.h below (they use their own SymPCH.h/TicPCH.h).
file(GLOB_RECURSE SYMTIC_SOURCES CONFIGURE_DEPENDS src/sym/*.cpp src/tic/*.cpp)

add_library(DmRtc SHARED ${RTC_SOURCES} ${RTC_HEADERS})
target_precompile_headers(DmRtc PRIVATE src/RtcPCH.h)
# sym/tic units include their own SymPCH.h/TicPCH.h first; don't force-include RtcPCH.h into them
set_source_files_properties(${SYMTIC_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_include_directories(DmRtc
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/sym>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/tic>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/sym
            ${CMAKE_CURRENT_SOURCE_DIR}/src/tic
)

target_compile_definitions(DmRtc PRIVATE DMRTC_EXPORTS DMSYM_EXPORTS DMTIC_EXPORTS)

# Debugger visualizers, moved here from tic/dll when the source tree was flattened.
# VS/MSVC-generator only; harmlessly ignored by the Ninja/GCC (.l) build.
if(MSVC)
    target_sources(DmRtc PRIVATE NatvisFile.natvis)
endif()

target_link_libraries(DmRtc PUBLIC Boost::boost)

if(WIN32)
    target_link_libraries(DmRtc PRIVATE psapi)
else()
    target_link_libraries(DmRtc PRIVATE dl pthread)
endif()
