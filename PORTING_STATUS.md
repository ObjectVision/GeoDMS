# GeoDMS Linux GUI Porting Status

> **Branch:** `refactor_linux_gui`
> **Last updated:** 2025-07-05 (commit 27eb30e6 + uncommitted ShvBase.h guard)
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

## Next Steps

### Step 4: Remove GDI wrappers
**Goal:** Eliminate GdiDrawContext, DcHandle, and all remaining direct GDI calls.

Key tasks:
- Migrate all remaining GDI drawing calls to DrawContext virtual methods
- Move GDI-specific code (Polygon, Polyline, FillRect, TextOut, etc.) behind DrawContext
- `QtDrawContext` already exists — extend it to cover all drawing operations
- Remove `GdiDrawContext.cpp` once all callsites use DrawContext
- Remove or deprecate `DcHandle.h` / `DcHandle.cpp` (HDC/HFONT/HPEN/HBRUSH wrappers)
- Remove `GdiRegionUtil.h` (HRGN↔QRegion — only needed by GDI code)

Files with remaining direct GDI calls to migrate:
- `DrawPolygons.h` — Polygon(), Polyline() via GDI (→ QPainter::drawPolygon/drawPolyline)
- `FeatureLayer.cpp` — Polyline() for link drawing
- `ScaleBar.cpp` — DrawText() via GDI (→ QPainter::drawText)
- `TextControl.cpp` — TextOut, GetCurrentPositionEx, MoveToEx
- `Carets.cpp` — MoveToEx, LineTo, DrawFocusRect, CreatePen
- `DcHandle.cpp` — all GDI object selectors (pen, brush, font, etc.)
- `GridLayer.cpp` — DIB/bitmap operations (StretchDIBits, etc.)
- `GraphicGrid.cpp` — FillRect with HBRUSH
- `GraphicRect.cpp` — FillRect, AlphaBlend
- `ViewPort.cpp` — HBITMAP, CompatibleDcHandle, GetAsDDBitmap

### Step 5: Linux build (CMake)
**Goal:** Build shv.dll and GeoDmsGuiQt on Linux via CMake + WSL.

Key tasks:
- Create CMakeLists.txt for shv/dll (find Qt6, link Qt6::Core Qt6::Gui)
- Create CMakeLists.txt for qtgui/exe (link Qt6::Widgets)
- Resolve any remaining `#ifdef _WIN32` gaps
- Handle platform differences: shared library loading, font paths, clipboard, etc.

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
