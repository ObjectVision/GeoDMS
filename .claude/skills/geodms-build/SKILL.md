---
name: geodms-build
description: Compiling GeoDMS on this shared working tree and running the per-change tests. Covers the one allowed incremental build (VS18 msbuild on all22.sln), what to check before starting because other sessions and Visual Studio use the same tree and the same bin folder, how to prove a build really ran, the test tiers from a headless probe up to the release suites, and what must never be done (single-project builds, edits to props files, improvised scripts). Use whenever an edit needs compiling or verifying, or when asked to build or to run tests.
---

# Building and testing on the shared tree

AGENTS.md holds the policy: build only through the committed solution or presets, never a
single project, never an improvised script. This skill is the operational side of that policy
on this machine, where `C:\dev\GeoDMS_2026` is one tree used at the same time by the user in
Visual Studio and by other agent sessions, all writing to the same `bin\<Config>\x64`.

## Build in `C:\dev\GeoDMS_2026`, NEVER inside a worktree

A session may be started in a git worktree under `.claude/worktrees/<name>\`. **Never build or
test there.** A worktree gets the tracked files only: no `vcpkg_installed`, and the `vcpkg`
submodule directory is empty. `tools\ensure-vcpkg.ps1` would take that as an unprovisioned tree
and bootstrap vcpkg plus every port from scratch — hours of compiling and gigabytes, into a
throwaway folder, while the real tree sits provisioned next door. There is also no second
`bin\<Config>\x64` worth having: the `Test*.bat` scripts, the GUI and the user's own workflow all
run out of the main checkout's.

So when the edits are in a worktree and they need compiling, do not build; carry them into
`C:\dev\GeoDMS_2026` first (`git diff > patch` + `git apply`, or merge the branch), after checking
the tree is quiet as below. Say so rather than building where you stand.

## Before you build: is the tree quiet

There is no lock. A second build interleaves writes into the same output folder, and a
running `GeoDmsRun` or `GeoDmsGuiQt` from `bin\Release\x64` holds a `Dm*.dll`, which turns
somebody's link step into a silent skip. Neither side gets an error that says so. So, right
before every build:

```powershell
Get-Process msbuild,cl,link,devenv,GeoDmsRun,GeoDmsGuiQt,cdb -ErrorAction SilentlyContinue |
  Select-Object Id,ProcessName,StartTime,Path
git diff --name-only
```

- Files in that diff you did not touch belong to someone else's work in progress. Their
  mtimes tell you whether that work is minutes or hours old.
- `MSBuild.exe /nodemode:1` processes with no children and no CPU are idle leftovers and may
  be killed (`taskkill /F /IM MSBuild.exe`); an msbuild without `/nodemode:` is a live build.
  `batch\BuildSignAndCreateSetup.bat` has the exact discrimination in a comment.
- A `GeoDmsGuiQt` from `bin\Release\x64` that is not yours: read its command line
  (`Get-CimInstance Win32_Process -Filter "Name='GeoDmsGuiQt.exe'"`), the config or log path
  names the issue it belongs to, and ask before killing it.
- If a build or a test is in flight, wait or ask. Never start a second one, whatever the
  configuration or flavour: the `DmClc`/`DmGeo` template units alone can exhaust RAM, and a
  test that reads DLLs while a link rewrites them produces garbage that looks like a
  regression in your own change (measured 2026-08-22: 209 of 209 testcases failing with exit
  0xC0000003 during someone else's link).
- If the user mentions running a setup script, finish or stop every build of yours first.

Build when the work needs it and you have checked this. Do not build speculatively, and do not
report results gathered while another build was running; rerun once the tree is quiet.

## The one incremental build

```powershell
Set-Location C:\dev\GeoDMS_2026
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
& $msbuild C:\dev\GeoDMS_2026\all22.sln -p:Configuration=Release -p:Platform=x64 -nologo -m -nr:false -v:minimal `
  > C:\dev\GeoDMS_2026\scratch\build_<topic>.log 2>&1
```

