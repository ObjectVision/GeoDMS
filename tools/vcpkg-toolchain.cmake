# Wrapper toolchain that auto-provisions the in-repo ./vcpkg submodule before
# chaining to vcpkg's real toolchain. This is the CMake counterpart of the
# MSBuild EnsureVcpkgBootstrapped target in Directory.Build.targets: it lets a
# fresh "git clone + cmake --preset ..." build succeed with no manual submodule
# checkout / bootstrap step. The toolchain file is processed before any
# CMakeLists.txt, which is the only point early enough — the vcpkg toolchain runs
# the manifest install (needing vcpkg.exe) during this stage.
#
# On Windows it reuses tools/ensure-vcpkg.ps1 (the same script the MSBuild hook
# calls); on POSIX it performs the equivalent submodule-checkout + bootstrap.
#
# Point CMAKE_TOOLCHAIN_FILE here (see CMakePresets.json), not directly at
# vcpkg/scripts/buildsystems/vcpkg.cmake.

get_filename_component(_geodms_repo_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_geodms_vcpkg_dir "${_geodms_repo_root}/vcpkg")

if(CMAKE_HOST_WIN32)
  set(_geodms_vcpkg_exe "${_geodms_vcpkg_dir}/vcpkg.exe")
else()
  set(_geodms_vcpkg_exe "${_geodms_vcpkg_dir}/vcpkg")
endif()

# Idempotent: once vcpkg.exe exists (normal case, and after the first configure)
# this whole block is skipped. The toolchain file is re-included several times
# during a configure; the EXISTS guard makes those subsequent passes no-ops.
if(NOT EXISTS "${_geodms_vcpkg_exe}")
  message(STATUS "vcpkg-toolchain: bootstrapping in-repo vcpkg (${_geodms_vcpkg_dir}) — one-time setup for this checkout...")
  if(CMAKE_HOST_WIN32)
    execute_process(
      COMMAND powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass
              -File "${_geodms_repo_root}/tools/ensure-vcpkg.ps1"
      WORKING_DIRECTORY "${_geodms_repo_root}"
      RESULT_VARIABLE _geodms_rc)
  else()
    # POSIX: mirror ensure-vcpkg.ps1 — check the submodule out if its working tree
    # is empty, then bootstrap.
    if(NOT EXISTS "${_geodms_vcpkg_dir}/bootstrap-vcpkg.sh")
      execute_process(
        COMMAND git -C "${_geodms_repo_root}" submodule update --init vcpkg
        RESULT_VARIABLE _geodms_rc)
      if(NOT _geodms_rc EQUAL 0)
        message(FATAL_ERROR "vcpkg-toolchain: 'git submodule update --init vcpkg' failed (${_geodms_rc})")
      endif()
    endif()
    execute_process(
      COMMAND "${_geodms_vcpkg_dir}/bootstrap-vcpkg.sh" -disableMetrics
      WORKING_DIRECTORY "${_geodms_repo_root}"
      RESULT_VARIABLE _geodms_rc)
  endif()
  if(NOT _geodms_rc EQUAL 0)
    message(FATAL_ERROR "vcpkg-toolchain: vcpkg bootstrap failed (${_geodms_rc})")
  endif()
  if(NOT EXISTS "${_geodms_vcpkg_exe}")
    message(FATAL_ERROR "vcpkg-toolchain: bootstrap completed but ${_geodms_vcpkg_exe} is still missing")
  endif()
endif()

include("${_geodms_vcpkg_dir}/scripts/buildsystems/vcpkg.cmake")
