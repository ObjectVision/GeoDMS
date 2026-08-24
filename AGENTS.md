# GeoDMS — Codex project instructions and repo notes

## Scratch files go in `scratch/`, never in the repo root

Diagnostic artifacts that must live inside the repo tree — cdb stack dumps, single-test
run logs, instrumentation output, watcher outputs, redirected tool output — go in the
gitignored **`scratch/`** folder at the repo root (create it if absent), NOT loose in the
repo root. Purely session-local temp files should use the session scratchpad directory
outside the repo instead. The repo root once accumulated 51 loose `cdb_*.txt`/`*.out`
diag files; don't let that happen again.

**The typed-HOF regression suite is git-tracked in `testcases/`** (the `fn_test*.dms`
battery + a few `fn_probe_*` probes + `tmpl_regress.dms`, with the `fe_names.txt`/
`impspec.txt` fixtures and `fnrun_itemmap.txt`). Run it via **`testcases/run_testcases.bat`**
(defaults to `bin\Release\x64\GeoDmsRun.exe`; classifies positives=exit-0,
`_neg`/`defcheck`=exit-nonzero, exit-3 assert=always-fail). Run artifacts land in the
gitignored `testcases/_out*/`. One-off investigation configs (controls, repros) still
belong in gitignored `scratch/`, not `testcases/`.

