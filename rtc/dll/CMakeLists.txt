# Rtc - Runtime Core library
file(GLOB_RECURSE RTC_SOURCES src/*.cpp)
file(GLOB_RECURSE RTC_HEADERS src/*.h src/*.hpp src/*.ipp)

add_library(DmRtc SHARED ${RTC_SOURCES} ${RTC_HEADERS})

target_include_directories(DmRtc
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DmRtc PRIVATE DMRTC_EXPORTS)

target_link_libraries(DmRtc PUBLIC Boost::boost)
find_package(Boost REQUIRED COMPONENTS locale)
target_link_libraries(DmRtc PRIVATE Boost::locale)

if(WIN32)
    target_link_libraries(DmRtc PRIVATE psapi)
else()
    target_link_libraries(DmRtc PRIVATE dl pthread)
endif()
