---
name: geodms-release
description: Producing GeoDMS setups and publishing them as a GitHub release. The four flavours (.m msbuild, .c cmake, .g GLOBIO, .l linux) and their assets in distr\, the version bump, the release notes in doc\release-notes\, why the user runs the canonical batch\BuildSignAndCreateSetup*.bat scripts in their own console while the agent prepares, tails and verifies, the release tests, the release-notes convention, and the gh release procedure with the immutable-release trap that burns a tag forever. Use for any request about a setup, an installer, a release, release notes, or publishing on GitHub.
---

# Building setups and publishing a release

## What a release is

The version lives in `rtc\dll\src\RtcVersionNumbers.h`; `GeoDmsVersion.cmd` parses it for
the scripts and `CMakeLists.txt` parses it independently. A bump is its own commit before
anything is built.

| Flavour | Script | Assets in `distr\` |
|---|---|---|
| `.m` Windows, msbuild | `batch\BuildSignAndCreateSetup.bat` | `GeoDms<ver>.m-Setup-x64.exe` |
| `.c` Windows, CMake | `batch\BuildSignAndCreateSetupCmake.bat` | `GeoDms<ver>.c-Setup-x64.exe` |
| `.g` Windows, GLOBIO stack | `batch\BuildSignAndCreateSetupGlobio.bat` | `GeoDms<ver>.g-Setup-x64.exe` |
| `.l` Linux, WSL Ubuntu 24.04 | `batch\BuildSignAndCreateSetupLinux.bat` | `GeoDms<ver>.l-linux-x64.deb`, `.tar.gz`, `.tar.gz.sha256`, `.tar.gz.sha256.p7s` |

`Build.bat` at the root runs the four in sequence. Each script builds, packages (NSIS or
`nsi\CreateLinuxSetup.sh`), signs with the SafeNet token, installs to
`C:\Program Files\ObjectVision\GeoDms<ver>.<flavour>` (or `/opt/ObjectVision/...` in WSL),
runs the `tst` unit suite against that install, and removes the install and the setup file
when the suite fails. The `.g` script does that through `batch\TestGlobioReleaseUnit.bat <ver>`
(#1231), which adds the testcases battery and the shipped-content test to the gate. That last
guard is why "all unit tests failed" after a setup run leaves only `examples`, `library` and
the uninstaller behind.

## Who runs what

The user runs the setup scripts, from their own interactive PowerShell. Reasons that have
each cost a day: the scripts inherit that shell's environment, and a clean environment (a
scheduled task, `Start-Process`, a synthesized console) makes vcpkg re-bootstrap and
reinstall; the `.m`, `.c` and `.g` scripts have CHOICE prompts and the signing PIN; and two
builds on one `bin\Release\x64` package a torn snapshot. Never launch one yourself, never run
your own build while one runs.

Never improvise around them either: no partial rerun of one stage (`makensis`, `signtool`,
the installer `/S`, `unit.bat`), no hand-copied files into `bin`, no wrapper that skips a
step, no edit that reorders one, no unsigned repackage. When a stage fails, fix the cause in
the committed script or project files and the whole script runs again; the build step is
incremental, so that is cheap. Advising a manual sub-step is as wrong as running it.

The agent prepares, watches and verifies.

## Pre-flight, by the agent

1. Version bumped and committed; wiki pages for the behaviour changes done (geodms-wiki);
   `git diff --name-only` shows nothing of yours uncommitted.
2. Tree quiet: no `msbuild`, `cl`, `link`, `devenv` at work; idle `MSBuild.exe` nodes killed
   (`taskkill /F /IM MSBuild.exe`, they hold no locks); no `GeoDmsRun` or `GeoDmsGuiQt`
   running from `bin\Release\x64` (the script aborts on that; tell the user to close it).
3. `testcases\run_testcases.bat` green on the current build, so the setup's unit gate does
   not fail on something cheap.
4. `.g` needs `GLOBIO_ENV_ROOT` pointing at the conda prefix from
   `vcpkg-globio\environment.yml`; `.l` needs the WSL distro up.
5. Hand the user the one-liner that keeps the output on their screen and in a log you can
   read:

```powershell
& cmd /c "batch\BuildSignAndCreateSetup.bat 2>&1" | Tee-Object -FilePath scratch\build_m_<ver>.log
```

`cmd /c "... 2>&1"` merges stderr inside cmd, so PowerShell 5.1 does not wrap warning lines
as errors; `Tee-Object` shows and records. One flavour at a time.

## While it runs

Tail the log (`Select-String 'error|warning|ABORT|FAILED'`). Things the scripts check for
themselves and report: a no-op build (`GeoDmsRun.exe` older than the script start), vcpkg
drift, the Qt deploy targets patch, the Python ABI modules, a concurrent MSBuild. Do not
"help" a CHOICE prompt along; the user answers it.

## After a flavour

- `Test-Path 'C:\Program Files\ObjectVision\GeoDms<ver>.<f>\GeoDmsGuiQt.exe'`
- `(Get-AuthenticodeSignature 'distr\GeoDms<ver>.<f>-Setup-x64.exe').Status` is `Valid`
- the unit aggregate `C:\LocalData\GeoDMSTestResults\unit\v<ver>.<f>_<stamp>.txt` lists no
  failures (about 190 bytes: header plus two `python ... OK` lines)
- `.g`: the launcher's summary at the end of the log reads `UNIT SUITE PASSED`,
  `TESTCASES BATTERY PASSED` and `SHIPPED CONTENT RELEASE TEST PASSED`
- `.l`: all four files in `distr\`, and the `.deb` installed in WSL

## Release tests

A publication is the one occasion for a full regression: `python full.py -version <ver>.<f>`
from `C:\dev\tst\batch`, per flavour, against the installed build. Ask before starting one;
it takes hours. Use the user's Python 3.13, start it from a scheduled task so it survives
this session and the Claude app's self-update, keep the console visible, clear
`NoDefaultCurrentDirectoryInExePath`, never set `PYTHONUTF8`, and read
`C:\dev\tst\CLAUDE.md` for the report and reference rules. A real regression is not hidden by
regenerating its reference from the regressing build.

## Release notes

Write `doc\release-notes\release-notes-<ver>.md` and commit it. They used to be written beside
the setups in `distr\`, which is line 1 of `.gitignore`, so "commit it" was impossible and no
notes file was ever in version control; the fourteen that existed moved to `doc\` in one go. The
baseline is the last published
GitHub release, not the last built version: `gh release list` shows it, and interim builds
in `distr\` are often never published (20.13 to 20.15 were built and skipped; 20.16.0's notes
covered everything since 20.12.0). Read the previous notes and the commits since that tag
(`git log v<prev>..HEAD --format='%h %s'`), and describe net-new work only, checked against
the previous notes when branches have diverged.

Format, as the existing files: an opening paragraph starting with **Pre-release.** or the
release status, naming the baseline and the headlines; a flavour table with one row per
flavour and its asset names (a flavour not shipped says "no (full release only)"); then `##`
sections by theme (Allocation, Polygons, Storages and export, IntegrityChecks, Charts and
views, Networks, Configuration language and diagnostics, Robustness, Packaging and build),
each bullet a bold lead-in, the issue number, what was wrong and what changed, in the voice
of the previous notes. Things a modeller must act on (an area that changes on upgrade, a
check that now fires) are said outright.

