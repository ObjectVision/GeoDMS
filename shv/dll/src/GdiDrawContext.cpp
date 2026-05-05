// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#if defined(_WIN32)

#include "DrawContext.h"

#include "DcHandle.h"
#include "GeoTypes.h"
#include "Region.h"
#include "GdiRegionUtil.h"

#include <vector>

GdiDrawContext::~GdiDrawContext()
{
	if (m_OwnedFont)
		::DeleteObject(m_OwnedFont);
}

void GdiDrawContext::FillRect(const GRect& rect, DmsColor color)
{
	GdiHandle<HBRUSH> br(CreateSolidBrush(DmsColor2COLORREF(color)));
	::FillRect(m_hDC, &AsRECT(rect), br);
}

void GdiDrawContext::FillRegion(const Region& rgn, DmsColor color)
{
	GdiHandle<HBRUSH> br(CreateSolidBrush(DmsColor2COLORREF(color)));
	auto hrgn = RegionToHRGN(rgn);
	::FillRgn(m_hDC, hrgn, br);
}

void GdiDrawContext::InvertRect(const GRect& rect)
{
	::InvertRect(m_hDC, &AsRECT(rect));
}

void GdiDrawContext::DrawFocusRect(const GRect& rect)
{
	::DrawFocusRect(m_hDC, &AsRECT(rect));
}

void GdiDrawContext::InvertRegion(const Region& rgn)
{
	auto hrgn = RegionToHRGN(rgn);
	::InvertRgn(m_hDC, hrgn);
}

static HPEN CreateDmsPen(DmsColor color, int width, DmsPenStyle style)
{
	int gdiStyle = PS_SOLID;
	switch (style) {
	case DmsPenStyle::Solid:      gdiStyle = PS_SOLID; break;
	case DmsPenStyle::Dash:       gdiStyle = PS_DASH; break;
	case DmsPenStyle::Dot:        gdiStyle = PS_DOT; break;
	case DmsPenStyle::DashDot:    gdiStyle = PS_DASHDOT; break;
	case DmsPenStyle::DashDotDot: gdiStyle = PS_DASHDOTDOT; break;
	case DmsPenStyle::Null:       gdiStyle = PS_NULL; break;
	}
	return CreatePen(gdiStyle, width, DmsColor2COLORREF(color));
}

void GdiDrawContext::DrawLine(GPoint from, GPoint to, DmsColor color, int width)
{
	GdiHandle<HPEN> pen(CreateDmsPen(color, width, DmsPenStyle::Solid));
	GdiObjectSelector<HPEN> sel(m_hDC, pen);
	::MoveToEx(m_hDC, from.x, from.y, NULL);
	::LineTo(m_hDC, to.x, to.y);
}

void GdiDrawContext::DrawPolyline(const GPoint* pts, int count, DmsColor color, int width, DmsPenStyle style)
{
	GdiHandle<HPEN> pen(CreateDmsPen(color, width, style));
	GdiObjectSelector<HPEN> sel(m_hDC, pen);
	::Polyline(m_hDC, &AsPOINT(*pts), count);
}

void GdiDrawContext::DrawPolygon(const GPoint* pts, int count, DmsColor fillColor, DmsHatchStyle hatch)
{
	GdiHandle<HPEN> invisiblePen(CreatePen(PS_NULL, 0, RGB(0, 0, 0)));
	GdiObjectSelector<HPEN> penSel(m_hDC, invisiblePen);

	GdiHandle<HBRUSH> brush(
		(hatch == DmsHatchStyle::Solid)
		? CreateSolidBrush(DmsColor2COLORREF(fillColor))
		: CreateHatchBrush(static_cast<int>(hatch), DmsColor2COLORREF(fillColor))
	);
	GdiObjectSelector<HBRUSH> brushSel(m_hDC, brush);
	::Polygon(m_hDC, &AsPOINT(*pts), count);
}

void GdiDrawContext::DrawEllipse(const GRect& boundingRect, DmsColor color)
{
	GdiHandle<HBRUSH> brush(CreateSolidBrush(DmsColor2COLORREF(color)));
	GdiObjectSelector<HBRUSH> brushSel(m_hDC, brush);
	GdiHandle<HPEN> pen(CreatePen(PS_NULL, 0, RGB(0, 0, 0)));
	GdiObjectSelector<HPEN> penSel(m_hDC, pen);
	::Ellipse(m_hDC, boundingRect.left, boundingRect.top, boundingRect.right, boundingRect.bottom);
}

void GdiDrawContext::TextOut(GPoint pos, CharPtr text, int len, DmsColor color)
{
	auto oldColor = ::SetTextColor(m_hDC, DmsColor2COLORREF(color));
	int wLen = MultiByteToWideChar(CP_UTF8, 0, text, len, nullptr, 0);
	if (wLen > 0)
	{
		std::vector<wchar_t> wBuf(wLen);
		MultiByteToWideChar(CP_UTF8, 0, text, len, wBuf.data(), wLen);
		::TextOutW(m_hDC, pos.x, pos.y, wBuf.data(), wLen);
	}
	::SetTextColor(m_hDC, oldColor);
}