- VS18 (`\18\`) only. The VS2022 msbuild on this machine fails with MSB8020 on toolset v145.
- `-nr:false` always: without it the worker nodes stay resident for fifteen minutes and trip
  the setup script's concurrent-build guard hours later.
- Absolute paths for the solution and the log. The Bash and PowerShell tools share one
  working directory, and a `cd` in one silently moves the other; a redirect into a folder
  that does not exist kills the pipeline while the background task still reports exit 0.
- Long builds go in the background; a clean incremental build is a few minutes, a header
  change under `rtc` is much longer.
- `-p:Configuration=Debug` for the Debug build; same rules. GLOBIO uses
  `batch\BuildGlobio.bat Release` (its own `bin_GLOBIO`).
- Noise to ignore in the output: `MSB4011 GetGlobalProperties.task` import warnings,
  `'pwsh.exe' is not recognized` after a DLL is written, the `dxcompiler.dll`/`dxil.dll`
  windeployqt lines. A Debug build's QtDeploy step also fails now and then with `Cannot remove
  existing file ...\msvcp140_1d.dll: The process cannot access the file because it is being
  used by another process` and msbuild exits 1 (13 of the 20 Debug build logs in `scratch\` up
  to 2026-09-03); the exe was linked before that step, so check its mtime and carry on. The
  holder was not identified.

Prove the build ran before believing it: compare the mtime of `bin\Release\x64\GeoDmsRun.exe`
(and of the DLL your edit lives in) with the launch time. msbuild exits 0 on an up-to-date
no-op, and a background task exits 0 when its redirect failed. The setup script applies the
same staleness guard to itself.

After the build, hand the machine back clean: `taskkill /F /IM MSBuild.exe` for idle nodes,
and no `GeoDmsRun`, `cdb` or `WerFault` of yours parked on a dialog (see geodms-debug).

## Never

- `msbuild <module>\<proj>.vcxproj`, with or without `-p:SolutionDir`. It makes
  `$(SolutionDir)` the module's `dll\` folder and scatters `vcpkg_installed`, `vc_archives`
  and `vc_downloads` there (this is what contaminated `rtc\dll` twice).
- `ninja <target>`, `cmake --build --target <x>`: same rule for the CMake side.
- Editing `Directory.Build.props`, `DmsDef.props`, any `*.props`, `*.targets` or `.vcxproj`
  for build routing. A timestamp bump there invalidates the up-to-date check of every
  project and the user's next F5 rebuilds everything.
- `vcpkg integrate install`, a manual `vcpkg install`, a hand-cloned vcpkg. Provisioning is
  `tools\ensure-vcpkg.ps1` and `tools\vcpkg-toolchain.cmake`.
- Using `batch\BuildSignAndCreateSetup*.bat` as a dev build. Those are the release scripts;
  see geodms-release.
- Building or testing from a `.claude\worktrees\<name>\` checkout, whatever the tool. See the
  top of this skill: an unprovisioned tree makes vcpkg rebuild the world into a throwaway
  folder. Move the change to `C:\dev\GeoDMS_2026` and build there.

If the build cannot be run exactly this way, stop and ask.

## The other flavours

- CMake Windows: `cmake --preset windows-x64-release` (or `-debug`), then `cmake --build`
  with no target. Paths and the explicit toolchain overrides are in
  `doc/development/build-tips.md`. The setup script only configures when `CMakeCache.txt`
  is absent; a `CMakeLists.txt` edit forces the reconfigure.
- Linux: from WSL, `cd /mnt/c/dev/GeoDMS_2026`, presets `linux-x64-release` / `-debug`, same
  tree, no separate checkout. A file that WSL claims is missing over 9p may exist; verify
  from the Windows side before concluding anything.
- One build at a time across all of these.

## Which test proves what

| Tier | What | Cost | Proves |
|---|---|---|---|
| 0 | a headless `GeoDmsRun` probe with `@statistics` or an IntegrityCheck (geodms-debug) | seconds | the one thing you changed, on the data |
| 1 | `testcases\run_testcases.bat` | minutes, offline | the typed-function battery, ~240 configs, positives exit 0, `_neg` exit nonzero, exit 3 always fails |
| 2 | `batch\TestReleaseUnit.bat` / `TestDebugUnit.bat` (`.m`), `TestCMakeReleaseUnit.bat` / `TestCMakeDebugUnit.bat` (`.c`), `TestGlobioReleaseUnit.bat` / `TestGlobioDebugUnit.bat` (`.g`) | 5 to 15 min | the `tst` unit suite, plus tier 1 (`.m`, `.g`), plus (Release) the shipped-content test |
| 3 | `python full.py -version <installed>` in `C:\dev\tst\batch` | hours | the project regressions; the only thing that trips threading, stack-pressure and meta-thread bugs |

Rules per tier:

- Tier 1 runs from a normal tool call. Its logs land in `testcases\_out\`; per-case item
  overrides live in `testcases\fnrun_itemmap.txt`, default item `/checks`.
- Tier 2 must run in a visible console, never piped headless: the suite ends by launching
  Notepad++ on the result file and pausing, which hangs without a console. Register an
  interactive scheduled task (`schtasks /Create ... /IT` then `/Run`) and read the results
  from disk. Every launcher goes through `batch\run_unit_suite.bat`, which puts the `tst`
  batch folder on `PATH` (so `NoDefaultCurrentDirectoryInExePath` no longer skips the nested
  `unit.bat` call), exits 1 when no new aggregate appeared and 2 when the new aggregate lists
  `FAILED`; the launcher prints which of the two it was and exits 1 for either. The verdict is
  the aggregate `C:\LocalData\GeoDMSTestResults\unit\v<selector>.<flavour>_<stamp>.txt`
  (`vR64.off_`, `vGR64.g_`, `v20.19.1.g_`): it lists only failures plus two `python ... OK`
  lines, so a file of about 190 bytes is all green; grep the per-test files under `unit\` for
  `FAILED|Error`. `TestReleaseUnit.bat` and `TestGlobioReleaseUnit.bat` also run
  `batch\TestShippedContent.bat`, which downloads and rasterises real data; `[W] GEOS fix
  failed` lines there are expected, any `[E]` is not. `TestGlobioReleaseUnit.bat 20.19.1`
  tests the installed `GeoDms20.19.1.g` instead of `bin_GLOBIO\Release\x64`; that is how the
  `.g` setup script calls it and how a released `.g` is re-tested without a rebuild.
- Tier 3 only with the user's explicit consent for that run. Use the Python 3.13 under the
  user's profile, launch from a scheduled task so the run survives this session and the
  Claude app's self-update, never set `PYTHONUTF8`, quote `set "VAR=value"` in cmd
  one-liners, and resume an aborted round with a bare `-version` (a `-tests` argument wipes
  the caches). `C:\dev\tst\CLAUDE.md` has the rest.
- Anything that reaches the network or real source data belongs in `TestShippedContent.bat`,
  not in `testcases\`.

Report what ran and what it showed, including a failing count, in the commit body and the
issue debrief (geodms-commit, geodms-issues).
