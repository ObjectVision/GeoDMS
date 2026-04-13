// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_WIN32VIEWHOST_H)
#define __SHV_WIN32VIEWHOST_H

#include "ViewHost.h"
#include "GeoTypes.h"

//----------------------------------------------------------------------
// Win32 implementation of ViewHost using a native HWND.
// This is the initial implementation that preserves existing behavior.
//----------------------------------------------------------------------

class SHV_CALL Win32ViewHost : public ViewHost
{
public:
	explicit Win32ViewHost(HWND hWnd);

	void VH_SetTimer(UInt32 id, UInt32 elapseMs) override;
	void VH_KillTimer(UInt32 id) override;

	void VH_SetCapture() override;
	void VH_ReleaseCapture() override;

	void VH_SetFocus() override;

	bool VH_GetCursorScreenPos(GPoint& pos) const override;
	GPoint VH_ScreenToClient(GPoint screenPt) const override;
	GPoint VH_ClientToScreen(GPoint clientPt) const override;
	void VH_SetGlobalCursorPos(GPoint screenPt) override;

	void VH_SetCursorArrow() override;
	void VH_SetCursorWait() override;

	void VH_InvalidateRect(const GRect& rect, bool erase) override;
	void VH_InvalidateRgn(const Region& rgn, bool erase) override;
	void VH_ValidateRect(const GRect& rect) override;

	void VH_UpdateWindow() override;

	void VH_PostMessage(UInt32 msg, UInt64 wParam, Int64 lParam) override;
	void VH_SendClose() override;

	bool VH_IsVisible() const override;
	UInt32 VH_GetShowCmd() const override;

	void VH_CreateTextCaret(int width, int height) override;
	void VH_DestroyTextCaret() override;
	void VH_SetTextCaretPos(GPoint pos) override;
	void VH_ShowTextCaret() override;
	void VH_HideTextCaret() override;

	void VH_TrackMouseLeave() override;

	void VH_NotifyParentActivation() override;

	void VH_ScrollWindow(GPoint delta, const GRect& scrollRect, const GRect& clipRect,
		Region& updateRgn, const GRect& validRect) override;

	void VH_DrawInContext(const Region& clipRgn, std::function<void(DrawContext&)> callback) override;

	void VH_SetCaretOverlay(const Region& rgn, bool visible) override;

	HWND VH_GetHWnd() const override;

	void ResetHWnd(HWND hWnd) { m_hWnd = hWnd; }

private:
	HWND m_hWnd;
};

#endif // !defined(__SHV_WIN32VIEWHOST_H)
