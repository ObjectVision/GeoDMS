---
name: geodms-release
description: Producing GeoDMS setups and publishing them as a GitHub release. The four flavours (.m msbuild, .c cmake, .g GLOBIO, .l linux) and their assets in distr\, the version bump, the release notes in doc\release-notes\, why the user runs the canonical batch\BuildSignAndCreateSetup*.bat scripts in their own console while the agent prepares, tails and verifies, the release tests, the licence texts every setup installs and what to do when their step stops, the release-notes convention, and the gh release procedure with the immutable-release trap that burns a tag forever. Use for any request about a setup, an installer, a release, release notes, or publishing on GitHub.
---

# Building setups and publishing a release

## What a release is

The version lives in `rtc\dll\src\RtcVersionNumbers.h`; `GeoDmsVersion.cmd` parses it for
the scripts and `CMakeLists.txt` parses it independently. A bump is its own commit before
anything is built.

| Flavour | Script | Assets in `distr\` |
|---|---|---|
| `.m` Windows, msbuild | `batch\BuildSignAndCreateSetup.bat` | `GeoDms<ver>.m-Setup-x64.exe`, and the PyPI wheels `geodms-<ver>-cp3XY-cp3XY-win_amd64.whl`, one per CPython minor of `python\PythonVersions.txt` |
| `.c` Windows, CMake | `batch\BuildSignAndCreateSetupCmake.bat` | `GeoDms<ver>.c-Setup-x64.exe` |
| `.g` Windows, GLOBIO stack | `batch\BuildSignAndCreateSetupGlobio.bat` | `GeoDms<ver>.g-Setup-x64.exe`, and the PyPI wheel of the distribution geodms-globio, `geodms_globio-<ver>-cp39-cp39-win_amd64.whl` (the CPython minor of `python\PythonVersionsGlobio.txt`) |
| `.l` Linux, WSL Ubuntu 24.04 | `batch\BuildSignAndCreateSetupLinux.bat` | `GeoDms<ver>.l-linux-x64.deb`, `.tar.gz`, `.tar.gz.sha256`, `.tar.gz.sha256.p7s`, and the PyPI wheels `geodms-<ver>-cp3XY-cp3XY-manylinux_2_39_x86_64.whl`, one per CPython minor of `python\PythonVersions.txt` |

`Build.bat` at the root runs the four in sequence. Each script builds, packages (NSIS or
`nsi\CreateLinuxSetup.sh`), signs with the SafeNet token, installs to
`C:\Program Files\ObjectVision\GeoDms<ver>.<flavour>` (or `/opt/ObjectVision/...` in WSL),
runs the `tst` unit suite against that install, and removes the install and the setup file
when the suite fails. The `.g` script does that through `batch\TestGlobioReleaseUnit.bat <ver>`
(#1231), which adds the testcases battery and the shipped-content test to the gate; the `.m`
and `.c` scripts call `batch\run_unit_suite.bat <ver> m|c "<install dir>"` (since the audit of
2026-09-27, BAT-A09), which, like the `.g` gate, fails when no new aggregate appeared, so a
rerun for the same version can no longer pass on the previous run's aggregate. The Linux
script's gate still runs `unit_linux.sh` on the build tree, not on the installed package
(BAT-A10, open). A failed suite uninstalls: until the fix of NSI-A32 (2026-09-27) the
uninstaller of the `.m` and `.g` setups left `examples`, `library`, `prelude.dms` and the
folder behind; it now deletes them, as the `.c` one did. A setup run since then that still
leaves them behind means the uninstall section is wrong again. Both uninstalls, before the
install and after a failed suite, go through `batch\UninstallGeoDms.bat "<install dir>"`,
which runs a copy of `uninstaller.exe` from `%TEMP%` with `_?=` and then reports whether the
folder is gone (#1296). Run from the install folder itself, the uninstaller could not delete
its own exe, and the `rmdir` that followed it failed in a console without administrator
rights, so every failed setup left `GeoDms<ver>.<flavour>\uninstaller.exe` behind. A folder
the helper reports as still there holds something the uninstall section does not list.
Before it builds, each Windows script runs `git pull` in `..\tst`, the checkout the suite
reads, and stops when the pull fails; the `.g` one always did, the `.m` and `.c` ones since
2026-10-07, after the 20.23.0.m suite failed `Operator\cfg\operator.dms` on the conflict
markers a conflicting pull had left in `tst` that night. When a setup stops there, resolve
the merge in `C:\dev\tst` (or ask whether to abort it) and run the setup again.
Before it installs, each Windows script removes a previous installation of the same version
with that installation's own uninstaller, so a file the new `.nsh` no longer lists cannot
survive into the post-install suite; the `.m` script installed over it until BAT-A35, and
it now also stops when makensis, signtool or the installer fails. Answering N at the vcpkg
drift question of the `.m` or `.g` script leaves the previous build output in place; the
output folder is wiped after that question (BAT-A34). The Linux script removes an old
`.p7s` before it signs and fails when signing writes none (BAT-A36), so a `.sha256.p7s` in
`distr\` belongs to the `.sha256` of the same run.

Every setup installs `license.txt`, the GPL of GeoDMS, and beside it a folder `licenses` with
the licence texts of the libraries it bundles and `README.txt`, a table of every binary it
installs: where that binary comes from, its version and which text is its licence (#1371).
Its subfolder `geodms` holds the texts of the material from other projects inside GeoDMS's own
files, listed in `INSIDE_GEODMS` of `tools\setup_licences.py`: the Noto fonts and Remix Icon in
`GeoDmsGuiQt` and `misc\fonts\dmstext.ttf`, and the code from GDAL in `Geo.dll`; a second table
in `README.txt` says which installed file contains which (#1389). Every other file that
`qtgui\exe\GeoDmsGuiQt.qrc` compiles in must be in `GEODMS_GUI_FILES`, the files of the GUI that
are part of GeoDMS and come under its licence.
`tools\setup_licences.py stage` writes that folder into the output folder right before
makensis. For `.l`, `nsi\CreateLinuxSetup.sh` runs it on the tree it has staged, before
patchelf, and the `.deb` also gets `/usr/share/doc/geodms/copyright`. After the install,
`setup_licences.py check` compares the installed binaries with the table in both directions
and checks that `license.txt` and every text the table names are there, and that every installed
file that contains an item of `INSIDE_GEODMS` has its row; a difference removes
the install and the setup, as a failed suite does. The `.l` script skips the check together
with the install when WSL has no passwordless sudo, and says so. The uninstall sections
remove `license.txt` and `licenses` with the rest, so when `UninstallGeoDms.bat` reports a
folder still there that holds them, those lines have gone from the uninstall section.
`tools\make-wheels.py` maps the DLLs of a wheel to their source with the same code, and copies
the text of each item of `INSIDE_GEODMS` that a GeoDMS DLL of the wheel contains.

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
3. `batch\run_source_checks.bat` green (about forty seconds, no build; every setup script
   runs it before it wipes or builds anything, and stops on a red check), and
   `testcases\run_testcases.bat` green on the current build, so the setup's unit gate does
   not fail on something cheap. The script itself runs `batch\TestShippedDms.bat` on the
   output folder right before NSIS (the shipped copy of the battery current, every shipped
   `examples\` and `library\` file reached from a `shipped_*.dms` case, the battery run
   from that folder); that gate reads the copies in `bin\`, so a change under `library\`
   or `testcases\` is green there only after the script's own msbuild has mirrored it.
   The `.m` and `.c` scripts run it as the first step of `batch\TestShippedContent.bat`, and
   the Linux script as the first step of `batch\TestShippedContent.sh`; their second step
   downloads the CBS geopackage into an empty temporary `SourceDataDir` and runs
   `examples/grid_to_polygon.dms` over it (#1360). The `.g` script keeps the offline gate
   there and runs the download after installing, through `TestGlobioReleaseUnit.bat`.
4. `.m`, `.c` and `.l` need a network and, for that second step, about 12 GB of memory
   (`.l`: the minimum that `batch\TestShippedContent.sh` names at its top, in WSL), or the
   setup stops before packaging. `.g` needs `GLOBIO_ENV_ROOT` pointing at the conda prefix
   from `vcpkg-globio\environment.yml`; `.l` needs the WSL distro up.
5. Hand the user the one-liner that keeps the output on their screen and in a log you can
   read:

```powershell
& cmd /c "batch\BuildSignAndCreateSetup.bat 2>&1" | Tee-Object -FilePath scratch\build_m_<ver>.log
```

`cmd /c "... 2>&1"` merges stderr inside cmd, so PowerShell 5.1 does not wrap warning lines
as errors; `Tee-Object` shows and records. One flavour at a time.

## While it runs

Tail the log (`Select-String 'error|warning|ABORT|FAILED'`). Things the scripts check for
themselves and report: the source checks, a no-op build (`GeoDmsRun.exe` older than the
script start), vcpkg drift, the Qt deploy targets patch, the Python ABI modules, a concurrent MSBuild. Do not
"help" a CHOICE prompt along; the user answers it. The `.m` script asks only where the `.c`
script does (another build running; vcpkg drift, 30 s, default yes) and stops on a failed check
instead of asking whether a step went well: a nonzero msbuild exit, an installer of less than
60000000 bytes (its setups measure 61 to 63 million), a failed `signtool sign` or
`signtool verify /pa` (it then deletes the unsigned installer), a failed install, a red shipped
battery or unit suite. Rerun the whole script after fixing the cause; there is no retry inside
it (until 2026-10-06 it asked after the build, the NSIS step and the signing, and paused at the
end).

Through `Tee-Object` the text of a CHOICE prompt does not show: `choice` writes it without a
line end, and Tee passes on whole lines only. A run that seems to hang right after a step may
be waiting at one; a running `choice.exe` shows it, and the user answers in that console. The
`.g` script asked "Ready to sign" after NSIS until 2026-10-07, which looked like a hang after
the makensis summary; it now signs right away. It still asks after a failed build whether to
retry, besides the two questions the `.m` script keeps.

A run that crawls without a prompt: look at the priority of its processes. A shell in the
Terminal of Visual Studio is started by `DevHub.exe` at BelowNormal and passes that on, and
vcpkg starts its own children at Idle whatever its own priority. On 2026-10-14, with a model run
at BelowNormal holding 26 of 32 cores, the vcpkg drift check of the `.m` script sat 20 minutes,
and on the rerun from a Normal console 18 minutes, in a `vcvarsall.bat` that got under half a
second of CPU, and the `.c` script once more in the `vcpkg install` that its CMake configure
started. Since then `Build.bat` and the four setup scripts first call
`batch\RaisePriorityToNormal.ps1`: it raises their `cmd.exe` to Normal and starts one hidden
watcher per `cmd.exe` that, while that `cmd.exe` lives, raises every process below it at Idle or
BelowNormal to Normal once a second, so every vcpkg the setup reaches is covered. It prints a line
for each. Do not change the priority of someone else's run unless the user asks.

When the licence step stops, it prints each problem as `setup_licences: ERROR: ...`, and the
Windows scripts stop before NSIS with `*** Licence texts FAILED before packaging`, the Linux
one with `*** CreateLinuxSetup.sh failed`. What the messages mean, and what makes the next run
pass:

- `no licence known for <file>`: the setup installs a binary that is not GeoDMS, not the
  Visual C++ runtime and not a copy of a file of the Qt installation, a vcpkg port, the conda
  environment (`.g`) or a Debian package of the build machine (`.l`). That is a new
  dependency; let it come from one of those sources, so that its licence text comes along.
- `<file> differs from every file of that name in the sources of this flavour`: the output
  folder holds a copy from another build of that library than the one its source holds now.
  A stale output folder is one cause (on 2026-11-16 `build\windows-x64-release\bin` held five
  DLLs of an older vcpkg build); the Windows setup scripts empty their output folder before
  they build, so a run of the whole script deploys it again. A Qt upgrade is the other: then
  every Qt file differs, or, when the old Qt installation is gone, the step reports `no Qt
  SBOM in ...`. `QT_ROOT` in `tools\setup_licences.py` names the Qt installation and moves
  with `QtInstall` in `qtgui\exe\GeoDmsGuiQt.vcxproj` and `shv\dll\ShvDLL.vcxproj` and with
  `CMAKE_PREFIX_PATH` in `batch\BuildSignAndCreateSetupCmake.bat`. That installation needs
  its `sbom` folder and the Sources component of the Qt installer (`C:\Qt\<ver>\Src`), which
  holds the `LICENSES` folder of each module.
- `the conda package <name> <version> has no licence text of its own, and the text kept ...
  is for another version`: `vcpkg-globio\environment.yml` moved `libcurl`, `libpq` or
  `libsqlite`, whose conda packages carry no text. Take the licence of that version from the
  upstream source, keep it as `vcpkg-globio\licenses\<package>-<version>.txt`, commit it and
  run the setup again.
- `the font <file> is installed, but no licence text is known for it`: a new font. Put its
  licence text, with the copyright lines of the font, beside it in the repository and add an
  item for it to `INSIDE_GEODMS`; a font that Object Vision made itself goes into
  `GEODMS_FONTS` instead.
- `<file> is compiled into GeoDmsGuiQt through qtgui/exe/GeoDmsGuiQt.qrc, but no licence text
  is known for it`: a new file in the resource file, an image or a font. If it is part of
  GeoDMS and comes under its licence, add its path in the resource file to `GEODMS_GUI_FILES`; if
  it comes from another project with a licence of its own, put that licence text beside it and add
  an item to `INSIDE_GEODMS`. Ask where it comes from rather than guess. `GEODMS_GUI_FILES ... names <file>, which ... does not compile in`: the file left
  the resource file; remove it from the list.
- `INSIDE_GEODMS in tools/setup_licences.py names <file>, but this setup installs none of the
  files that contain it`: a binary was renamed, or GeoDMS no longer contains that file. Correct
  the names of the item, or remove it.
- `<nsi file>:<line number>: cannot read '<line>': <reason>`, such as `the command <name>,
  which this script does not know`: the NSIS script has a line that the NSIS parser of
  `tools\setup_licences.py` does not understand. It stops on purpose, so that a new `File`
  line cannot install a binary the table misses. Teach the parser the line; a command that
  installs no file goes into `NSIS_HARMLESS`.

## After a flavour

- `Test-Path 'C:\Program Files\ObjectVision\GeoDms<ver>.<f>\GeoDmsGuiQt.exe'`
- `(Get-AuthenticodeSignature 'distr\GeoDms<ver>.<f>-Setup-x64.exe').Status` is `Valid`
- the unit aggregate `C:\LocalData\GeoDMSTestResults\unit\v<ver>.<f>_<stamp>.txt` lists no
  failures: the header, two `python ... OK` lines and one `shipped examples\testcases
  battery OK` line (the battery run from the installed copy), nothing else
- `.g`: the launcher's summary at the end of the log reads `UNIT SUITE PASSED`,
  `TESTCASES BATTERY PASSED` and `SHIPPED CONTENT RELEASE TEST PASSED`
- `.l`: all four files in `distr\`, and the `.deb` installed in WSL
- the log line `setup_licences: <install dir>: <n> installed binaries, all in the table and
  the table holds no other`, and `license.txt` and `licenses\README.txt` in the installation
  folder (`.l`: under `/opt/ObjectVision/GeoDms<ver>.l`, with `/usr/share/doc/geodms/copyright`)

## Release tests

A publication is the one occasion for a full regression: `python full.py -version <ver>.<f>`
from `C:\dev\tst\batch`, per flavour, against the installed build. Ask before starting one;
it takes hours. Use the user's Python 3.13, start it from a scheduled task so it survives
this session and the agent app's self-update, keep the console visible, clear
`NoDefaultCurrentDirectoryInExePath`, never set `PYTHONUTF8`, and read
`C:\dev\tst\CLAUDE.md` for the report and reference rules. A real regression is not hidden by
regenerating its reference from the regressing build.

## Release notes

Write `doc\release-notes\release-notes-<ver>.md` and commit it. They used to be written beside
the setups in `distr\`, which is line 1 of `.gitignore`, so "commit it" was impossible and no
notes file was ever in version control; the fourteen that existed moved to `doc\` in one go. The
baseline is the latest full release, the one `gh release list` marks Latest, not the last
built version: interim builds in `distr\` are often never published (20.13 to 20.15 were built
and skipped; 20.16.0's notes covered everything since 20.12.0). The notes of a pre-release
also cover everything since that full release, the pre-releases in between included, and mark
what is new since the previous pre-release with *(new in <major>.<minor>)*: 20.28.0 to 20.30.0
each covered everything since 20.26.0, and 20.28.0 and 20.30.0 marked what was new. Start
from the previous notes, check each item against the commits since the previous release tag
(`git log v<prev>..HEAD --format='%h %s'`), and add the new work.

Format, as the existing files: an opening paragraph starting with **Pre-release.** or the
release status, naming the baseline and the headlines; a table with one row per flavour and
per set of Python wheels, with their asset names (a flavour not shipped says "no (full release
only)"); then `##` sections by theme, as in the previous notes, each bullet one short item
without a bold lead-in: what was wrong and what changed, with the issue number and the wiki
page. The notes end with a section "Changes that can affect an existing configuration", where
the things a modeller must act on (a result that changes on upgrade, a check that now fires, a
new operator name) are said outright.

