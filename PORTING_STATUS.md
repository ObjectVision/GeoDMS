# Linux Porting Status — DmShv + GeoDmsGuiQt

Branch: `refactor_linux_gui`
Last updated: 2026-04-14

## Build status

| Target | Platform | Status |
|--------|----------|--------|
| DmShv (libDmShv.so) | Linux GCC 14 / Ubuntu 24.04 | Compiles and links clean |
| GeoDmsGuiQt | Linux GCC 14 / Ubuntu 24.04 | Compiles and links clean |
| DmShv (DmShv.dll) | Windows MSVC / VS 2022 | Not yet verified after latest changes |
| GeoDmsGuiQt.exe | Windows MSVC / VS 2022 | Not yet verified after latest changes |

Build environment: WSL2 Ubuntu 24.04, vcpkg (baseline `1441f2ae63070ddb8afa19b35127c32c03dcf9dd`), CMake + Ninja, Qt 6.9, Boost 1.88.

## Architecture

Two abstraction layers decouple platform-specific code from the shared model:

- **ViewHost** (`shv/dll/src/ViewHost.h`) — abstract interface for windowing operations (timers, capture, focus, cursors, invalidation, tooltips, context menus, drawing, scroll, text caret). Implementations:
  - `Win32ViewHost` (HWND-based, GDI) — for Windows
  - `QDmsViewArea` (QWidget-based, QPainter) — for Qt/Linux (and Windows Qt path)

- **DrawContext** (`shv/dll/src/DrawContext.h`) — abstract interface for all rendering (rectangles, lines, polygons, text, images, clipping, fonts). Implementations:
  - `GdiDrawContext` (HDC-based) — for Windows GDI
  - `QtDrawContext` (QPainter-based) — for Qt/Linux

## Completed work

### DmShv compilation fixes

| File | Fix | Category |
|------|-----|----------|
| GraphicContainer.cpp | Added `template` keyword for dependent base call; fixed explicit instantiation syntax | GCC two-phase lookup |
| FeatureLayer.cpp | Added `typename` for dependent types | GCC two-phase lookup |
| DataItemColumn.cpp | Fixed narrowing conversion in switch case | GCC strictness |
| DrawPolygons.h | Created `LabelDrawer.h` header to resolve incomplete type | Header refactoring |
| GridLayer.cpp | Guarded `TB_CutSel`/`TB_CopySel`/`TB_PasteSelDirect`/`TB_PasteSel` with `#ifdef _WIN32` | Platform guard |
| EditPalette.cpp | Guarded `CreateMdiChild()` calls with `#ifdef _WIN32` | Platform guard |
| MapControl.cpp | Guarded `TB_CopyLC` (Export) with `#ifdef _WIN32` | Platform guard |
| ShvCompat.h | Added `MK_*` mouse flags, `SW_*` show constants, `IDC_*` cursor stubs | Linux stubs |
| ShvBase.h | Added `DmsCursor` enum for portable cursor types | Portable abstraction |

### DmShv — portable code extracted from Win32 blocks

| File | What was moved | Details |
|------|---------------|---------|
| dataview.cpp | `InsertCaret`, `RemoveCaret`, `MoveCaret`, `ClearTextCaret`, `InsertController`, `RemoveController`, `OnCommandEnable`, `ReverseCaretsImpl` | Were trapped inside `#ifdef _WIN32` (DestroyWindow/GetDefaultFont block). Closed `#ifdef` after GetDefaultFont. |
| dataview.cpp | `UpdateTextCaret` Win32 fallbacks (`CreateCaret`, `SetCaretPos`, `ShowCaret`, `HideCaret`, `DestroyCaret`) | Guarded individual `else` branches; ViewHost paths remain portable. |
| dataview.cpp | `ReverseSelCaretImpl` (uses HBITMAP/BITMAPINFOHEADER) | Wrapped with `#ifdef _WIN32` |
| dataview.h | Added `OnResize(GPoint, CrdPoint)` | Portable alternative to `OnSize(WPARAM, GPoint)` |
| GraphVisitor.cpp | `AddTransformation`, `AddClientLogicalOffset`, `VisitorDeviceRectSelector` | Moved from DcHandle.cpp (excluded on Linux) |
| Region.cpp | `DcClipRegionSelector` implementation | Moved from DcHandle.cpp |
| ShvUtils.cpp | `GetFocusClr()`, `GetSelectedClr()` Linux stubs | Added `#ifndef _WIN32` block |
| ScrollPort.cpp | `OnChildSizeChanged()` | Moved out of `#ifdef _WIN32` block |
| ViewPort.cpp | `GetExportInfo()` | Moved out of `#ifdef _WIN32` block |
| ViewPort.cpp | `Export()` Linux stub | Added `#else` branch to satisfy pure virtual |
| TextControl.cpp | `TextControl` class methods | Moved out of `#ifdef _WIN32` block |

