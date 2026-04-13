# GeoDMS Linux GUI Porting Status

> **Branch:** `refactor_linux_gui`
> **Last updated:** 2025-07-05 (commit 5498c583)
> **Goal:** Port `shv.dll` (ShvDLL) and `GeoDmsGuiQt.exe` to Linux by replacing all Win32 GDI with QPainter/Qt.

## Overall Strategy

Replace **all** GDI drawing with QPainter — even on Windows. No dual GDI/Qt rendering paths.
The Win32 `ViewHost` implementation is kept temporarily but will be removed once `DmsViewArea` (Qt) covers all functionality.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  GeoDmsGuiQt.exe (Qt application)                       │
│  ├── DmsViewArea : QWidget  (qtgui/exe/src/)            │
│  │   └── implements ViewHost interface                  │
│  │   └── creates QtDrawContext for painting             │
│  ├── QtDrawContext : DrawContext  (qtgui/exe/src/)       │
│  │   └── QPainter-based drawing                         │
│  └── DmsMainWindow (Qt UI shell)                        │
├─────────────────────────────────────────────────────────┤
│  shv.dll (shared visualization logic)                   │
│  ├── DrawContext (abstract base)  → shv/dll/src/DrawContext.h  │
│  ├── GdiDrawContext : DrawContext → Win32-only, transitional   │
│  ├── ViewHost (abstract base)     → shv/dll/src/ViewHost.h    │
│  ├── Win32ViewHost : ViewHost     → Win32-only, transitional  │
│  ├── Region (QRegion-backed)      → shv/dll/src/Region.h      │
│  ├── GdiRegionUtil.h              → Win32-only HRGN↔QRegion   │
│  ├── GeoTypes.h                   → standalone GPoint/GRect   │
│  └── DcHandle.h                   → Win32-only HDC wrappers   │
└─────────────────────────────────────────────────────────┘
```

## Completed Steps

### Step 1: ViewHost abstraction [5ed8ea93]
- Created abstract `ViewHost` interface in `shv/dll/src/ViewHost.h`
- Created `Win32ViewHost` implementing ViewHost for Win32 HWND
- `DmsViewArea` (Qt) also implements ViewHost
- `DataView` uses ViewHost* instead of direct HWND calls

### Step 2a: DrawContext abstraction [a2472124]
- Created abstract `DrawContext` base class in `shv/dll/src/DrawContext.h`
- Created `GdiDrawContext` (non-owning HDC wrapper) for Win32
- `GraphDrawer` stores `DrawContext*` via `GetDrawContext()`

### Step 2b: Abstract drawing methods [516b3655]
- Added `FillRect`, `FillRegion`, `InvertRect`, `DrawFocusRect` to DrawContext
- `GdiDrawContext` implements via GDI
- Migrated first callsites: MovableContainer, GraphicObject, DataItemColumn, TextControl

### Step 2c: Polymorphic DrawContext in GraphDrawer [8236a79b]
- `GraphDrawer` uses polymorphic `DrawContext*`
- `QtDrawContext` (in qtgui/) implements DrawContext with QPainter

### Step 2d: QRegion everywhere [62a6ebb1]
- `Region` class now backed by `QRegion` (was HRGN-based)
- Created `GdiRegionUtil.h` for transitional HRGN↔QRegion conversion (Win32-only)
- Qt 6.9 integrated in ShvDLL.vcxproj (modules: core;gui)
- UNICODE conflict resolved via `UndefinePreprocessorDefinitions=UNICODE;_UNICODE`

### Step 3: Standalone GPoint/GRect [27eb30e6]
- `GPoint` and `GRect` no longer inherit from Win32 `POINT`/`RECT`
- `GType` changed from `typedef LONG` to `using GType = Int32` (portable)
- Win32 interop helpers behind `#ifdef _WIN32` in GeoTypes.h:
  - `AsPOINT()`/`AsRECT()` — reinterpret_cast (for passing to Win32 APIs)
  - `POINTToGPoint()`/`RECTToGRect()` — value conversion
  - `static_assert(sizeof(Int32) == sizeof(LONG))` for layout safety
- `HitTest(POINT)` → `HitTest(GPoint)` in GraphicObject/MovableObject
- `PolygonCaret` changed from `const POINT*` to `const GPoint*`
- `GPoint::ScreenToClient()` method → free function `ScreenToClientGPoint()` in ShvUtils
- RGBQUAD/COLORREF guarded behind `#ifdef _WIN32`; portable `using COLORREF = UInt32` in `#else`
- All Win32 API call sites (22 files) wrapped with AsPOINT/AsRECT

