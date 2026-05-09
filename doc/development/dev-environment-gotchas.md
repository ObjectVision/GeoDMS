# Development-environment gotchas

Quirks of the Windows + WSL2 setup most developers run into, with the
right counter-measure beside each.

## `NoDefaultCurrentDirectoryInExePath` blocks `Test*.bat`

Many corporate Windows builds have `NoDefaultCurrentDirectoryInExePath=1`
set at the user level. Any `cmd.exe` spawned via PowerShell (or
`cmd.exe /c …`) inherits this and refuses to find batch files in the
current directory — so `cd C:\dev\tst\batch && Call unit.bat …` fails
with `'unit.bat' is not recognized` even though `dir unit.bat` shows
the file.

The repo's `TestDebugUnit.bat`, `TestReleaseUnit.bat`,
`TestCMakeDebugUnit.bat`, `TestCMakeReleaseUnit.bat`, `RunGUITests.bat`,
and `run_unit.bat` all do `cd ..\tst\batch && Call unit.bat <cfg> <flag>`
and break at the unprefixed `Call`.

**Fix:** clear the variable in the spawning shell *before* invoking any of
those scripts.

```powershell
$env:NoDefaultCurrentDirectoryInExePath = $null
cd C:\dev\GeoDMS_2026
& cmd.exe /c .\TestDebugUnit.bat
```

```bash
unset NoDefaultCurrentDirectoryInExePath
./TestReleaseUnit.bat
```

A Developer-PowerShell session inherited from VS will not have the var set,
which is why interactive runs from there don't trip on this — only
spawned/agent shells do.

## Git case-sensitivity in `shv/dll/src/`

A few files in `shv/dll/src/` were originally checked into git under a
lowercase name, but later renamed on disk to CamelCase. Windows'
case-insensitive filesystem hides this until you try to diff:

```bash
git diff <ref> -- shv/dll/src/DataView.cpp     # empty — wrong case
git diff <ref> -- shv/dll/src/dataview.cpp     # shows the real changes
```

**Known affected:** `dataview.cpp`, `dataview.h`, `movableobject.h`,
`palettecontrol.h`. (More may exist — verify with
`git diff --name-only <ref>... -- shv/dll/src/`.)

**Fix:** always use the **lowercase** path in any `git show` / `git diff`
that filters by file. Resolving the inconsistency requires the
`git mv --force` two-step on a case-insensitive filesystem; nobody has
done it yet.

## Repository search scope

When grepping C++ source, exclude generated / third-party trees and
include `.ipp` files (GeoDMS uses them for template implementations):

```sh
# rg / ripgrep
rg --type cpp -g '!build/' -g '!bin/' -g '!vcpkg*/' -g '*.{cpp,h,ipp}' …
# grep
grep -rn --include='*.cpp' --include='*.h' --include='*.ipp' \
     --exclude-dir=build --exclude-dir=bin --exclude-dir=vcpkg \
     --exclude-dir=vcpkg_installed pattern .
```

Without these excludes, results from `build/`, `bin/`, and the various
`vcpkg*` trees overwhelm anything in your actual source.
