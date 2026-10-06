# Linux port of DmShv and GeoDmsGuiQt: what is still open

*Status (2026-10-06): the port is done and ships. The work of branch `refactor_linux_gui` is in
`main` (its last merge, 5a4a65df7 of 2026-06-16, is an ancestor of HEAD); `GeoDmsGuiQt` builds with
the `linux-x64-release` preset and is packaged by `nsi/CreateLinuxSetup.sh`; the Windows releases are
built from the same code. Open are the four items below. Moved on 2026-10-06
from `PORTING_STATUS.md` at the repository root (last updated 2026-04-15) and trimmed to what is open:
its "not yet verified" Windows rows, its TODOs before merging to v17 and its record of the
compilation fixes and of the code moved out of Win32 blocks are obsolete or history, which git keeps.*

## How-to / gotchas

Practical notes that travel with the code, useful when picking up the port on a different
machine, especially the GUI and display-indirection ones (see also [README.md](README.md)):

| Doc | What it covers |
|---|---|
| [wsl-build-setup.md](wsl-build-setup.md) | WSL2 / CMake / vcpkg / Qt6 setup |
| [qt-hwnd-gotchas.md](qt-hwnd-gotchas.md) | Mouse capture, key events, focus, timer paths in the hybrid HWND/QWidget architecture |
| [operator-dms-tests.md](operator-dms-tests.md) | Operator.dms unit-test fixes (hex-escape parsing, ASCII translit, intentional non-UTF-8 bytes) |
| [sa-iterator-lifetime.md](sa-iterator-lifetime.md) | `auto& x = *(iter + n)` dangling-reference pattern (surfaced by Linux GCC) |

## Architecture

Two abstraction layers decouple platform-specific code from the shared model:

- **ViewHost** (`shv/dll/src/ViewHost.h`): abstract interface for windowing operations (timers,
  capture, focus, cursors, invalidation, tooltips, context menus, drawing, scroll, text caret).
  `QDmsViewArea` (`qtgui/exe/src/DmsViewArea.cpp`, QWidget-based) implements it on both platforms.
  `Win32ViewHost` (HWND-based), compiled into the Windows build but constructed nowhere, was
  deleted on 2026-10-06 (c0737bfe8).
- **DrawContext** (`shv/dll/src/DrawContext.h`): abstract interface for all rendering (rectangles,
  lines, polygons, text, images, clipping, fonts). Implementations: `GdiDrawContext` (HDC-based,
  Windows, used for the DataView's own paint and for `GetAsDDBitmap`) and `QtDrawContext`
  (`qtgui/exe/src/QtDrawContext.cpp`, QPainter-based).

## Open

- **Pen and font caching, on both platforms.** `PenIndexCache` and `FontIndexCache` still compute
  the distinct pens and fonts of a layer, but the handles are GDI (`HPEN`, `HFONT`/`LOGFONT`;
  the Win32-only `PenArray` and `FontArray`, dead since Step 4, were deleted on 2026-10-06, c0737bfe8),
  and there is no portable pen or font array behind the
  `DrawContext`. Windows lost its caches as well: since Step 4a and 4c (ca9bb9da0, 922fb95bc)
  `GdiDrawContext` creates and deletes a GDI pen, brush or font per draw call, about 3M calls per
  redraw of a 1M-polygon layer (audit SHV-A07 of the code audit of 2026-09-27).
- **Bitmap export.** `MovableObject::GetAsDDBitmap`, `SaveBitmap` and `ViewPort::Export` are
  Win32-only; on Linux `ViewPort::Export` is an empty stub. Qt equivalent: `QImage::save()`.
- **`Win32ViewHost`** (`shv/dll/src/Win32ViewHost.{cpp,h}`): compiled but never constructed;
  removing it is one of the leftovers of finished migrations (audit PLN-A16).
- **`_WIN32` guards in shv**: 163 `#if`, `#ifdef`, `#ifndef` or `#elif` lines that test `_WIN32`
  across 43 files of `shv/dll/src` at 0563aa5a0.