### DmShv — portable ViewHost features implemented

| Feature | ViewHost method | QDmsViewArea (Qt) | DataView integration |
|---------|----------------|-------------------|---------------------|
| Popup menus | `VH_ShowPopupMenu(GPoint, MenuData&)` | QMenu from MenuData (levels, separators, flags, execute) | `ShowPopupMenu()` routes through ViewHost |
| Tooltips | `VH_ShowTooltip(GPoint, CharPtr)` / `VH_HideTooltip()` | `QToolTip::showText()` / `hideText()` | `OnTimer(HOVER_TIMER_ID)` and `HideActiveTooltip()` use ViewHost |
| Cursors | `VH_SetCursor(DmsCursor)` | Maps DmsCursor enum to Qt::CursorShape | `SetViewPortCursor(DmsCursor)` on MovableObject; used by ViewPort, TableControl, DataItemColumn, TextControl |
| Drawing | `VH_DrawInContext(Region&, callback)` | QPainter on backing store QImage | Caret insert/remove/move |
| Caret overlay | `VH_SetCaretOverlay(Region&, bool)` | XOR compositing in paintEvent | Selection/graphical caret display |
| Timers | `VH_SetTimer` / `VH_KillTimer` | QTimer per ID | Hover, update, tip watchdog |
| Text caret | `VH_CreateTextCaret` / `VH_SetTextCaretPos` / `VH_Show/HideTextCaret` / `VH_DestroyTextCaret` | Manual drawing in paintEvent | Text editing cursor |
| Invalidation | `VH_InvalidateRect` / `VH_InvalidateRgn` / `VH_ValidateRect` | `QWidget::update()` / `repaint()` | Incremental redraw |
| Scroll | `VH_ScrollWindow` | Backing store blit + expose region | Hardware-accelerated scroll |
| Capture | `VH_SetCapture` / `VH_ReleaseCapture` | `grabMouse()` / `releaseMouse()` | Drag operations |
| Focus | `VH_SetFocus` | `setFocus()` | Window activation |
| Cursor pos | `VH_GetCursorScreenPos` / `VH_ScreenToClient` / `VH_ClientToScreen` / `VH_SetGlobalCursorPos` | QCursor/mapFromGlobal/mapToGlobal | Tooltip positioning, hit testing |
| Window state | `VH_IsVisible` / `VH_GetShowCmd` / `VH_SendClose` | QWidget state queries | View lifecycle |

### GeoDmsGuiQt compilation fixes

| File | Fix | Category |
|------|-----|----------|
| main_qt.cpp | `std::exception` -> `std::runtime_error`; `__argc`/`__argv` -> static storage; COPYDATASTRUCT/nativeEventFilter/darkmode guarded; `MessageBoxA` -> `QMessageBox` | Portable types, platform guards |
| TestScript.h/cpp | `UINT32` -> `UInt32`; `stx_error` -> `std::runtime_error`; PassMsg guarded; ppltasks -> std::future/promise; `Sleep` -> `std::this_thread::sleep_for` | Portable types, platform guards |
| DmsMainWindow.cpp | `MessageBoxA` -> `QMessageBox`; `MessageBeep` -> `QApplication::beep()`; `setTabOrder` chained calls | Portable replacements |
| DmsExport.h/cpp | `driver_characteristics` member renamed to `m_driver_characteristics` | GCC name shadowing |
| DmsOptions.cpp | Registry usage guarded with `#ifdef _WIN32` | Platform guard |
| DmsViewArea.cpp | `OnSize` -> `OnResize` with `devicePixelRatioF()`; popup menu, tooltip, cursor implementations | Portable ViewHost |
| ~17 Qt include files | Case-sensitive includes fixed (QCheckbox->QCheckBox, QMenubar->QMenuBar, etc.) | Linux filesystem |
| CMakeLists.txt | Added `CMAKE_AUTOUIC_SEARCH_PATHS` for .ui files in res/ui/ | Build fix |
| ElemTraits.h | `is_numeric<bool>` added `: std::false_type` base | GCC strictness |

## Remaining Win32-only code in DmShv

These areas are still guarded with `#ifdef _WIN32` and compile out on Linux.

### Not needed — platform backends (intentionally Win32-only)