### Step 3b: Guard windows.h in ShvBase.h [uncommitted]
- `#include <windows.h>` in ShvBase.h now behind `#ifdef _WIN32`
- `MsgResult` (uses LRESULT), `MG_WM_SIZE` (uses WM_SIZE), `g_ShvDllInstance` (HINSTANCE) all guarded
- No Linux equivalent needed for `MsgResult` — Qt event system replaces Win32 message dispatch
- Builds successfully on Windows (clean rebuild verified)

### Step 3c: QDmsViewArea implements ViewHost [5498c583]
- `QDmsViewArea` now inherits from `ViewHost` (multiple inheritance with `QMdiSubWindow`)
- Implements all `VH_*` methods using Qt APIs:
  - Timer: `QTimer` for `VH_SetTimer`/`VH_KillTimer`, calls `DataView::OnTimer()`
  - Capture: `grabMouse()`/`releaseMouse()` for `VH_SetCapture`/`VH_ReleaseCapture`
  - Focus: `setFocus()` for `VH_SetFocus`
  - Cursor: `QCursor::pos()`, `mapFromGlobal()`, `mapToGlobal()` for cursor position methods
  - Invalidation: `update()` for `VH_InvalidateRect`/`VH_InvalidateRgn`
  - Synchronous paint: `repaint()` for `VH_UpdateWindow`
  - Visibility: `isVisible()`, window state queries for `VH_IsVisible`/`VH_GetShowCmd`
  - Text caret: Custom state tracking (Qt has no native caret API)
  - Mouse tracking: `setMouseTracking(true)` for `VH_TrackMouseLeave`
- Removed `Win32ViewHost` member from `QDmsViewArea`, now passes `this` as ViewHost
- Added `DataView::OnTimer(UInt32)` public method with `SHV_CALL` export
- Added `SHV_CALL` to `Region` struct for proper DLL export
- Fixed `ClipBoard::GetText()` to use `CharPtrRange` constructor
- **Windows build: OK**

## Next Steps

### Step 4: Remove GDI wrappers
**Goal:** Eliminate GdiDrawContext, DcHandle, and all remaining direct GDI calls.

#### Step 4a: DrawContext line/shape/text interface + first migrations [aeff614d..14a1c511]
- Extended DrawContext with 12 new abstract methods:
  - Lines: `DrawLine`, `DrawPolyline`
  - Polygons: `DrawPolygon` with `DmsHatchStyle` enum
  - Text: `TextOut`, `DrawText`, `GetTextExtent`, `SetTextColor`, `SetBkColor`, `SetBkMode`
  - Regions: `InvertRegion`
  - Clipping: `GetClipRect`
  - Portable enums: `DmsPenStyle`, `DmsHatchStyle`
- Implemented in both GdiDrawContext (Win32, transitional) and QtDrawContext (QPainter)
- GdiDrawContext guarded behind `#ifdef _WIN32`; `GetHDC()` now Win32-only
- Migrated caret system: `AbstrCaret::Reverse/Move` take `DrawContext&` instead of `HDC`
  - All caret subclasses updated: NeedleCaret, FocusCaret, LineCaret, PolygonCaret,
    InvertRgnCaret, RectCaret, MovableRectCaret, RoiCaret, CircleCaret, ScaleBarCaret
  - DataView caret methods wrap CaretDcHandle in GdiDrawContext
- Migrated drawing files:
  - `DrawPolygons.h` — Polygon/Polyline via DrawContext (removed PenArray usage)
  - `FeatureLayer.cpp` — network/arc Polyline via DrawContext (removed PenArray usage)
  - `ScaleBar.cpp` — Draw() uses DrawContext::FillRect/DrawText
  - `Carets.cpp` — all GDI removed, uses DrawContext::DrawLine/DrawPolygon/InvertRegion
  - `GraphicGrid.cpp` — DmsColor members, DrawContext::FillRect/DrawLine
  - `GraphicRect.cpp` — removed AlphaBlend/CreateBlender, uses DrawContext
  - `TextControl.cpp` — Draw() uses DrawContext::FillRect/TextOut/InvertRect
  - `DataItemColumn.cpp` — DrawBackground uses DrawContext::FillRect
