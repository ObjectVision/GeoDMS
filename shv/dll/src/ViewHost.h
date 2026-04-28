// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_VIEWHOST_H)
#define __SHV_VIEWHOST_H

//----------------------------------------------------------------------
// Abstract interface for windowing operations.
// DataView uses this to perform platform-specific window operations
// without directly depending on Win32 HWND APIs.
// Implementations are provided by the host application (e.g. qtgui).
//----------------------------------------------------------------------

#include "ShvBase.h"
#include "GeoTypes.h"

#include <functional>

struct Region;
struct MenuData;
class DrawContext;

class SHV_CALL ViewHost
{
public:
	virtual ~ViewHost() = default;

	// Timer management
	virtual void VH_SetTimer(UInt32 id, UInt32 elapseMs) = 0;
	virtual void VH_KillTimer(UInt32 id) = 0;

	// Mouse capture
	virtual void VH_SetCapture() = 0;
	virtual void VH_ReleaseCapture() = 0;

	// Focus
	virtual void VH_SetFocus() = 0;

	// Cursor position
	virtual bool VH_GetCursorScreenPos(GPoint& pos) const = 0;
	virtual GPoint VH_ScreenToClient(GPoint screenPt) const = 0;
	virtual GPoint VH_ClientToScreen(GPoint clientPt) const = 0;
	virtual void VH_SetGlobalCursorPos(GPoint screenPt) = 0;

	// Cursor shape
	virtual void VH_SetCursorArrow() = 0;
	virtual void VH_SetCursorWait() = 0;
	virtual void VH_SetCursor(DmsCursor cursor) = 0;

	// Invalidation and validation
	virtual void VH_InvalidateRect(const GRect& rect, bool erase) = 0;
	virtual void VH_InvalidateRgn(const Region& rgn, bool erase) = 0;
	virtual void VH_ValidateRect(const GRect& rect) = 0;

	// Force synchronous paint processing
	virtual void VH_UpdateWindow() = 0;

	// Async operations
	virtual void VH_PostMessage(UInt32 msg, UInt64 wParam, Int64 lParam) = 0;
	virtual void VH_SendClose() = 0;

	// Visibility and window state
	virtual bool VH_IsVisible() const = 0;
	virtual UInt32 VH_GetShowCmd() const = 0;

	// Text caret (blinking cursor in text editors)
	virtual void VH_CreateTextCaret(int width, int height) = 0;
	virtual void VH_DestroyTextCaret() = 0;
	virtual void VH_SetTextCaretPos(GPoint pos) = 0;
	virtual void VH_ShowTextCaret() = 0;
	virtual void VH_HideTextCaret() = 0;

	// Mouse tracking (request leave notification)
	virtual void VH_TrackMouseLeave() = 0;

	// Parent notification
	virtual void VH_NotifyParentActivation() = 0;

	// Scroll
	virtual void VH_ScrollWindow(GPoint delta, const GRect& scrollRect, const GRect& clipRect,
		Region& updateRgn, const GRect& validRect) = 0;

	// Drawing: execute callback with a DrawContext for the backing store
	// The backing store contains model data only; carets are drawn as overlays.
	virtual void VH_DrawInContext(const Region& clipRgn, std::function<void(DrawContext&)> callback) = 0;

	// Tooltip
	virtual void VH_ShowTooltip(GPoint screenPoint, CharPtr utf8Text) = 0;
	virtual void VH_HideTooltip() = 0;

	// Context menu
	virtual void VH_ShowPopupMenu(GPoint clientPoint, const MenuData& menuData) = 0;

	// Copy the visible backing-store contents for the given device rect to the system clipboard (and save to /tmp/geodms_copy.png on Linux).
	// Default: no-op (Win32 handles clipboard copy per-object in MovableObject::CopyToClipboard).
	virtual void VH_CopyToClipboard(const GRect& /*rect*/) {}

	// Scroll bars: show/hide and set position/page/range for horizontal and vertical scroll bars.
	// Called by ScrollPort when content size changes or content is scrolled.
	// pos/page/max are in the same tick-unit space as the ScrollPort (1 tick = 1 logical unit for typical content).
	// Default: no-op (Win32 uses child HWND scroll bars created directly by ScrollPort).
	virtual void VH_SetHScrollBar(bool /*visible*/, int /*pos*/, int /*page*/, int /*max*/, int /*tick*/) {}
	virtual void VH_SetVScrollBar(bool /*visible*/, int /*pos*/, int /*page*/, int /*max*/, int /*tick*/) {}

	// Caret overlay: set region to be XOR-inverted on top of backing store during paint.
	// This keeps backing store clean with only model data representation.
	// Pass empty region to clear the overlay.
	virtual void VH_SetCaretOverlay(const Region& rgn, bool visible) = 0;

	// Get the native window handle (Win32-only, for transitional GDI code)
#ifdef _WIN32
	virtual HWND VH_GetHWnd() const = 0;
#endif
};

#endif // !defined(__SHV_VIEWHOST_H)
