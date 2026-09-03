GeoDms 20.2.0 builds on the 20.1.0 Linux release with a new **interactive Chart view** (#75), editor-preset improvements, and a round of operator, calculation, and build-system fixes.

Builds are provided at the same source revision in three flavors:

| Flavor | Platform | Asset |
|---|---|---|
| `.m` | Windows x64 (MSBuild) | `GeoDms20.2.0.m-Setup-x64.exe` |
| `.c` | Windows x64 (CMake) | `GeoDms20.2.0.c-Setup-x64.exe` |
| `.l` | Linux x64 (WSL / Ubuntu 24.04) | `GeoDms20.2.0.l-linux-x64.deb` (+ `.tar.gz`) |

## 📊 Chart view (#75, new)
- New **Chart data view** with a histogram mode and a series mode.
- **Series layers**: scatter and line rendering via a dedicated `ChartLayer`, with thematic colouring and menu synchronisation.
- **Bar mode**: bar draw mode with an X-axis picker, a categorical X axis, and side-by-side grouped bars for multiple bar layers.
- Qt exception-safety hardening around the new view.

## 📝 Editor presets (#1125)
- Editor presets are now **template-driven command lines** with column support.
- The preset combobox is replaced by a **defaults popup**; only the command line itself is stored.

## ➗ Operators & calculation
- **`area` / `arc_length`** (#1119) now convert the result to the requested unit and gain a unary auto-derive form.
- **`geos_buffer`** (#1038) dispatches on argument `ValueComposition`; `buffer_multi_point` accepts arc/polygon compositions, plus a `multi_point` constructor and clearer composition diagnostics / deprecation warnings.

## ⚡ Stability
- **#1126**: dropped a false-positive DeadLock detection in `lock_shared`.
- **#607**: fixed palette-editor caret leaving traces outside the `#Classes` box.

## 🔧 Diagnostics & build
- **#1133**: the `/L` log now carries explicit severity + category tags; the profiler parses both the tagged and the legacy log formats.
- The executable now **self-determines its exe-root directory**, and Help/About reports the running executable.
- Build-system housekeeping: project-local vcpkg MSBuild integration, `CONFIGURE_DEPENDS` on CMake source/header globs, de-hardcoded repo-root paths in build/test scripts, LOC tooling, and a `CLAUDE.md` build/run policy.

## Verifying the Linux download
```
sha256sum -c GeoDms20.2.0.l-linux-x64.tar.gz.sha256
```
The accompanying `.p7s` is a detached SHA-256 signature of the checksum file (GlobalSign EV).

**Full Changelog**: https://github.com/ObjectVision/GeoDMS/compare/v20.1.0...v20.2.0