void GdiDrawContext::TextOutW(GPoint pos, const wchar_t* text, int len, DmsColor color)
{
	auto oldColor = ::SetTextColor(m_hDC, DmsColor2COLORREF(color));
	::TextOutW(m_hDC, pos.x, pos.y, text, len);
	::SetTextColor(m_hDC, oldColor);
}

void GdiDrawContext::SetFont(CharPtr fontName, int pixelHeight, UInt16 angleDegTenths)
{
	HFONT hFont = CreateFontA(
		pixelHeight, 0, angleDegTenths, 0,
		FW_NORMAL, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
		PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
		fontName
	);
	if (hFont)
	{
		HFONT old = (HFONT)::SelectObject(m_hDC, hFont);
		if (m_OwnedFont)
			::DeleteObject(m_OwnedFont);
		m_OwnedFont = hFont;
	}
}

void GdiDrawContext::SetTextAlign(bool centerH, bool baseline)
{
	UINT flags = TA_NOUPDATECP;
	flags |= centerH ? TA_CENTER : TA_LEFT;
	flags |= baseline ? TA_BASELINE : TA_TOP;
	::SetTextAlign(m_hDC, flags);
}

void GdiDrawContext::DrawText(const GRect& rect, CharPtr text, int len, UInt32 format, DmsColor color)
{
	auto oldColor = ::SetTextColor(m_hDC, DmsColor2COLORREF(color));
	RECT r = AsRECT(rect);
	int wLen = MultiByteToWideChar(CP_UTF8, 0, text, len, nullptr, 0);
	if (wLen > 0)
	{
		std::vector<wchar_t> wBuf(wLen);
		MultiByteToWideChar(CP_UTF8, 0, text, len, wBuf.data(), wLen);
		::DrawTextW(m_hDC, wBuf.data(), wLen, &r, format);
	}
	::SetTextColor(m_hDC, oldColor);
}

GPoint GdiDrawContext::GetTextExtent(CharPtr text, int len)
{
	SIZE sz = {};
	int wLen = MultiByteToWideChar(CP_UTF8, 0, text, len, nullptr, 0);
	if (wLen > 0)
	{
		std::vector<wchar_t> wBuf(wLen);
		MultiByteToWideChar(CP_UTF8, 0, text, len, wBuf.data(), wLen);
		::GetTextExtentPoint32W(m_hDC, wBuf.data(), wLen, &sz);
	}
	return GPoint(sz.cx, sz.cy);
}

void GdiDrawContext::SetTextColor(DmsColor color)
{
	::SetTextColor(m_hDC, DmsColor2COLORREF(color));
}

void GdiDrawContext::SetBkColor(DmsColor color)
{
	::SetBkColor(m_hDC, DmsColor2COLORREF(color));
}

void GdiDrawContext::SetBkMode(bool transparent)
{
	::SetBkMode(m_hDC, transparent ? TRANSPARENT : OPAQUE);
}

GRect GdiDrawContext::GetClipRect() const
{
	GRect r;
	::GetClipBox(m_hDC, &AsRECT(r));
	return r;
}

void GdiDrawContext::SetClipRegion(const Region& rgn)
{
	auto hrgn = RegionToHRGN(rgn);
	::SelectClipRgn(m_hDC, hrgn);
}

void GdiDrawContext::SetClipRect(const GRect& rect)
{
	HRGN hrgn = ::CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);
	::SelectClipRgn(m_hDC, hrgn);
	::DeleteObject(hrgn);
}

void GdiDrawContext::ResetClip()
{
	::SelectClipRgn(m_hDC, NULL);
}

void GdiDrawContext::DrawImage(const GRect& destRect, const void* pixelData, int width, int height, int bitsPerPixel, const void* paletteRGBQuads, int paletteCount)
{
	int paletteBytes = paletteCount * sizeof(RGBQUAD);
	std::vector<Byte> bmiBuffer(sizeof(BITMAPINFOHEADER) + paletteBytes);
	BITMAPINFO* bmi = reinterpret_cast<BITMAPINFO*>(bmiBuffer.data());
	bmi->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth         = width;
	bmi->bmiHeader.biHeight        = height; // bottom-up
	bmi->bmiHeader.biPlanes        = 1;
	bmi->bmiHeader.biBitCount      = bitsPerPixel;
	bmi->bmiHeader.biCompression   = BI_RGB;
	bmi->bmiHeader.biSizeImage     = 0;
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;
	bmi->bmiHeader.biClrUsed       = paletteCount;
	bmi->bmiHeader.biClrImportant  = paletteCount;
	if (paletteCount > 0 && paletteRGBQuads)
		memcpy(bmi->bmiColors, paletteRGBQuads, paletteBytes);

	::StretchDIBits(m_hDC,
		destRect.left, destRect.top, destRect.Width(), destRect.Height(),
		0, 0, width, height,
		pixelData, bmi, DIB_RGB_COLORS, SRCCOPY);
}

#endif // _WIN32