- Added `PenIndexCache::GetPenKey()`/`IsPenVisible()` for portable pen property lookup

#### Step 4b: All remaining GetDC() calls migrated or guarded [72c3ce19..6b03e6b8]
- DrawContext extended with: SetClipRegion/SetClipRect/ResetClip, DrawButtonBorder/DrawReversedBorder, DrawEllipse
- DcClipRegionSelector: changed from HDC to DrawContext* (portable clipping)
- GraphVisitor.cpp: clip regions, borders, text color all via DrawContext
- FeatureLayer.cpp: LabelDrawer uses DrawContext (no DcTextColorSelector/DcBackColorSelector)
- DrawPoints/DrawMultiPoints: SetTextColor via DrawContext, font/TextOutW in _WIN32
- DataItemColumn.cpp: DrawButtonBorder via DrawContext, symbol/text in _WIN32
- GraphicPoint.h: DrawEllipse via DrawContext
- GridLayer.cpp: focus drawing via DrawContext::FillRegion, GridDrawer HDC in _WIN32
- WmsLayer.cpp: GridDrawer HDC in _WIN32
- dataview.cpp: UpdateView draw loop, Scroll, InvalidateRgn, SelCaret all in _WIN32

**Result:** All `GetDC()` calls in shv/dll/src are now either:
- Behind `#if defined(_WIN32)` guards
- Inside comment blocks (dead code)
- In `GraphVisitor.h` definition (already _WIN32 guarded)

Linux clean rebuild (230/230 targets) and operator tests pass.

#### Step 4c: Cleanup (TODO)
Remaining work:
- Remove unused GDI utility functions from ShvUtils.h/cpp (DrawBorder, FillRectDmsColor etc.)
- Guard DcHandle.h contents with _WIN32
- Guard PenArray in _WIN32 (only used by legacy code now)
- Clean up FontIndexCache/SelectingFontArray for portable font handling
- Replace GridDrawer StretchDIBits with portable QImage rendering for Qt path

### Step 5: Linux build (CMake)
**Goal:** Build shv.dll and GeoDmsGuiQt on Linux via CMake + WSL.

#### Step 5a: CMakeLists.txt for shv/dll and qtgui/exe
- Created `shv/dll/CMakeLists.txt`:
  - `DmShv` shared library, sources globbed from `src/*.cpp`
  - Win32-only files (`Win32ViewHost.cpp`, `GdiDrawContext.cpp`, `DcHandle.cpp`) excluded on non-WIN32
  - `DM_SHV_EXPORTS` compile definition (matches `ShvDllPch.h` / `ShvBase.h` guard)
  - Links all DLL targets: DmRtc, DmSym, DmTic, DmStx, DmStg, DmClc, DmGeo
  - Links Qt6::Core, Qt6::Gui (for QRegion, QRect, QPoint)
  - Win32: links Shcore, comctl32, msimg32
- Created `qtgui/exe/CMakeLists.txt`:
  - `GeoDmsGuiQt` executable with AUTOMOC/AUTORCC/AUTOUIC
  - Processes `GeoDmsGuiQt.qrc` and `res/ui/*.ui` files
  - Links all DLL targets + DmShv + Qt6::Core, Qt6::Gui, Qt6::Widgets
  - Win32: links Shcore, shell32; sets WIN32_EXECUTABLE
  - Deploys font files to `bin/misc/fonts/`
- Added `add_subdirectory(shv/dll)` and `add_subdirectory(qtgui/exe)` to top-level CMakeLists.txt

