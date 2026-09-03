GeoDms 20.8.0 is a large **engineering** release. The headline is an **object-ownership refactor** — the `TreeItem` / data-item family moved from intrusive reference-counting onto standard `std::shared_ptr` / `std::weak_ptr`, closing a class of teardown hangs, exit leaks and use-after-frees. Alongside it are a broad **build & compile-time modernization** (precompiled headers, `std::format`, a large boost reduction down to a handful of header-only libraries, a three-DLL merge), a **Qt 6.11** upgrade, a big **Linux/GCC warning cleanup**, expanded **Python bindings**, and a set of view, operator and storage fixes.

The sections below cover everything since **20.3.0** — the 20.4 / 20.5 / 20.6 / 20.7 builds were internal and not published as GitHub releases.

Builds are provided at the same source revision in three flavors:

| Flavor | Platform | Asset |
|---|---|---|
| `.m` | Windows x64 (MSBuild) | `GeoDms20.8.0.m-Setup-x64.exe` |
| `.c` | Windows x64 (CMake) | `GeoDms20.8.0.c-Setup-x64.exe` |
| `.l` | Linux x64 (WSL / Ubuntu 24.04) | `GeoDms20.8.0.l-linux-x64.deb` (+ `.tar.gz`) |

## 🧬 Object-ownership refactor (internals)
- The `TreeItem` / data-item family was migrated from intrusive `SharedPtr` / `WeakPtr` onto standard `std::shared_ptr` / `std::weak_ptr`, with an explicit **parent-owns-child** model and non-owning `std::weak_ptr` back-references.
- Fixed a class of ownership bugs surfaced by the migration: several **teardown drain hangs**, **memory leaks reported at exit**, cache-unit liveness use-after-frees, move-assignment lock leaks (`ItemWriteLock` / `DataWriteLock` / `DataReadLockAtom`), and interest-at-destruction leaks that hung GUI close.
- Fixed a **GUI exit deadlock**: open map views and value-info windows are now destroyed *before* the shutdown drain waits on the item interest they hold (previously their deletion was deferred past the drain, hanging test-script and app-exit teardown — reproducibly so on Linux).
- Debug unit tests now exit cleanly; headless Debug runs surface asserts and leak dumps **without** modal `abort()` dialog stalls.

## 🏗️ Build & compile-time modernization
- **Precompiled headers** enabled for the 8 DLL projects.
- `boost::format` → `std::format`; broad **boost reduction** (random / locale / tuple / mpl / core / preprocessor → std / native); **`boost_thread` eliminated** (Spirit parsing serialized behind a single mutex); **`boost::signals2` replaced** by a ~100-line synchronous `Signal<>` (drops ~1100 transitive headers from 29 view-DLL translation units).
- The **boost dependency narrowed to 11 header-only libraries** actually used; GeoDMS links **zero compiled boost**, and the installed boost runtime DLLs went from 37 to 15 (the remainder are CGAL / asio package dependencies).
- **`rtc` + `sym` + `tic` merged into a single DLL** (`DmRtc` / `Rtc.dll`); source tree flattened accordingly.
- `clc`: the four largest translation units split per value-type for faster, more parallel builds.
- On-demand MSVC `/analyze` (PREfast) pass (`analyze.bat`); fixed the intermittent vcpkg runtime-DLL deploy gap for good (both a stray machine-wide vcpkg integration and an `IncrementalClean` bookkeeping asymmetry that reaped the GDAL/GEOS/PROJ runtime DLLs on certain build sequences).

## 🐧 Linux / GCC
- **Warning cleanup** from ~67k down to a few hundred, fixing a handful of latent bugs along the way (a `delete[]` of `void*`, a pointer-vs-literal comparison, a 16-bit truncation, a `1 << 63` shift overflow, an always-false `IsInTrans()`).
- **Self-contained Linux package** — the `.l` setup bundles the Qt6 runtime with an `$ORIGIN` rpath, so the GUI runs without a system Qt6 install.
- Fixed an **exit-time SIGABRT** after GDAL write sessions (the GDAL cleanup `atexit` hook is now Windows-only; on Linux it ran after GDAL's own teardown).
- Missing-`#include` and link fixes that GCC surfaces but MSVC hides.

## 🖥️ Qt & Python
- **Qt upgraded to 6.11.1.**
- **Python bindings**: primary-data accessors, `InMemoryConfig` functions, an example and binding test scripts; the CMake build now ships Python 3.13.

## 🗺️ View & interaction
- **#515**: overview zoom-out limited to the background / world extent.
- **#1151**: drag sliders widened to 8 px (was 5 px).
- **#1150**: table resize works after Toggle Rows/Cols; honest, non-sticky resize cursors; added untoggled row-height resize.
- **#1149**: statistics copy-to-clipboard now starts with an `Item name <path>` row (and fixes a `ReplaceChar` infinite loop).

## 🔧 Operators, storage & fixes
- **#1152**: items whose storage turns out to hold no primary data now **fail with a clear error** instead of lingering non-ready and non-failed.
- **#411**: the table-view export button opens the Export Primary Data dialog (adds XML format) and reports export failures.
- **#367**: internal `file://` UNC encoding is converted to native paths at the GDAL / cfs boundaries.
- **#1146 / #1148**: actionable errors on export-meta self-reference instead of recursing; force a `0` class-break when signed data straddles zero.
- **#1124**: MetaInfo sidecar diverted to `<stem>.meta.<ext>` when it would collide with the dataset; GML classified as non-updatable.
- Removed long-obsolete operators (`format` / unprefixed `boost::polygon` / non-`DPoint` GEOS).
