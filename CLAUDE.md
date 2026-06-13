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
- **The vcpkg MSBuild integration is project-local — never run `vcpkg integrate install`.**
  `Directory.Build.props` (props, early) and `Directory.Build.targets` (targets, late) import
  vcpkg's `vcpkg.props`/`vcpkg.targets` from the in-repo `./vcpkg` submodule via `$(VcpkgRoot)`.
  If MSBuild stops seeing vcpkg include/lib paths ("Cannot open `boost/format.hpp`", unresolved
  `boost_locale`, …), do **not** "fix" it with `vcpkg integrate install` — that reintroduces a
  machine-wide dependency that silently breaks when the registered Visual Studio is uninstalled
  (the original failure). Instead check those two imports and that `./vcpkg/vcpkg.exe` is
  bootstrapped. CMake is already self-contained via `tools/vcpkg-toolchain.cmake`.
- **Do not change the toolset/triplet.** Compiler is pinned to MSVC **14.50.35717**
  (`Directory.Build.props` `VCToolsVersion`; triplet `x64-windows-v145`) — 14.51 miscompiles
  geos.

If a build cannot be run exactly as above, **stop and ask** — do not work around it.
# GeoDMS — repo notes for Claude


## Running the freshly built GeoDmsGuiQt.exe

An incremental build drops the GUI at:

```
C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe
```
or
```
C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe
```

**Always launch it with a specific config `.dms` file as a command-line argument:**

```powershell
& 'C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe' '<path>\some_config.dms'
```

Why the explicit config matters:

- Launched **without** a config argument, the app stops at the splash screen on a protective
  "Reopen last configuration?" confirmation dialog and does **not** show the main window until
  someone clicks Yes/No. Automated/headless drivers get stuck there.
- Passing a config makes it load that file directly and bring up the main window with a
  **recognisable caption** (`<config>.dms (aka ...) in <path> - GeoDms <ver> ...`), which is
  how you confirm you're driving the right instance — important when several `GeoDmsGuiQt.exe`
  processes are around (e.g. an old one still holding the last config).

Tip: kill any stale instances and launch exactly one fresh process so you know you're running
the just-built binary, not an older in-memory one:

```powershell
Get-Process GeoDmsGuiQt -ErrorAction SilentlyContinue | Stop-Process -Force
& 'C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe' '<path>\some_config.dms'
```

For headless feature verification (capture the exact command an action launches, drive
test-script verbs) use the `/L<logfile> /T<scriptfile> <config>` form instead of clicking.
