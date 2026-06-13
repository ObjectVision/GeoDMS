# GeoDMS26 — Claude project instructions

## Build & setup policy — do NOT improvise

Build ONLY through the committed solution / preset files, using **msbuild** or **CMake**.
Never invent custom build, setup, or bootstrap steps, and never build to "save time" in a
way the solution/presets don't define.

- **Windows (msbuild):** build the solution **`all22.sln`**, or run
  `BuildSignAndCreateSetup.bat`. Do **not** build individual `*.vcxproj` projects standalone
  (e.g. `DmTic.vcxproj`, `DmRtc.vcxproj`). A standalone project build makes `$(SolutionDir)`
  resolve to that module's `dll\` folder, which scatters `vcpkg_installed`, `vc_archives`, and
  `vc_downloads` into the module folder (this is what contaminated `rtc\dll`). All vcpkg
  caches must stay at the **repo root**.
- **Windows (CMake):** use the committed presets —
  `cmake --preset windows-x64-release` (or `windows-x64-debug`), then `cmake --build`. Or run
  `BuildSignAndCreateSetupCmake.bat`.
- **Linux (CMake):** use presets `linux-x64-release` / `linux-x64-debug`, or run
  `BuildSignAndCreateSetupLinux.bat`.
- **No partial / incremental single-project builds.** Build per the solution or presets only.
- **No improvised setup.** vcpkg is auto-provisioned by `tools/ensure-vcpkg.ps1` (msbuild
  hook) and `tools/vcpkg-toolchain.cmake` (CMake). Never manually `git clone` / bootstrap
  vcpkg or run `vcpkg install` by hand.
- **Do not change the toolset/triplet.** Compiler is pinned to MSVC **14.50.35717**
  (`Directory.Build.props` `VCToolsVersion`; triplet `x64-windows-v145`) — 14.51 miscompiles
  geos.

If a build cannot be run exactly as above, **stop and ask** — do not work around it.
