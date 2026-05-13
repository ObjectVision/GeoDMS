First public pre-release with three installer flavors: Windows MSBuild (`.m`), Windows CMake (`.c`), and Linux/WSL (`.l` `.deb` + `.tar.gz`). Same source, three build paths — pick whichever matches your workflow.

For installation steps see the wiki:
- [Installation Instructions](https://github.com/ObjectVision/GeoDMS/wiki/Installation-Instructions)

Linux-only verification of the signed `.tar.gz` is documented in [`nsi/VERIFY-LINUX.md`](https://github.com/ObjectVision/GeoDMS/blob/v20.0.0b/nsi/VERIFY-LINUX.md).

## What's new since 19.3.0

### GUI (#1100)
- Bold table-header text, 1-px padding fix in `DataItemColumn`; correct row-height unit in tabular controls
- MapView: zoom keys, ROI alpha-blend, GridLayer palette R/B swap
- TableView: column-resize cursor drift + scroll-blit caret artifacts fixed
- `dmsscript`: `BringToFront` keyword to raise the GUI in Z-order

### Cross-platform / Linux port
- Full Linux/WSL2 build via CMake + Qt 6 (`linux-x64-release` / `linux-x64-debug` presets)
- New flavor-suffixed install layout (`GeoDms<ver>.{m,c,l}`) so all three can coexist
- Native scroll bars, focus/keyboard routing, DetailPage HTML capture on Qt
- Unit test suite (`TestLinuxDebugUnit.sh`, `TestLinuxReleaseUnit.sh`) and Linux regression harness (`full.py -version local-linux-release`)

### Filesystem & Unicode (#1101)
- Three-pass UTF-8 → wide-char audit across Windows filesystem calls (incl. `IsFileOrDirWritable` → `_waccess`)
- Linux `ConvertDmsFileName` lexically normalises `..` so includes through .dms-stem subdirs work cross-platform

### Correctness
- **#1103 / #462** `lookup`: propagate `V::UNDEFINED` through merged-unsigned proxy — fixes the 407K-row BAG snapshot regression where IPoint NULLs leaked as `(-1, -1)`
- **#1102** `IsMainThread()` → `IsMetaThread()` sweep across rtc/tic/stx/sym/clc/stg
- `connect`: fix dangling `SA_Reference` from temporary `SA_Iterator`
- `stg/shp`: `ShpHeader` / `ShpRecordHeader` / `ShpPolygonHeader` `long → Int32`
- `qtgui`: construct `QApplication` before the try block

### Performance / memory
- Linux: cache stack base in TLS (was reading `/proc/self/maps` per `RemainingStackSpace` call)
- Linux: `munmap` mapped views in `ViewData` destructor (mapping leak fixed)
- `rtc/FixedAlloc`: drop power-of-2 guard from `SpecialSize`
- `stg/tic`: expose native GDAL block size as `StorageTileSizeX/Y` props

### CLI ergonomics
- `GeoDmsRun` / `GeoDmsGuiQt`: unknown command-line options are diagnosed (Windows `/<x>`, Linux `-<x>`) instead of silently treated as a config-file name
- `StxInterface`: `Cannot open configuration file '<path>'.` error now also reports the CWD so the cause of a path mismatch is one glance away

### Build & release
- `nsi/GeoDmsVersion.cmd` — single source for `MAJOR.MINOR.PATCH`, shared across the three `BuildSignAndCreateSetup{,Cmake,Linux}.bat` scripts
- `BuildSignAndCreateSetup.bat` (msbuild) drops two interactive CHOICE prompts: always refreshes the generated headers, always does an incremental build
- `DMS_PLATFORM` title-bar tag distinguishes cmake vs msbuild builds
- Signed `.deb` + `.tar.gz` produced via PowerShell `SignedCms`; verification recipe in `VERIFY-LINUX.md`

## SmartScreen note

Newly-published EV-signed installers still trigger Windows SmartScreen "Don't run" prompts until Microsoft's reputation system has seen enough clean downloads from enough unique machines for this exact file hash. Click **More info** → **Run anyway**. Subsequent downloads of the same hash become clean automatically after a few days.

## Known issues

- Linux regression: `t641_*` (RSopen MakeBaseData / Variant / Allocatie) OOM on a 64 GB WSL VM — workload genuinely needs more memory than WSL has by default. Increase WSL memory cap or run on native Linux.
- Profiler bokeh series flat-lines for the `.l` flavor (#1104) — cosmetic; test status is still correct.
- Python binding version matching (#1105) — `.m` `geodms.pyd` links Python 3.13, `.c` links Python 3.12, no runtime is bundled. Users supply their own matching Python.

## Full changelog

https://github.com/ObjectVision/GeoDMS/compare/v19.3.0...v20.0.0b
