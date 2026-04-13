// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "Win32ViewHost.h"

#include "DrawContext.h"
#include "DcHandle.h"
#include "Region.h"
#include "GdiRegionUtil.h"
#include "windowsx.h"
#include "CommCtrl.h"

Win32ViewHost::Win32ViewHost(HWND hWnd)
	: m_hWnd(hWnd)
{
}

void Win32ViewHost::VH_SetTimer(UInt32 id, UInt32 elapseMs)
{
	::SetTimer(m_hWnd, id, elapseMs, nullptr);
}

void Win32ViewHost::VH_KillTimer(UInt32 id)
{
	::KillTimer(m_hWnd, id);
}

void Win32ViewHost::VH_SetCapture()
{
	::SetCapture(m_hWnd);
}

void Win32ViewHost::VH_ReleaseCapture()
{
	::ReleaseCapture();
}

void Win32ViewHost::VH_SetFocus()
{
	::SetFocus(m_hWnd);
}

bool Win32ViewHost::VH_GetCursorScreenPos(GPoint& pos) const
{
	return ::GetCursorPos(&AsPOINT(pos));
}

GPoint Win32ViewHost::VH_ScreenToClient(GPoint screenPt) const
{
	::ScreenToClient(m_hWnd, &AsPOINT(screenPt));
	return screenPt;
}

GPoint Win32ViewHost::VH_ClientToScreen(GPoint clientPt) const
{
	::ClientToScreen(m_hWnd, &AsPOINT(clientPt));
	return clientPt;
}

void Win32ViewHost::VH_SetGlobalCursorPos(GPoint screenPt)
{
	::SetCursorPos(screenPt.x, screenPt.y);
}

void Win32ViewHost::VH_SetCursorArrow()
{
	SetCursor(LoadCursor(NULL, IDC_ARROW));
}

void Win32ViewHost::VH_SetCursorWait()
{
	SetCursor(LoadCursor(NULL, IDC_WAIT));
}

void Win32ViewHost::VH_InvalidateRect(const GRect& rect, bool erase)
{
	::InvalidateRect(m_hWnd, &AsRECT(rect), erase);
}

void Win32ViewHost::VH_InvalidateRgn(const Region& rgn, bool erase)
{
	auto hrgn = RegionToHRGN(rgn);
	::InvalidateRgn(m_hWnd, hrgn, erase);
}

void Win32ViewHost::VH_ValidateRect(const GRect& rect)
{
	::ValidateRect(m_hWnd, &AsRECT(rect));
}

void Win32ViewHost::VH_UpdateWindow()
{
	::UpdateWindow(m_hWnd);
}

void Win32ViewHost::VH_PostMessage(UInt32 msg, UInt64 wParam, Int64 lParam)
{
	::PostMessage(m_hWnd, msg, wParam, lParam);
}

void Win32ViewHost::VH_SendClose()
{
	::SendMessage(m_hWnd, WM_CLOSE, 0, 0);
}

bool Win32ViewHost::VH_IsVisible() const
{
	return ::IsWindowVisible(m_hWnd);
}

UInt32 Win32ViewHost::VH_GetShowCmd() const
{
	WINDOWPLACEMENT wpl;
	wpl.length = sizeof(WINDOWPLACEMENT);
	CheckedGdiCall(
		GetWindowPlacement(m_hWnd, &wpl),
		"GetWindowPlacement"
	);
	if (wpl.showCmd == SW_SHOWNORMAL)
		CheckedGdiCall(
			GetWindowPlacement(GetParent(m_hWnd), &wpl),
			"GetWindowPlacement"
		);
	return wpl.showCmd;
}

void Win32ViewHost::VH_CreateTextCaret(int width, int height)
{
	::CreateCaret(m_hWnd, NULL, width, height);
}

void Win32ViewHost::VH_DestroyTextCaret()
{
	::DestroyCaret();
}

void Win32ViewHost::VH_SetTextCaretPos(GPoint pos)
{
	::SetCaretPos(pos.x, pos.y);
}

void Win32ViewHost::VH_ShowTextCaret()
{
	::ShowCaret(m_hWnd);
}

void Win32ViewHost::VH_HideTextCaret()
{
	::HideCaret(m_hWnd);
}

void Win32ViewHost::VH_TrackMouseLeave()
{
	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = m_hWnd;
	if (!_TrackMouseEvent(&tme))
		throwLastSystemError("TrackMouseEvent");
}

void Win32ViewHost::VH_NotifyParentActivation()
{
	auto parent = GetAncestor(m_hWnd, GA_PARENT);
	SendMessage(parent, WM_QT_ACTIVATENOTIFIERS, 0, 0);
}

void Win32ViewHost::VH_ScrollWindow(GPoint delta, const GRect& scrollRect, const GRect& clipRect,
	Region& updateRgn, const GRect& validRect)
{
	if (!validRect.empty())
		::ValidateRect(m_hWnd, &AsRECT(validRect));

	GdiHandle<HRGN> hUpdateRgn(CreateRectRgn(0, 0, 0, 0), nullptr);
	ScrollWindowEx(m_hWnd,
		delta.x, delta.y,
		&AsRECT(scrollRect),
		&AsRECT(clipRect),
		hUpdateRgn,
		NULL,
		0
	);
	updateRgn = Region(HRGNToQRegion(hUpdateRgn));
}

HWND Win32ViewHost::VH_GetHWnd() const
{
	return m_hWnd;
}

void Win32ViewHost::VH_DrawInContext(const Region& clipRgn, std::function<void(DrawContext&)> callback)
{
	HDC hdc = ::GetDC(m_hWnd);
	if (!hdc)
		return;
	::SetBkMode(hdc, TRANSPARENT);
	auto hrgn = RegionToHRGN(clipRgn);
	::SelectClipRgn(hdc, hrgn);
	GdiDrawContext ctx(hdc);
	callback(ctx);
	::SelectClipRgn(hdc, NULL);
	::ReleaseDC(m_hWnd, hdc);
}

void Win32ViewHost::VH_SetCaretOverlay(const Region& rgn, bool visible)
{
	// Win32 uses immediate-mode XOR drawing, so caret overlay is handled
	// directly via VH_DrawInContext. This method is a no-op for Win32.
	// The caret state is managed by the caret objects themselves.
}
