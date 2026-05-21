# GeoDmsRun - command-line runner executable
add_executable(GeoDmsRun src/MainRun.cpp)

target_include_directories(GeoDmsRun PRIVATE src)

target_link_libraries(GeoDmsRun PRIVATE
    DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo
)

if(WIN32)
    set_target_properties(GeoDmsRun PROPERTIES WIN32_EXECUTABLE FALSE)
    # Match the msbuild build's stack-reserve setting (DmsDef.props sets
    # StackReserveSize=67108864 / 64 MB). Iterated calculation steps can
    # produce thousands-deep AST/rewrite chains in SubstituteExpr_impl and
    # ApplyTopEnv (Lispeval); until those are iterative (tasks C1b, H1,
    # H3) the MSVC 1 MB default overflows. See also #1102.
    target_link_options(GeoDmsRun PRIVATE "/STACK:67108864,8192")
elseif(UNIX AND NOT APPLE)
    # Linux parity with the 64 MB Windows stack reserve. The PT_GNU_STACK
    # ELF segment size sets the main-thread stack hint for the kernel
    # loader; modern glibc honours this. Worker threads (PPL task_group /
    # std::async) inherit RLIMIT_STACK at creation time -- for full
    # parity, main() would also need setrlimit(RLIMIT_STACK, ...) before
    # any thread spawn.
    target_link_options(GeoDmsRun PRIVATE "-Wl,-z,stack-size=67108864")
endif()