- `GdiDrawContext.cpp` — Win32 GDI DrawContext (Qt equivalent: `QtDrawContext`)
- `DcHandle.h/cpp` — RAII wrappers for HDC/HFONT/HBRUSH (not needed on Qt)
- `Win32ViewHost.cpp` — Win32 ViewHost (Qt equivalent: `QDmsViewArea`)
- `dataview.cpp: DispatchMsg()` (~330 lines) — Win32 message loop (Qt uses QWidget events)
- `dataview.cpp: OnPaint/OnEraseBkgnd` — Win32 paint cycle (Qt uses `paintEvent`)
- `dataview.cpp: InvalidateDeviceRect/InvalidateRgn/ValidateRect` — (Qt uses `update()`)
- `dataview.cpp: CreateMdiChild/ResetHWnd/DestroyWindow/PostGuiOper` — Win32 lifecycle
- `ShvDllInterface.cpp: SHV_DataView_DispatchMessage/DllMain` — Win32 DLL entry points

### Medium priority — needed for full interactive use

- **Font management** (`FontIndexCache.cpp`, `DataItemColumn.cpp: GetFont()`, `dataview.cpp: GetDefaultFont()`) — GDI HFONT/LOGFONT. Qt path uses `DrawContext::SetFont()`. Need portable `FontArray` for indexed font caching.
- **Pen management** (`PenIndexCache.cpp`) — GDI HPEN/CreatePen. Qt path uses `DrawContext::DrawPolyline()`. Need portable `PenArray` for indexed pen caching.
- **Clipboard** (`ClipBoard.cpp`, `GridLayer.cpp: CopySelValues/PasteSelValues*`) — Win32 clipboard. Qt equivalent: `QClipboard`/`QMimeData`.
- **Scroll bars** (`ScrollPort.cpp: SetScrollX/Y, CalcNewPos, OnHScroll/OnVScroll`) — Native Win32 scroll bar controls. Qt equivalent: `QScrollBar`.

### Low priority — polish / rare features

- **Bitmap export** (`ViewPort.cpp: Export/SaveBitmap`, `MovableObject.cpp: GetAsDDBitmap`) — Qt equivalent: `QImage::save()`.
- **Color dialog** (`DataItemColumn.cpp: ChooseColorDialog()`) — Qt equivalent: `QColorDialog::getColor()`.
- **GDI drawing utilities** (`ShvUtils.cpp: FillRectWithBrush, ShadowRect`, etc.) — 3D border painting.
- **DPI scaling** (`ShvUtils.cpp: GetWindowDip2PixFactor*`) — Qt uses `devicePixelRatioF()` (already working).
- **Grid bitmap rendering** (`GridDrawer.cpp: BITMAPINFO/RGBQUAD`) — Qt: `QImage` with `DrawContext::DrawImage()` (already implemented in QtDrawContext).

### Dead code (no callers, can be removed)

- `TextControl.cpp: DrawSymbol(), TextOutLimited(), DrawTextWithCursor(), DrawText()` (free functions) — legacy GDI text rendering, superseded by `DrawContext::TextOut()/TextOutW()`. Zero callers outside own `#ifdef` block.
- `TextControl.h/cpp: AbstrTextEditControl::DrawEditText()` — declared in header, zero callers in entire codebase.

## TODO

### Before merging to v17

- [ ] Verify Windows MSVC build — no regressions from refactored `#ifdef` guards
- [ ] Remove dead code: `TextControl.cpp` GDI free functions, `AbstrTextEditControl::DrawEditText()`
- [ ] Runtime test on Linux: launch GeoDmsGuiQt, open a configuration, verify map/table views render

### Medium priority — needed for full interactive Linux use

- [ ] **Portable font caching** — Replace `FontIndexCache` (GDI HFONT/LOGFONT) with a portable `FontArray` backed by `DrawContext::SetFont()`. Affects `DataItemColumn::GetFont()` and `dataview.cpp::GetDefaultFont()`.
- [ ] **Portable pen caching** — Replace `PenIndexCache` (GDI HPEN) with a portable `PenArray` backed by `DrawContext::DrawPolyline()`.
- [ ] **Clipboard** — Port `ClipBoard.cpp` and `GridLayer.cpp: CopySelValues/PasteSelValues*` to `QClipboard`/`QMimeData`.
- [ ] **Scroll bars** — Port `ScrollPort.cpp: SetScrollX/Y, CalcNewPos, OnHScroll/OnVScroll` to `QScrollBar` (likely in `QDmsViewArea` or a wrapper).

### Low priority — polish / rare features

- [ ] **Bitmap export** — Port `ViewPort::Export()/SaveBitmap` and `MovableObject::GetAsDDBitmap` using `QImage::save()`.
- [ ] **Color dialog** — Port `DataItemColumn::ChooseColorDialog()` using `QColorDialog::getColor()`.
- [ ] **GDI drawing utilities** — Port `ShvUtils.cpp: FillRectWithBrush, ShadowRect` etc. (3D borders) if needed.
- [ ] **Grid bitmap rendering** — Verify `GridDrawer.cpp` works via `QtDrawContext::DrawImage()` on Linux (may already work).
