# WSL build setup for GeoDMS Linux target

This is the reference setup for building and running `libDmShv.so` and
`GeoDmsGuiQt` on Linux. It runs on **WSL2 Ubuntu 24.04** with **WSLg** for
display, against the shared Windows source tree.

## Source location — shared, not duplicated

The Windows source tree at `C:\dev\GeoDMS_2026` is shared with WSL at
`/mnt/c/dev/GeoDMS_2026`. There is **no** separate Linux checkout — both
platforms build from the same files.

Build outputs land in `build/linux-x64-{debug,release}/` and
`build/windows-x64-{debug,release}/` respectively, side-by-side under the
same repo root, accessible from both Windows and WSL.

## Build system

- **CMake + Ninja** with vcpkg.
- Presets in [`CMakePresets.json`](../../CMakePresets.json) (repo root):
  - `linux-x64-debug`   → `build/linux-x64-debug/`
  - `linux-x64-release` → `build/linux-x64-release/`
- Both presets are condition-guarded on `hostSystemName == Linux` and require
  `$VCPKG_ROOT` to be set.
- vcpkg triplet: `x64-linux`.

Typical build invocation from a WSL2 bash shell:

```sh
cd /mnt/c/dev/GeoDMS_2026
cmake --preset linux-x64-debug
cmake --build --preset linux-x64-debug
```

## Dependencies

| Source | Components |
|---|---|
| **vcpkg** (managed via `vcpkg.json`) | boost, cgal, fftw3, gdal (+arrow +parquet), geos, openssl, sqlite3 (+rtree). pybind11 is Windows-only. |
| **System Qt 6.9** (NOT vcpkg) | Found via `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)`. Install via apt or the official Qt installer. |

## Output artefacts

| Windows | Linux |
|---|---|
| `DmRtc.dll`, `DmTic.dll`, `DmShv.dll`, … | `libDmRtc.so`, `libDmTic.so`, `libDmShv.so`, … |
| `GeoDmsGuiQt.exe` | `GeoDmsGuiQt` (no extension) |
| `GeoDmsRun.exe` | `GeoDmsRun` (no extension) |

## Display — WSLg

GUI runs via **WSLg** (WSL2's built-in Wayland/X11 display server). No extra
DISPLAY setup is needed on modern WSL2 with WSLg.

WSL2 is a hard requirement (not just a preference): WSLg only exists in WSL2.
WSL1 uses a syscall translation layer with no real Linux kernel and no GUI
support — Qt apps like GeoDmsGuiQt cannot open windows under WSL1 without a
separately installed X server (e.g. VcXsrv).

## Run regression tests

The Windows-side `python full.py -version local-linux-release` exercises the
linux build (path-based via `BinDir`); `python full.py -version 20.0.0l`
exercises an installed copy at `/opt/ObjectVision/GeoDms20.0.0l/` (produced
by [`nsi/CreateLinuxSetup.sh`](../../nsi/CreateLinuxSetup.sh) and
[`BuildSignAndCreateSetupLinux.bat`](../../BuildSignAndCreateSetupLinux.bat)).
Both routes invoke the binary via `wsl --` from the Windows-side runner.

## Architecture summary

- `libDmShv.so` = the GeoDMS rendering / data-view engine (formerly only
  `DmShv.dll`).
- `GeoDmsGuiQt` links against `DmRtc DmSym DmTic DmStx DmStg DmClc DmGeo
  DmShv Qt6::Core Qt6::Gui Qt6::Widgets`.
- The portability layer (`ViewHost` / `DrawContext`) is documented in the
  root [`PORTING_STATUS.md`](../../PORTING_STATUS.md).
