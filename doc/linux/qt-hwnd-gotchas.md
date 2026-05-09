# Qt + native-HWND interaction gotchas in `qtgui/` and `shv/`

`GeoDmsGuiQt.exe` runs on both Windows and Linux. On **Windows** the
architecture is hybrid: each `DataView` creates its own native Win32 child
HWND (the `DmsView` window class, registered in
`qtgui/exe/src/DmsViewArea.cpp::RegisterViewAreaWindowClass`) that is
parented to a `QDmsViewArea` (a `QMdiSubWindow`). On **Linux** there is no
native HWND — `QDmsViewArea` does everything itself.

This split causes several non-obvious traps because two event-routing
systems (Win32 message pump + Qt event loop) need to cooperate without
stepping on each other. This document captures the ones we already paid
for. Read it before changing anything in `qtgui/exe/src/DmsViewArea.cpp`
or `shv/dll/src/dataview.cpp`.

## 1. Mouse capture must be on the native HWND, not on the Qt parent

**Symptom:** Mouse capture leaks. After certain drag operations, clicks on
the QMdiSubWindow chrome (`[x]` close button, the title bar, …) stop
working.

**Cause:** `WM_LBUTTONDOWN` reaches DataView's native child HWND first
(it is the topmost HWND under the cursor) and is processed by
`DataView::DispatchMsg`. If you call `m_ViewHost->VH_SetCapture()` there,
that ends up calling `grabMouse()` on `QDmsViewArea` — Qt then routes
**all** subsequent mouse events to `QDmsViewArea` instead of to the
native child. But `QDmsViewArea::mouseReleaseEvent` is `#ifndef _WIN32`,
so on Windows the `WM_LBUTTONUP` never reaches `DispatchMsg` and
`VH_ReleaseCapture()` is never called. Capture is stuck.

**Fix / convention:** Inside `DispatchMsg` (which is `#ifdef _WIN32`-only,
called only when the message is on the native child HWND), always use
**Win32 `SetCapture(m_hWnd)` / `ReleaseCapture()` directly** on the child
HWND. The `m_ViewHost->VH_SetCapture / VH_ReleaseCapture` calls are
intended for the Linux path where there is no native HWND.

## 2. Key events are NOT delivered to DataView via Qt by default

**Symptom:** Arrow keys eaten by `QMdiSubWindow`'s default handling, which
puts the sub-window into a move/resize mode. While in that mode the `[x]`
close button also stops responding.

**Cause:** `QDmsViewArea` does not override `keyPressEvent` by default,
so the events bubble up to `QMdiSubWindow`. On Windows the native child
HWND has focus only after a click on it, but Qt's focus management can
shift focus back to `QDmsViewArea`, so `WM_KEYDOWN` may never reach
DataView's `DispatchMsg`.

**Fix / convention:** `QDmsViewArea` overrides `keyPressEvent`. It
translates Qt key codes to DMS `virtKey` codes via `qtKeyToVK()` and
calls `dv->OnKeyDown(vk)`. **Always call `event->accept()`** on handled
keys to prevent `QMdiSubWindow`'s move/resize default behaviour. Also:
`setFocusPolicy(Qt::StrongFocus)` is required on `QDmsViewArea` so it
actually receives key events.

## 3. `DataView::OnKeyDown` is portable; `DispatchMsg` is Win32-only

**Background:** `DispatchMsg` (and `IsTooltipKiller`) live in a `#ifdef _WIN32 …
#endif` block in `dataview.cpp`. `OnKeyDown` was originally inside that block
but has been moved out (still in `dataview.cpp`). On Windows, `OnKeyDown`
queries Shift/Ctrl/Alt via `GetKeyState` (kept under `#ifdef _WIN32`); on
Linux, the caller (`QDmsViewArea::keyPressEvent`) encodes modifiers into
the `virtKey` using `KeyInfo::Flag::*` *before* calling `OnKeyDown`.

**Fix / convention:** When adding key handling, put platform-independent
code **outside** the `#ifdef _WIN32` block guarding `DispatchMsg`. The
closing `#endif` comment in `dataview.cpp` no longer references
`OnKeyDown` (since it moved out) — it should reference `DispatchMsg` only.

## 4. Two timer paths coexist on Windows-Qt

**Background:** `SetUpdateTimer()` calls `m_ViewHost->VH_SetTimer` when
`m_ViewHost` is set (always true on Windows-Qt and Linux). That uses a
`QTimer` whose timeout slot calls `DataView::OnTimer(UPDATE_TIMER_ID)`.

The `WM_TIMER` branch of `DataView::DispatchMsg` is therefore **dead
code** in the Qt build (both Windows and Linux) — it only executes if a
`DataView` is created without a `ViewHost` (legacy / test paths).

**Fix / convention:** When fixing timer-related issues, edit `OnTimer`
(the active path), **not** the `WM_TIMER` branch in `DispatchMsg`. The
`OnTimer` `UPDATE_TIMER_ID` branch already calls `SuspendTrigger::Resume()`
for the `m_ViewHost` path.