#### Step 5b: Linux build fixes + portable QClipboard [ff1e53aa]
- **ShvBase.h**: `SHV_CALL` empty on non-MSVC (matches `RtcBase.h` pattern for `RTC_CALL`)
- **ShvUtils.h**: `ScreenToClientGPoint(HWND)`, `DrawBorder/FillRect` HDC functions, `GetWindowDip2PixFactor(HWND)` all behind `#ifdef _WIN32`; portable `UNDEFINED_WCHAR` as `char16_t` on Linux
- **DcHandle.h**: Entire content behind `#ifdef _WIN32` (excluded from Linux build via CMake, but header still included transitively)
- **PenIndexCache.h/.cpp**: `PenArray` struct behind `#ifdef _WIN32`; portable `PS_*` constants (`#ifndef PS_SOLID`) for `PenIndexCache` key computation
- **GeoTypes.h**: Fixed `IsDefined(GPoint)` forward-declaration order — `GRect` constructor assert expanded to member-level `IsDefined(p.x)` calls
- **Clipboard.h/.cpp**: Rewritten with `QClipboard` (Qt) for portable text operations:
  - `GetText()`, `SetText()`, `AddText()`, `AddTextLine()`, `Clear()`, `ClearBuff()` all use `QGuiApplication::clipboard()`
  - UTF-8↔QString conversion replaces Win32 `MultiByteToWideChar`/`GlobalAlloc`
  - Win32-only: `GlobalLockHandle`, `SetBitmap(HBITMAP)`, `SetDIB(HBITMAP)`, `GetData(UINT)`, `SetData(UINT,...)`, `GetCurrFormat()` kept behind `#ifdef _WIN32`
  - Free function `ClipBoard_SetData(ClipBoard&, UINT, GlobalLockHandle&)` replaces removed member overload
- **GridLayer.h/.cpp**: `PasteHandler`, `CopySelValues()`, `PasteSelValues*()`, `PasteNow()`, `ClearPaste()`, `InvalidatePasteArea()`, `DrawPaste()`, `m_PasteHandler`, `CF_CELLVALUES` all behind `#ifdef _WIN32`
- **MovableObject.h/.cpp**: `HBITMAP GetAsDDBitmap()`, `HCURSOR` members, `CopyToClipboard()` implementation behind `#ifdef _WIN32` (Linux stub provided for `CopyToClipboard`)
- **dataview.h / GraphVisitor.h**: Unscoped forward-declared enums (`ViewStyle`, `ViewStyleFlags`, `ToolButtonID`, `ShvSyncMode`, `GdMode`) given explicit `: int` underlying type (GCC requirement)
- **Aspect.h**: `AspectNr : int` (same GCC fix)
- **LispList.h**: `operator<<` changed from by-value to `const&` parameter (fixes GCC overload ambiguity with `AssocListPtrWrap`)

**Result:** `DmShv` compiles clean on Linux (GCC 13, WSL). Windows MSVC build: OK.

#### Step 5b continued: Additional Win32 guards [c012f78d+]
- **DataItemColumn.cpp**: `ChooseColorDialog`, `GetFont(HFONT)`, `m_FontArray` usage all behind `#ifdef _WIN32`; Linux stub for `ChooseColorDialog` returns false
- **DataItemColumn.h**: `FontArray` forward-decl and member behind `#ifdef _WIN32`
- **ShvCompat.h**: Added `COLOR_BTNFACE`, `CLR_INVALID`, `COLOR_HIGHLIGHT`/`TEXT`, `GetSysColor` stub, `MB_ICONEXCLAMATION`/`YESNO`, `MessageBoxA` stub; `WCHAR = wchar_t` (was `char16_t`)
- **ShvUtils.cpp**: `GetInstance(HWND)`, `ScreenToClientGPoint(HWND)`, `CheckedGdiCall`/`FillRectWithBrush`/`ShadowRect`/`DrawButtonBorder`/`DrawReversedBorder`/`DrawRectDmsColor`/`FillRectDmsColor` block, `GetWindowEffectiveDPI`/`GetWindowDip2PixFactor*` functions all behind `#ifdef _WIN32`
- **ScrollPort.cpp**: `SetScrollX/Y`, `RePosScrollBar`, `CalcNewPosBase`, `CalcNewPos`, `OnHScroll/OnVScroll`, scrollbar info code all behind `#ifdef _WIN32`; Linux stubs provided
- **TableControl.cpp**: `SetViewPortCursor(LoadCursor(...))` call behind `#ifdef _WIN32`
- **Theme.cpp**: Removed `static` keyword from `CreateValue`/`CreateVar` template definitions (GCC requires static only on declaration)
- **ViewPort.cpp**: Constructor `SetViewPortCursor`, `SaveBitmap`+`Export` functions, `PasteGridController` class+`PasteGrid` function, `OnCommand` cursor changes all behind `#ifdef _WIN32`
- **DcHandle.h**: Moved `AddTransformation`, `AddClientLogicalOffset`, `ClipDeviceRectSelector`, `VisitorDeviceRectSelector` to `GraphVisitor.h` (portable helper structs)
- **GraphVisitor.h**: Added portable helper structs; now includes `utl/Swapper.h` and conditionally includes `DcHandle.h` on Win32
- **dataview.cpp**: Guard `CommCtrl.h` includes, `LParam2Point`, `MsgStruct::Send`, `m_hWnd` initializer, destructor `DestroyWindow` call, large Win32 blocks (`DestroyWindow`→`ReverseSelCaretImpl`, `IsTooltipKiller`→`OnKeyDown`, `UpdateWindow(GetHWnd())` call)
- **ShvDllInterface.cpp**: `DllMain`/`g_ShvDllInstance` behind `#ifdef _WIN32`, added `Environment.h` include for `g_DispatchLockCount`

