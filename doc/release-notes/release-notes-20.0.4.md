Public pre-release of the **20.0.4** line with three installer flavors:
- Windows CMake (`.c`) flavor — `GeoDms20.0.4.c-Setup-x64.exe`
- Linux/WSL (`.l`) — signed `.deb` + `.tar.gz`
- Windows MSBuild (`.m`) — `GeoDms20.0.4.m-Setup-x64.exe`, available for comparison with the `.c` flavor; may be phased out to reduce the number of build tools.

Same source, three build paths — pick whichever matches your workflow.

This release supersedes and **retracts `20.0.0c`**; its release notes are folded in below so nothing is lost.

For installation steps see the wiki:
- [Installation Instructions](https://github.com/ObjectVision/GeoDMS/wiki/Installation-Instructions)

Linux-only verification of the signed `.tar.gz` is documented in [`nsi/VERIFY-LINUX.md`](https://github.com/ObjectVision/GeoDMS/blob/v20.0.4/nsi/VERIFY-LINUX.md).

## What's new since v20.0.0c

### Security / hardening
- **GDAL + DLL search path** hardened against config-driven RCE
- `LastConfigFile`: validate the registry path before silent auto-load, and confirm before reopening on startup
- `ConfigFileName`: cap `#include` nesting depth at 64; FS-aware path compare for include-recursion detection
- Reject out-of-envelope ROI; harden `Round.h` against NaN / out-of-range input

### Storage (#1098, #1106)
- `ReadUnitRange` now defines StorageManager-determined unit ranges using the block size the external file actually has (tif and gdal.grid)
- Expose native TIFF tile/strip size as `StorageTileSizeX/Y`
- `gdal_base`: register user-specified `GDAL_DRIVER` on the read path (#1106)

### GUI
- #946 indirect `StorageName` components are now clickable
- #621 F2 traces the error through `FenceContainer`
- #1112 fix in-cell editing regression in `PaletteEditor` / `TableView`
- #1093 `MultiPointLayer`: `PointCount` column + `sequence_element_count` operator
- #212 square symbol support
- #1109 tooltip values capped at 400 characters
- #1100 follow-ups: `ScrollDevice` no longer blanks the MapView on pan; XOR-mode line/polygon carets with redraw bracketing and resize/straddle artifact fixes
- #1113 refactor suitability-map retrieval and error handling
- `GridLayer`: restore `SRCAND` blit so white pixels keep underlying layers visible
- Restore treeview / detail-pages width after a window squeeze
- `layercontrol` length-attribute fix

### Correctness / threading
- Reverted the experimental post-order supplier-DAG drain (`Actor::SuspendibleUpdate` / `UpdateMetaInfo`, `FuncDC` R2 phases) that caused worker-pool starvation; kept the stack-safety refactors that convert deep recursion to explicit worklists
- `tile_task_group`: keep the CheckThis sum-vs-counter invariant in bulk-completion paths
- Catch exceptions before they propagate to Qt; richer context when a `DmsException` is thrown
- Fixed #1108, #1110, #1111, #1114, #1115

### Windows integration
- #499 GeoDMS installs now register in Windows **Apps & Features**

### Build & release
- `windeployqt`: use `--translationdir` (not `--translations`)
- Build-staleness guard switched from empty `FileVersion` metadata to an mtime check; cmake (`.c`) guard checks `DmRtc.dll` (always relinked) to avoid a false abort when ABI-unchanged dependents skip relink

### Linux
- `-Wl,-z,stack-size=67108864` for `GeoDmsRun` and `GeoDmsGuiQt`
- Longer regression time-out for `t641` under WSL

---

## What's new since v19.3.0 (carried over from the retracted 20.0.0c)

### Cross-platform / Linux port
- Full Linux/WSL2 build via CMake + Qt 6 (`linux-x64-release` / `linux-x64-debug` presets)
- New flavor-suffixed install layout (`GeoDms<ver>.{m,c,l}`) so all three can coexist
- Unit test suite (`TestLinuxDebugUnit.sh`, `TestLinuxReleaseUnit.sh`) and Linux regression harness (`full.py -version local-linux-release`)

### GUI (#1100)
- Bold table-header text, 1-px padding fix in `DataItemColumn`; correct row-height unit in tabular controls
- `dmsscript`: `BringToFront` keyword to raise the GUI in Z-order

### Filesystem & Unicode (#1101)
- Three-pass UTF-8 → wide-char audit across Windows filesystem calls (incl. `IsFileOrDirWritable` → `_waccess`)
- Linux `ConvertDmsFileName` lexically normalises `..` so includes through .dms-stem subdirs work cross-platform

### Correctness (other)
- **#1103 / #462** `lookup`: propagate `V::UNDEFINED` through merged-unsigned proxy — fixes the 407K-row BAG snapshot regression where IPoint NULLs leaked as `(-1, -1)`
- **#1102** `IsMainThread()` → `IsMetaThread()` sweep across rtc/tic/stx/sym/clc/stg
- `connect`: fix dangling `SA_Reference` from temporary `SA_Iterator`
- `stg/shp`: `ShpHeader` / `ShpRecordHeader` / `ShpPolygonHeader` `long → Int32`
- `qtgui`: construct `QApplication` before the try block
- UnifyValues regression fix (`bee4cd22`, `6c421f66`)

### Performance / memory
- `rtc/FixedAlloc`: drop power-of-2 guard from `SpecialSize`
- `stg/tic`: expose native GDAL block size as `StorageTileSizeX/Y` props

### CLI ergonomics
- `GeoDmsRun` / `GeoDmsGuiQt`: unknown command-line options are diagnosed (Windows `/<x>`, Linux `-<x>`) instead of silently treated as a config-file name
- `StxInterface`: `Cannot open configuration file '<path>'.` error now also reports the CWD

### Build & release
- `nsi/GeoDmsVersion.cmd` — single source for `MAJOR.MINOR.PATCH`, shared across the three `BuildSignAndCreateSetup{,Cmake,Linux}.bat` scripts
- `DMS_PLATFORM` title-bar tag distinguishes cmake vs msbuild builds
- Signed `.deb` + `.tar.gz` produced via PowerShell `SignedCms`; verification recipe in `VERIFY-LINUX.md`

## Known issues

- The `.m` setup in this release was built from `193fc052` (the 20.0.4 version bump), one commit before the `windeployqt --translationdir` fix; its bundled Qt translations layout differs cosmetically from `.c`. The `.m` is provided only for comparison.
- Linux regression: `t641_*` (RSopen MakeBaseData / Variant / Allocatie) needs WSL memory ≥ 64 GB; on smaller VMs they OOM. Native Linux is unaffected.
- Profiler bokeh series flat-lines for the `.l` flavor (#1104) — cosmetic; test status is still correct.
- Python binding version matching (#1105) — `.m` `geodms.pyd` links Python 3.13, `.c` links Python 3.12, no runtime is bundled. Users supply their own matching Python.

## Full changelog

https://github.com/ObjectVision/GeoDMS/compare/v19.3.0...v20.0.4
