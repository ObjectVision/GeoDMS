// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_GDIREGIONUTIL_H)
#define __SHV_GDIREGIONUTIL_H

//----------------------------------------------------------------------
// GDI ↔ QRegion conversion utilities.
// Used by Win32-specific code that still needs HRGN handles
// (e.g. Win32ViewHost, GdiDrawContext, DcHandle).
//----------------------------------------------------------------------

#include "DcHandle.h"  // GdiHandle<HRGN>
#include "Region.h"

inline GdiHandle<HRGN> QRegionToHRGN(const QRegion& qrgn)
{
	if (qrgn.isEmpty())
		return GdiHandle<HRGN>();

	// Build HRGN from QRegion rect decomposition
	auto it = qrgn.begin();
	auto end = qrgn.end();
	assert(it != end);

	const QRect& first = *it;
	HRGN result = CreateRectRgn(first.x(), first.y(), first.x() + first.width(), first.y() + first.height());
	++it;

	for (; it != end; ++it)
	{
		const QRect& qr = *it;
		HRGN rectRgn = CreateRectRgn(qr.x(), qr.y(), qr.x() + qr.width(), qr.y() + qr.height());
		CombineRgn(result, result, rectRgn, RGN_OR);
		DeleteObject(rectRgn);
	}
	return GdiHandle<HRGN>(result, nullptr); // non-throwing ctor
}

inline GdiHandle<HRGN> RegionToHRGN(const Region& rgn)
{
	return QRegionToHRGN(rgn.GetQRegion());
}

inline QRegion HRGNToQRegion(HRGN hrgn)
{
	if (!hrgn)
		return QRegion();

	DWORD size = GetRegionData(hrgn, 0, nullptr);
	if (!size)
		return QRegion();

	std::vector<BYTE> buf(size);
	RGNDATA* data = reinterpret_cast<RGNDATA*>(buf.data());
	GetRegionData(hrgn, size, data);

	QRegion result;
	RECT* rects = reinterpret_cast<RECT*>(data->Buffer);
	for (DWORD i = 0; i < data->rdh.nCount; ++i)
	{
		result |= QRegion(QRect(rects[i].left, rects[i].top,
			rects[i].right - rects[i].left, rects[i].bottom - rects[i].top));
	}
	return result;
}

//----------------------------------------------------------------------
// Factory functions for Region from Win32-specific sources
//----------------------------------------------------------------------

inline Region RegionFromUpdateRgn(HWND hWnd)
{
	HRGN hrgn = CreateRectRgn(0, 0, 0, 0);
	GetUpdateRgn(hWnd, hrgn, false);
	QRegion result = HRGNToQRegion(hrgn);
	DeleteObject(hrgn);
	return Region(std::move(result));
}

inline Region RegionFromSystemClipRgn(HDC hdc, HWND hWnd)
{
	HRGN hrgn = CreateRectRgn(0, 0, 0, 0);
	GetRandomRgn(hdc, hrgn, SYSRGN);

	// The region returned by GetRandomRgn is in screen coordinates
	POINT pt = {0, 0};
	ScreenToClient(hWnd, &pt);
	OffsetRgn(hrgn, pt.x, pt.y);

	QRegion result = HRGNToQRegion(hrgn);
	DeleteObject(hrgn);
	return Region(std::move(result));
}

inline Region RegionFromDCClipBox(HDC hdc, const GRect& rect)
{
	GRect clipRect;
	switch (GetClipBox(hdc, &AsRECT(clipRect)))
	{
		case SIMPLEREGION:
		case COMPLEXREGION:
			clipRect &= rect;
			if (!clipRect.empty())
				return Region(clipRect);
			break;
		case ERROR:
		case NULLREGION:
			break;
	}
	return Region();
}

#endif // __SHV_GDIREGIONUTIL_H