## Publishing on GitHub

`gh` is authenticated as the user. This repository has immutable releases: after publishing,
assets cannot be added, and deleting a published release does not free its tag name;
`v20.13.0` was burned this way and shipped as `v20.13.0-preview`. So:

1. Create the draft with every asset in the same call:

```
gh release create v<ver> --draft --title "GeoDms <ver>" --notes-file doc\release-notes\release-notes-<ver>.md ^
  [--target <full sha>] distr\GeoDms<ver>.m-Setup-x64.exe distr\GeoDms<ver>.c-Setup-x64.exe ^
  distr\GeoDms<ver>.g-Setup-x64.exe distr\GeoDms<ver>.l-linux-x64.deb distr\GeoDms<ver>.l-linux-x64.tar.gz ^
  distr\GeoDms<ver>.l-linux-x64.tar.gz.sha256 distr\GeoDms<ver>.l-linux-x64.tar.gz.sha256.p7s ^
  distr\geodms-<ver>-cp312-cp312-win_amd64.whl distr\geodms-<ver>-cp313-cp313-win_amd64.whl ^
  distr\geodms-<ver>-cp314-cp314-win_amd64.whl distr\geodms_globio-<ver>-cp39-cp39-win_amd64.whl ^
  distr\geodms-<ver>-cp312-cp312-manylinux_2_39_x86_64.whl distr\geodms-<ver>-cp313-cp313-manylinux_2_39_x86_64.whl ^
  distr\geodms-<ver>-cp314-cp314-manylinux_2_39_x86_64.whl
```

   A draft creates no tag; it is safe to make and to inspect. Missing an asset here means a
   new draft, not an upload later. The wheels are those of `python\PythonVersions.txt`;
   `batch\BuildSignAndCreateSetup.bat` made and tested the Windows ones with the `.m` setup
   (`tools\make-and-test-wheels.ps1`, #1355), `batch\BuildSignAndCreateSetupGlobio.bat`
   the GLOBIO wheel with the `.g` setup (`-Flavour g`), and
   `batch\BuildSignAndCreateSetupLinux.bat` the Linux ones with the `.l` setup
   (`tools/make-and-test-wheels.sh`). Its last step tests a Linux wheel next to the osgeo of
   Ubuntu's `python3-gdal` (#1381); without that package in WSL the step is skipped and the
   log has a line `*** NOT TESTED next to another GDAL`, so install it on the build machine.
   A Linux wheel of 20.29.0 measured 90.6 MiB, one of 20.30.0 77.2 MiB.
   `tools\make-wheels.py` stops on a wheel over 100 MiB, the limit per file that PyPI sets
   for a project outside an organisation. The project geodms belongs to the PyPI
   organisation Object Vision, whose projects get twice that by default (`upload_limit_size`
   in warehouse's `packaging/models.py`), so a larger wheel can be allowed by raising the
   limit in the script, after checking the limit that pypi.org shows for the project.
2. `--target <full sha>` whenever the built commit is not the head of `main` on GitHub:
   without it the tag lands on `main` HEAD, the wrong commit. The commit must already be on
   origin (`git branch -r --contains <sha>`); if it is not, the user pushes it first. You
   never push.
3. Publishing is `gh release edit v<ver> --draft=false --prerelease` (or without
   `--prerelease` for a full release). That creates the tag on origin, an outward action:
   do it only on the user's explicit go-ahead for this release, after they have looked at
   the draft. A pre-release is not marked Latest. A full release, published as one or turned
   from a pre-release into one, also starts `.github\workflows\pypi-upload.yml`, which
   uploads the wheel assets of that release to PyPI: the `geodms-*` wheels to the project
   `geodms`, and a `geodms_globio-*` wheel to the project `geodms-globio`. A file on PyPI can
   never be replaced, so say so when asking for the go-ahead of a full release; a pre-release
   stays off PyPI.
4. Check `gh release view v<ver>` afterwards: title, target, assets, pre-release flag.

Never test what a release or asset operation does by writing placeholder text to a live
release; write only final content, and try mechanics on a draft.