## Publishing on GitHub

`gh` is authenticated as the user. This repository has immutable releases: after publishing,
assets cannot be added, and deleting a published release does not free its tag name;
`v20.13.0` was burned this way and shipped as `v20.13.0-preview`. So:

1. Create the draft with every asset in the same call:

```
gh release create v<ver> --draft --title "GeoDms <ver>" --notes-file doc\release-notes\release-notes-<ver>.md ^
  [--target <full sha>] distr\GeoDms<ver>.m-Setup-x64.exe distr\GeoDms<ver>.c-Setup-x64.exe ^
  distr\GeoDms<ver>.g-Setup-x64.exe distr\GeoDms<ver>.l-linux-x64.deb distr\GeoDms<ver>.l-linux-x64.tar.gz ^
  distr\GeoDms<ver>.l-linux-x64.tar.gz.sha256 distr\GeoDms<ver>.l-linux-x64.tar.gz.sha256.p7s
```

   A draft creates no tag; it is safe to make and to inspect. Missing an asset here means a
   new draft, not an upload later.
2. `--target <full sha>` whenever the built commit is not the head of `main` on GitHub:
   without it the tag lands on `main` HEAD, the wrong commit. The commit must already be on
   origin (`git branch -r --contains <sha>`); if it is not, the user pushes it first. You
   never push.
3. Publishing is `gh release edit v<ver> --draft=false --prerelease` (or without
   `--prerelease` for a full release). That creates the tag on origin, an outward action:
   do it only on the user's explicit go-ahead for this release, after they have looked at
   the draft. A pre-release is not marked Latest.
4. Check `gh release view v<ver>` afterwards: title, target, assets, pre-release flag.

Never test what a release or asset operation does by writing placeholder text to a live
release; write only final content, and try mechanics on a draft.
