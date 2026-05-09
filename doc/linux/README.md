# GeoDMS Linux porting notes

Practical notes captured while porting GeoDMS (DmShv + GeoDmsGuiQt) to Linux on the
`refactor_linux_gui` branch. Companion to [PORTING_STATUS.md](../../PORTING_STATUS.md)
at the repo root, which tracks the *progress* of the port; the files here are the
*how-to / gotchas* — meant to onboard a contributor on a different machine without
having to re-discover the things we already paid for.

## Files in this directory

| File | What it covers |
|---|---|
| [wsl-build-setup.md](wsl-build-setup.md) | Building DmShv.so + GeoDmsGuiQt on WSL2 Ubuntu 24.04 from the shared Windows source tree at `/mnt/c/...`. CMake presets, vcpkg triplet, Qt6 placement, WSLg display, output layout. |
| [qt-hwnd-gotchas.md](qt-hwnd-gotchas.md) | Pitfalls of the hybrid Windows architecture where DataView's native HWND is parented under a `QDmsViewArea`. Mouse capture, key events, focus, the WM_TIMER vs `OnTimer` split. **Read this before touching any code in `qtgui/exe/src/DmsViewArea.cpp` or `shv/dll/src/dataview.cpp`.** |
| [operator-dms-tests.md](operator-dms-tests.md) | The set of unit-test fixes that made `unit_linux.sh` go green: hex-escape support in the DMS parser, ASCII-translit replacement for glibc iconv, intentional non-UTF-8 bytes that must be preserved. |
| [sa-iterator-lifetime.md](sa-iterator-lifetime.md) | C++ lifetime pitfall first surfaced by Linux GCC strict-builds — `auto& ref = *(iter + n)` is a dangling reference whenever the iterator is a temporary. Generic but bit us during porting. |

## Cross-machine usage

These notes are kept in the repo (rather than in any one developer's tooling memory)
so they travel with the code. When pulling the branch on a new machine, the build
instructions and gotchas are immediately available.