**That battery must stay offline and cheap.** Anything that reaches the network or
processes real source data belongs in **`batch\TestShippedContent.bat`** instead, the
release test for the `.dms` content the installer ships (issue #1031). It runs against
`bin\<Config>\x64\` — the copies NSIS packages, not the source tree beside them — in two
steps: the shipped `examples\testcases` battery through its own `run_testcases.bat`, and
`examples\grid_to_polygon.dms` over the real CBS buurt map, with the CBS geopackage
renamed to `.bak` first so `RegioIndelingen.dms` has to download it again. Both
`batch\TestReleaseUnit.bat` and `batch\TestCMakeReleaseUnit.bat` call it, so each flavour
walks its own output folder. Pass a third argument to point the geopackage step at a
scratch `SourceDataDir` when testing the script itself.

## Build & setup policy — do NOT improvise

Build ONLY through the committed solution / preset files, using **msbuild** or **CMake**.
Never invent custom build, setup, or bootstrap steps, and never build to "save time" in a
way the solution/presets don't define.

**For testing a new feature, always use msbuild as the preferred build tool** (the `.m`
flavour — see "Codex CLI msbuild recipe" below). It gives the fastest incremental turnaround
into `bin\Release\x64`.

- **Windows (msbuild):** use the **MSVC 18 (VS18) msbuild**, never the VS2022 one — the repo
  builds with PlatformToolset **v145** and the VS2022 msbuild fails with `MSB8020: build tools
  for v145 cannot be found`. Use:
  `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`.
  Build the solution **`all22.sln`**, or run
  `batch\BuildSignAndCreateSetup.bat` (or `Build.bat` at the repo root to do all four
  flavours m/c/g/l in sequence). Do **not** build individual `*.vcxproj` projects standalone
  (e.g. `DmRtc.vcxproj`, `Clc.vcxproj`). A standalone project build makes `$(SolutionDir)`
  resolve to that module's `dll\` folder, which scatters `vcpkg_installed`, `vc_archives`, and
  `vc_downloads` into the module folder (this is what contaminated `rtc\dll`). All vcpkg
  caches must stay at the **repo root**.
- **Windows GLOBIO compatibility (`.g`):** use `batch\BuildGlobio.bat Debug` or
  `batch\BuildGlobio.bat Release`; use `batch\BuildSignAndCreateSetupGlobio.bat`
  for the release setup. These still build `all22.sln`, with the committed
  `GeoDmsGlobio=true` switch. Outputs go to `bin_GLOBIO\<Config>\x64`, objects to
  `obj_GLOBIO`, and non-spatial vcpkg packages to `vcpkg_installed_GLOBIO` from
  `vcpkg-globio\vcpkg.json`. `GLOBIO_ENV_ROOT` must name the exact conda prefix
  locked by `vcpkg-globio\environment.yml`; never point a G build at the regular
  `vcpkg_installed` spatial stack.
- **Windows (CMake):** use the committed presets —
  `cmake --preset windows-x64-release` (or `windows-x64-debug`), then `cmake --build`. Or run
  `batch\BuildSignAndCreateSetupCmake.bat`.
- **Linux (CMake):** use presets `linux-x64-release` / `linux-x64-debug`, or run
  `batch\BuildSignAndCreateSetupLinux.bat`.
- **The per-flavor setup scripts now live in `batch\`.** The repo root holds a single
  `Build.bat` that calls the m/c/g/l `batch\BuildSignAndCreateSetup*.bat` scripts sequentially; the
  unit-test launchers (`Test*Unit.bat`, `RunGUITests.bat`, `run_unit.bat`) also moved to
  `batch\`. Each moved script re-roots to the repo root via `cd /d "%~dp0.."`, so it still
  works whether run from `batch\` or via `Build.bat`. `GeoDmsVersion.cmd` stays at the root.
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

### Build and test ONLY through the committed scripts — and never headless

Use `batch\*.bat` for building and testing. They set the environment the steps depend on and run
them in the right order; hand-rolled equivalents skip or misconfigure something. Two failures seen
in practice from invoking the pieces directly instead of the launcher:

- `unit.bat` called on its own tests the WRONG binaries. `generic\SetGeoDMSPlatform.bat` defaults
  `geodms_rootdir` to `C:\dev\GeoDMS` when unset, so every test silently runs a nonexistent exe.
  `batch\TestReleaseUnit.bat` sets it. Check the `Testing <path>` line at the top of the output.
- `batch\TestReleaseUnit.bat` exits **0** even when the whole unit portion never ran. If the shell
  sets `NoDefaultCurrentDirectoryInExePath=1`, cmd cannot resolve `unit.bat` from the current
  directory and prints `'unit.bat' is not recognized`; only the testcases battery then runs. Clear
  that variable in the parent process first — setting it inside cmd is too late.

**Do not run these scripts headless.** Piping them through `cmd /c ... | Tee-Object` leaves them
without a console: the script's `timeout /T` steps fail with
`ERROR: Input redirection is not supported`, nothing is visible on screen, and GUI tests pop up
with no context around them. Run them in a real console window — see below.

### Running a build or test script with live progress on screen *and* readable by Codex

The `batch\BuildSignAndCreateSetup{,Cmake,Globio,Linux}.bat` scripts (and the `Build.bat` wrapper), and the
`batch\Test*Unit.bat` / `RunGUITests.bat` test launchers, are run by the **user** in their own
interactive PowerShell (so they inherit the user's environment and don't trigger a clean-env
vcpkg re-bootstrap). To let the user watch live progress **and** let Codex read the output,
pipe the script through `Tee-Object`:

```powershell
& cmd /c "batch\BuildSignAndCreateSetupLinux.bat 2>&1" | Tee-Object -FilePath build_linux_<ver>.log
```

- `cmd /c "... 2>&1"` merges **stderr** (where GCC/clang warnings + errors land) into stdout
  *inside cmd*, avoiding Windows PowerShell 5.1 wrapping native stderr lines as
  `NativeCommandError` noise.
- `Tee-Object` shows live output on the user's console and writes a logfile Codex tails
  (`wc -l`, `Read` with offset, `Select-String 'warning:|error:'`).
- Swap the `.bat` name and log name per flavor: `build_{m,c,l}_<ver>.log`. Codex does **not**
  launch these scripts itself — it only reads the teed log. The `.m`/`.c` Windows setups still
  need the user to drive CHOICE prompts + the SafeNet PIN at the console.


## Running the freshly built GeoDmsGuiQt.exe

An incremental build with msbuild drops the GUI at:

```
C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe
```
or
```
C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe
```

**For grabbing user control, always launch it with a specific config `.dms` file as a command-line argument:**

```powershell
& 'C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe' '<path>\some_config.dms'
```

Why the explicit config matters:

- Launched **without** a config argument, the app comes up with an **empty project** (since the
  #1162 change; the old "Reopen last configuration?" confirmation dialog that used to block
  headless drivers at the splash screen is gone) — so nothing is loaded and the caption carries no
  config name. Passing the config is what makes the run reproducible.
- Passing a config makes it load that file directly and bring up the main window with a
  **recognisable caption** (`<config>.dms (aka ...) in <path> - GeoDms <ver> ...`), which is
  how you confirm you're driving the right instance.

```powershell
Get-Process GeoDmsGuiQt -ErrorAction SilentlyContinue | Stop-Process -Force
& 'C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe' '<path>\some_config.dms'
```

For headless feature verification (capture the exact command an action launches, drive
test-script verbs) use the `/L<logfile> /T<scriptfile> <config>` form instead of clicking.


## Documenting a behaviour change: the wiki

The user-facing documentation is the **GitHub wiki**, checked out beside this repo as
**`C:\dev\GeoDMS.wiki`** (`ObjectVision/GeoDMS.wiki.git`; `GeoDMS_Academy.wiki` is a separate
one). It is a normal git clone: edit the `.md` pages, commit locally, **never push** (same rule
as every repo here).

**Always `git pull` the wiki before editing a page.** Other people write to the same `main`
branch from other machines, so an edit made on a stale checkout turns into a merge conflict that
someone has to resolve by hand — including conflict markers landing inside a page. Pull first,
then edit.

**A change that alters observable behaviour is not finished when it builds and the tests pass.**
Semantics that a modeller can notice — a new or changed notation, a property that starts warning
or erroring, a check that now fires where it did not, a rule about what a storage records or what
a reader may declare — belongs on the wiki in the same session, next to the code and the issue
debrief. Verifying a change tells you it works; the wiki is what makes it usable.

Conventions that the existing pages follow, worth matching:

- **Date the change**: "**Since GeoDMS 20.14.0** …", so a reader on an older build knows why the
  page and their build disagree. When the old behaviour was wrong rather than merely different,
  say what it was — the pages are read by people debugging configurations written years ago.
- **Say what breaks.** Where a change makes a previously silent configuration fail, name that:
  the IntegrityCheck page states that a check which never fired before can surface on the first
  run after upgrading, which is the whole point of the change but still a surprise.
- **Put it on the topic page**, not in a changelog: `IntegrityCheck.md`, `MMD.md`, `XY-order.md`,
  `Indirect-expression.md`, … Link between them with the wiki's own `[[Page-name]]` /
  `[[label|Page-name]]` syntax, and add the reverse link on the pages that should point back.
- **Keep the issue debrief and the wiki distinct**: the issue records what was wrong, how it was
  diagnosed and what was measured; the wiki records only what a modeller must now do differently.

## Build & headless-run gotchas

- **`'pwsh.exe' is not recognized` post-build line is noise.** A post-build event shells out to
  `pwsh.exe` (PowerShell 7), which may be absent (only Windows PowerShell `powershell.exe` is
  installed). When missing you'll see `'pwsh.exe' is not recognized as an internal or external
  command` after `… -> …\Geo.dll`, but msbuild still succeeds and the DLL is already written to
  its `OutDir` (`bin\<Config>\x64`). Treat the line as noise, not a build failure. (Install
  PowerShell 7 to silence it.)

- **`GeoDmsRun.exe` item paths are relative to the desktop root, and the config's top-level
  container *is* that root.** For `container foo { … export { … } }`, compute `/export`, not
  `/foo/export` — the wrapping container's name is not part of the path. A wrong prefix reports
  `the specified item '/foo/export' was not found`.

- **Debug-build assertions pop a modal `abort()` dialog that hangs headless runs.** A failed
  assertion prints `Assertion failed: <cond>, file …, line …` then `abort() has been called` and
  blocks on a Retry/Ignore dialog; a headless `GeoDmsRun` then hangs forever and keeps a handle
  on the build's `Dm*.dll` (which silently turns the next link into a skip — see the
  `batch\BuildSignAndCreateSetup.bat` guard). To capture the assertion text + stack non-interactively,
  run under cdb and always kill stray `GeoDmsRun`/`cdb` before rebuilding:
  ```powershell
  & 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe' -c 'g;kn;q' `
      'C:\dev\GeoDMS26\bin\Debug\x64\GeoDmsRun.exe' /L<logfile> <config>.dms /item/path
  ```