**Status:** Linux build errors reduced from ~400 to ~149 across 7 files. Windows MSVC: clean.

#### Step 5c: Remaining tasks (TODO)
- Build `GeoDmsGuiQt` on Linux (resolve qtgui/exe compilation issues)
- Handle platform differences: shared library loading, font paths, etc.

## Key Technical Decisions

| Decision | Rationale |
|----------|-----------|
| QPainter everywhere (no GDI even on Windows) | Single rendering path, simpler maintenance |
| No dual ViewHost | Qt's DmsViewArea replaces Win32ViewHost entirely on Linux |
| QRegion as Region backing store | Qt provides cross-platform region operations |
| GPoint/GRect as standalone structs | Removes POINT/RECT inheritance dependency on windows.h |
| AsPOINT/AsRECT reinterpret_cast | Layout-compatible (static_assert verified), zero-cost for transitional Win32 interop |
| COLORREF portable fallback | `using COLORREF = UInt32` — same bit layout as Win32 COLORREF |

## Win32-Only Files (to be removed or replaced in Step 4-5)

| File | Purpose | Replacement |
|------|---------|-------------|
| `Win32ViewHost.h/.cpp` | HWND-based ViewHost | DmsViewArea (Qt) |
| `GdiDrawContext.cpp` | HDC-based DrawContext | QtDrawContext |
| `DcHandle.h/.cpp` | HDC/HFONT/HPEN/HBRUSH RAII | QPainter state management |
| `GdiRegionUtil.h` | HRGN↔QRegion conversion | Remove (no HRGN on Linux) |
| `ShvDllInterface.h/.cpp` | DLL exports with HWND/LRESULT params | Qt-native API or conditional |

## Qt Integration Details

- **Qt version:** 6.9.0 (msvc2022_64 on Windows)
- **Qt modules used:** core, gui (ShvDLL); core, gui, widgets (GeoDmsGuiQt)
- **QtMsBuild:** `$env:LOCALAPPDATA\QtMsBuild` (Windows VS integration)
- **UNICODE workaround:** Qt defines UNICODE/\_UNICODE; ShvDLL counters with `UndefinePreprocessorDefinitions`
- **Qt in ShvDLL:** Only for QRegion and QRect (lightweight, no QWidget dependency)

## File Modification Summary (all steps)

### GeoTypes.h (core geometry)
- GPoint: standalone `{ GType x, y; }` (was `: POINT`)
- GRect: standalone `{ GType left, top, right, bottom; }` (was `: RECT`)
- `using GType = Int32` (was `typedef LONG GType`)
- `#ifdef _WIN32`: AsPOINT/AsRECT, POINTToGPoint/RECTToGRect, RGBQUAD, COLORREF
- `#else`: `using COLORREF = UInt32`, portable DmsColor2COLORREF/COLORREF2DmsColor

### Region.h/.cpp (region operations)
- Backed by QRegion (was HRGN)
- All set operations (union, intersect, subtract, XOR) via QRegion
- `GetQRegion()` accessor for Qt interop

### ShvBase.h (shared header)
- `#include <windows.h>` guarded behind `#ifdef _WIN32`
- `MsgResult`, `MG_WM_SIZE`, `g_ShvDllInstance` guarded behind `#ifdef _WIN32`

### ViewHost.h (abstraction layer)
- Abstract interface: cursor, scroll, invalidation, coordinate transform
- Implemented by Win32ViewHost (HWND) and DmsViewArea (QWidget)

### DrawContext.h (abstraction layer)
- Abstract interface: FillRect, FillRegion, InvertRect, DrawFocusRect
- Implemented by GdiDrawContext (HDC) and QtDrawContext (QPainter)
