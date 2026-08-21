# tools/DeployResources.cmake
#
# Deploy GeoDMS runtime resource files into the binary output directory.
#
# Invoked at BUILD time (via `cmake -P` from the deploy_resources custom target
# in the root CMakeLists.txt), NOT only at configure time. That distinction is
# the whole point: the BuildSignAndCreateSetup{Cmake,Linux}.bat scripts wipe the
# bin\ output folder before each build (to drop obsolete binaries left over from
# renamed / removed components). A plain file(COPY) in CMakeLists runs only
# during `cmake` configure, which an incremental `cmake --build` skips -- so the
# wiped resources would never be restored, and the installer (plus the unit
# tests that run the freshly built binary) would fail on a missing
# RewriteExpr.lsp, gdaldata, library, ... Running the copies here, on every
# build, fixes that. file(COPY) preserves timestamps and skips unchanged files,
# so re-running it each build is cheap.
#
# Expected -D arguments:
#   SRC_DIR           = repo root                       (CMAKE_SOURCE_DIR)
#   RUNTIME_DIR       = binary output dir               (CMAKE_RUNTIME_OUTPUT_DIRECTORY)
#   VCPKG_SHARE       = <vcpkg>/<triplet>/share, or ""  (empty when not using vcpkg)
#   MSVC_RUNTIME_LIBS = MSVC CRT redist DLLs, or ""     (empty on non-MSVC builds)

# Expression-rewrite rules, required for expression parsing at runtime.
file(COPY ${SRC_DIR}/res/RewriteExpr.lsp DESTINATION ${RUNTIME_DIR})

# Typed standard prelude: replacements for retired rewrite rules (WP4.5);
# configs opt in with '#include <%exeDir%/prelude.dms>'.
file(COPY ${SRC_DIR}/res/prelude.dms DESTINATION ${RUNTIME_DIR})

# Microsoft C/C++ runtime, deployed app-locally so the binaries do NOT depend on
# whatever VC++ redistributable is installed on the target machine (#1186: an
# older system-wide msvcp140_atomic_wait.dll has no __std_calloc_crt, which
# arrow.dll imports, and GeoDmsGuiQt then refuses to start). The list is resolved
# at configure time by InstallRequiredSystemLibraries in the root CMakeLists.txt;
# it is empty for non-MSVC builds, which makes this loop a no-op on Linux.
foreach(_crt IN LISTS MSVC_RUNTIME_LIBS)
    if(EXISTS ${_crt})
        file(COPY ${_crt} DESTINATION ${RUNTIME_DIR})
    else()
        message(WARNING "DeployResources: MSVC runtime DLL not found: ${_crt}")
    endif()
endforeach()

# GDAL and PROJ geographic data files (needed at runtime by DmStg via GDAL/PROJ).
if(VCPKG_SHARE AND IS_DIRECTORY ${VCPKG_SHARE})
    if(IS_DIRECTORY ${VCPKG_SHARE}/gdal)
        file(COPY ${VCPKG_SHARE}/gdal/ DESTINATION ${RUNTIME_DIR}/gdaldata)
    endif()
    foreach(_proj_share proj proj4)
        if(IS_DIRECTORY ${VCPKG_SHARE}/${_proj_share})
            file(COPY ${VCPKG_SHARE}/${_proj_share}/ DESTINATION ${RUNTIME_DIR}/proj4data)
        endif()
    endforeach()
endif()

# DMS script library and example configurations, from the top-level layout --
# the same source CopyResources.vcxproj robocopies for the msbuild flavour, so
# .c and .m ship the same files (issue #1031).
#
# This used to copy from CopyResources/<dir>/ as well, an older duplicate of the
# same trees. That made the .c setup ship two files the .m setup did not --
# library/geometry/Grid2Poly_ipoint.dms and the examples/grid_to_vector.dms that
# includes it -- and the _ipoint copy predated 38b9c5a7 ("fixed filled lakes
# caused by geos ignoring inverted rings"), so .c shipped a template with a bug
# that had been fixed in the copy .m shipped. Those duplicates are gone.
foreach(_res_dir library examples)
    if(IS_DIRECTORY ${SRC_DIR}/${_res_dir})
        file(COPY ${SRC_DIR}/${_res_dir} DESTINATION ${RUNTIME_DIR})
    endif()
endforeach()

# The typed-function testcases battery ships as end-user example content under
# examples/testcases (run via run_testcases.bat against the installed
# GeoDmsRun). The gitignored _out* run-artifact folders stay local. The Linux
# setup (nsi/CreateLinuxSetup.sh) stages examples/ recursively, so the .deb and
# .tar.gz packages inherit this copy.
file(COPY ${SRC_DIR}/testcases DESTINATION ${RUNTIME_DIR}/examples
     PATTERN "_out*" EXCLUDE)

# Python utility scripts.
foreach(_py_script profiler.py regression.py)
    if(EXISTS ${SRC_DIR}/profiler/${_py_script})
        file(COPY ${SRC_DIR}/profiler/${_py_script} DESTINATION ${RUNTIME_DIR})
    endif()
endforeach()
