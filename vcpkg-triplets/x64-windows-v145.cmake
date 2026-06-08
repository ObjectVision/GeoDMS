set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PLATFORM_TOOLSET v145)
# WORKAROUND: MSVC 14.51.36231 miscompiles geos (drops a loop null-check -> AV in buffer/snap-rounding).
# Pin deps to 14.50.35717 (verified correct). Keep in sync with VCToolsVersion in Directory.Build.props.
# Remove once Microsoft fixes 14.5x. See memory project_geos_buffer_crash_2026_06.
set(VCPKG_PLATFORM_TOOLSET_VERSION 14.50.35717)