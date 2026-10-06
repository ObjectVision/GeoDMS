// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#if defined(_WIN32)

#include "DrawContext.h"

#include "DcHandle.h"
#include "GeoTypes.h"
#include "Region.h"
#include "GdiRegionUtil.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

//----------------------------------------------------------------------
// GdiObjectCache
//----------------------------------------------------------------------
// SHV-A07, continuations B11. A GdiDrawContext lives for one draw callback (SHV_DrawInHDC, DataView::OnPaint,
// MovableObject's bitmap), in which a layer draws feature after feature with a few distinct pens, brushes
// and fonts. Since the DrawContext port (Steps 4a and 4c, ca9bb9da0 and 922fb95bc) every call created and
// deleted its own GDI objects, where the layers used to keep a PenArray and a FontArray: on 2026-10-06, on
// this kind of machine, 6.6 to 7.6 us per polygon with an outline against 4.0 to 5.2 us with objects made
// once, and 11.5 to 16 us per point symbol with a varying font against 3.6 to 4.5 us. So the context keeps
// what it creates, keyed by what it was created from.
// Every call selects back the pen and brush it replaced, so no cached pen or brush is selected between
// calls, and the caches can be emptied whenever they are full. They are bounded because a layer with a
// colour per feature would otherwise exhaust the 10,000 GDI objects that a process may hold. The font that
// SetFont or SetBold selected stays selected, as before; it is deselected before the fonts are deleted.

struct GdiObjectCache
{
	static constexpr std::size_t MAX_PENS = 1024, MAX_BRUSHES = 1024, MAX_FONTS = 64;

	std::unordered_map<UInt64, HPEN>   m_Pens;
	std::unordered_map<UInt64, HBRUSH> m_Brushes;
	std::vector<std::pair<LOGFONTW, HFONT>> m_Fonts;

	~GdiObjectCache()
	{
		DeletePens();
		DeleteBrushes();
		DeleteFonts();
	}
	void DeletePens()
	{
		for (auto& penEntry : m_Pens)
			::DeleteObject(penEntry.second);
		m_Pens.clear();
	}
	void DeleteBrushes()
	{
		for (auto& brushEntry : m_Brushes)
			::DeleteObject(brushEntry.second);
		m_Brushes.clear();
	}
	void DeleteFonts()
	{
		for (auto& fontEntry : m_Fonts)
			::DeleteObject(fontEntry.second);
		m_Fonts.clear();
	}
	bool HasFont(HFONT hFont) const
	{
		return std::any_of(m_Fonts.begin(), m_Fonts.end(), [hFont](const auto& fontEntry) { return fontEntry.second == hFont; });
	}
};

GdiDrawContext::GdiDrawContext()
	: m_hDC(NULL)
{}

GdiDrawContext::GdiDrawContext(HDC hdc)
	: m_hDC(hdc)
{}

GdiDrawContext::~GdiDrawContext()
{
	if (!m_Cache)
		return;
	// The DC outlives this context (DataView::OnPaint keeps its PaintDcHandle), and GDI refuses to delete
	// a font that is still selected into a DC, so the font that the first SetFont replaced goes back first.
	if (m_OrgFont && !m_Cache->m_Fonts.empty())
		::SelectObject(m_hDC, m_OrgFont);
	m_Cache.reset();
}

GdiObjectCache& GdiDrawContext::Cache()
{
	if (!m_Cache)
		m_Cache = std::make_unique<GdiObjectCache>();
	return *m_Cache;
}

void GdiDrawContext::FillRect(const GRect& rect, DmsColor color)
{
	UInt8 trans = GetTrans(color);
	if (trans == 0xFF)
		return; // fully transparent: nothing to draw

	DmsColor opaqueColor = color & 0x00FFFFFF; // CreateSolidBrush requires high byte == 0

	if (trans == 0)
	{
		::FillRect(m_hDC, &AsRECT(rect), GetBrush(opaqueColor, DmsHatchStyle::Solid));
		return;
	}

	// Semi-transparent fill via AlphaBlend with a 1x1 source bitmap.
	CompatibleDcHandle memDC(m_hDC, nullptr);
	GdiHandle<HBITMAP> bmp(::CreateCompatibleBitmap(m_hDC, 1, 1));
	GdiObjectSelector<HBITMAP> selectBitmap(memDC, bmp);

	RECT oneByOne = { 0, 0, 1, 1 };
	::FillRect(memDC, &oneByOne, GetBrush(opaqueColor, DmsHatchStyle::Solid));

	BLENDFUNCTION blendFunction = {};
	blendFunction.BlendOp             = AC_SRC_OVER;
	blendFunction.BlendFlags          = 0;
	blendFunction.SourceConstantAlpha = 255 - trans;
	blendFunction.AlphaFormat         = 0; // not AC_SRC_ALPHA

	::AlphaBlend(
		m_hDC,
		rect.left, rect.top,
		rect.Width(), rect.Height(),
		memDC, 0, 0, 1, 1,
		blendFunction
	);
}

void GdiDrawContext::FillRegion(const Region& rgn, DmsColor color)
{
	auto hrgn = RegionToHRGN(rgn);
	::FillRgn(m_hDC, hrgn, GetBrush(color, DmsHatchStyle::Solid));
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
	if (width <= 1 || gdiStyle == PS_SOLID || gdiStyle == PS_NULL)
		return ::CreatePen(gdiStyle, width, DmsColor2COLORREF(color));

	// CreatePen draws a dashed or dotted style of more than one pixel wide as a solid line (SHV-A07); a
	// geometric pen keeps the pattern, with the round ends and joins that PenArray gave such pens.
	LOGBRUSH logBrush = { BS_SOLID, DmsColor2COLORREF(color), 0 };
	return ::ExtCreatePen(PS_GEOMETRIC | gdiStyle | PS_ENDCAP_ROUND | PS_JOIN_ROUND, width, &logBrush, 0, nullptr);
}

HPEN GdiDrawContext::GetPen(DmsColor color, int width, DmsPenStyle style)
{
	if (style == DmsPenStyle::Null)
		return static_cast<HPEN>(::GetStockObject(NULL_PEN)); // a stock object, not created, never deleted

	width = std::clamp(width, 0, 0xFFFFFF);
	UInt64 key = (UInt64(color) << 32) | (UInt64(width) << 8) | UInt8(style);
	auto& cache = Cache();
	if (auto pos = cache.m_Pens.find(key); pos != cache.m_Pens.end())
		return pos->second;

	if (cache.m_Pens.size() >= GdiObjectCache::MAX_PENS)
		cache.DeletePens(); // none of them is selected: each call selects back the pen it replaced
	HPEN pen = CreateDmsPen(color, width, style);
	if (!pen)
		throwLastSystemError("GdiDrawContext: creating a pen of width {} failed; the process may have run out of GDI objects", width);
	cache.m_Pens.emplace(key, pen);
	return pen;
}

HBRUSH GdiDrawContext::GetBrush(DmsColor color, DmsHatchStyle hatch)
{
	UInt64 key = (UInt64(color) << 8) | UInt8(static_cast<Int32>(hatch) + 1);
	auto& cache = Cache();
	if (auto pos = cache.m_Brushes.find(key); pos != cache.m_Brushes.end())
		return pos->second;

	if (cache.m_Brushes.size() >= GdiObjectCache::MAX_BRUSHES)
		cache.DeleteBrushes(); // none of them is selected: each call selects back the brush it replaced
	HBRUSH brush = (hatch == DmsHatchStyle::Solid)
		? ::CreateSolidBrush(DmsColor2COLORREF(color))
		: ::CreateHatchBrush(static_cast<int>(hatch), DmsColor2COLORREF(color));
	if (!brush)
		throwLastSystemError("GdiDrawContext: creating a brush failed; the process may have run out of GDI objects");
	cache.m_Brushes.emplace(key, brush);
	return brush;
}

void GdiDrawContext::DrawLine(GPoint from, GPoint to, DmsColor color, int width)
{
	GdiObjectSelector<HPEN> sel(m_hDC, GetPen(color, width, DmsPenStyle::Solid));
	::MoveToEx(m_hDC, from.x, from.y, NULL);
	::LineTo(m_hDC, to.x, to.y);
}

void GdiDrawContext::DrawPolyline(const GPoint* pts, int count, DmsColor color, int width, DmsPenStyle style)
{
	GdiObjectSelector<HPEN> sel(m_hDC, GetPen(color, width, style));
	::Polyline(m_hDC, &AsPOINT(*pts), count);
}

void GdiDrawContext::DrawPolygon(const GPoint* pts, int count, DmsColor fillColor, DmsHatchStyle hatch)
{
	GdiObjectSelector<HPEN>   penSel  (m_hDC, GetPen(0, 0, DmsPenStyle::Null));
	GdiObjectSelector<HBRUSH> brushSel(m_hDC, GetBrush(fillColor, hatch));
	::Polygon(m_hDC, &AsPOINT(*pts), count);
}

void GdiDrawContext::DrawEllipse(const GRect& boundingRect, DmsColor color)
{
	GdiObjectSelector<HBRUSH> brushSel(m_hDC, GetBrush(color, DmsHatchStyle::Solid));
	GdiObjectSelector<HPEN>   penSel  (m_hDC, GetPen(0, 0, DmsPenStyle::Null));
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

static bool SameLogFont(const LOGFONTW& a, const LOGFONTW& b)
{
	return a.lfHeight == b.lfHeight && a.lfWidth == b.lfWidth
		&& a.lfEscapement == b.lfEscapement && a.lfOrientation == b.lfOrientation
		&& a.lfWeight == b.lfWeight && a.lfItalic == b.lfItalic && a.lfUnderline == b.lfUnderline && a.lfStrikeOut == b.lfStrikeOut
		&& a.lfCharSet == b.lfCharSet && a.lfOutPrecision == b.lfOutPrecision && a.lfClipPrecision == b.lfClipPrecision
		&& a.lfQuality == b.lfQuality && a.lfPitchAndFamily == b.lfPitchAndFamily
		&& wcsncmp(a.lfFaceName, b.lfFaceName, LF_FACESIZE) == 0; // GetObjectW may leave bytes after the terminator
}

void GdiDrawContext::SelectFont(const LOGFONTW& logFont)
{
	auto& cache = Cache();
	HFONT hFont = NULL;
	for (const auto& fontEntry : cache.m_Fonts)
		if (SameLogFont(fontEntry.first, logFont))
		{
			hFont = fontEntry.second;
			break;
		}

	HFONT current = static_cast<HFONT>(::GetCurrentObject(m_hDC, OBJ_FONT));
	if (hFont && hFont == current)
		return; // consecutive symbols and labels mostly share their font

	if (!hFont)
	{
		if (cache.m_Fonts.size() >= GdiObjectCache::MAX_FONTS)
		{
			if (cache.HasFont(current))
				::SelectObject(m_hDC, m_OrgFont); // the selected font is one of those about to be deleted
			cache.DeleteFonts();
		}
		hFont = ::CreateFontIndirectW(&logFont);
		if (!hFont)
			return; // as before: a font that cannot be made leaves the current one
		cache.m_Fonts.emplace_back(logFont, hFont);
	}
	HFONT old = static_cast<HFONT>(::SelectObject(m_hDC, hFont));
	if (!m_OrgFont && !cache.HasFont(old))
		m_OrgFont = old; // the DC's own font, restored in the destructor
}

void GdiDrawContext::SetFont(CharPtr fontName, int pixelHeight, UInt16 angleDegTenths)
{
	// Font name carries UTF-8 (DMS strings are UTF-8); convert to wide and use the
	// explicit -W font API, like the text path (TextOutW/DrawTextW) already does.
	// The LOGFONTW is what CreateFontW made of these arguments.
	LOGFONTW logFont = {};
	logFont.lfHeight         = pixelHeight;
	logFont.lfEscapement     = angleDegTenths;
	logFont.lfWeight         = FW_NORMAL;
	logFont.lfCharSet        = ANSI_CHARSET;
	logFont.lfOutPrecision   = OUT_TT_PRECIS;
	logFont.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
	logFont.lfQuality        = PROOF_QUALITY;
	logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	MultiByteToWideChar(CP_UTF8, 0, fontName, -1, logFont.lfFaceName, LF_FACESIZE);
	logFont.lfFaceName[LF_FACESIZE - 1] = 0;
	SelectFont(logFont);
}

void GdiDrawContext::SetBold(bool isBold)
{
	HFONT current = (HFONT)::GetCurrentObject(m_hDC, OBJ_FONT);
	if (!current)
		return;
	LOGFONTW lf{};
	if (!::GetObjectW(current, sizeof(lf), &lf))
		return;
	int desired = isBold ? FW_BOLD : FW_NORMAL;
	if (lf.lfWeight == desired)
		return;
	lf.lfWeight = desired;
	SelectFont(lf);
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

void GdiDrawContext::SetXorMode(bool on)
{
	::SetROP2(m_hDC, on ? R2_NOTXORPEN : R2_COPYPEN);
}

void GdiDrawContext::FrameRegion(const Region& rgn, DmsColor color, int xThickness, int yThickness)
{
	GdiHandle<HRGN> hrgn(RegionToHRGN(rgn));
	::FrameRgn(m_hDC, hrgn, GetBrush(color, DmsHatchStyle::Solid), xThickness, yThickness);
}

void GdiDrawContext::FillRegion(const Region& rgn, DmsColor color, DmsHatchStyle hatch)
{
	GdiHandle<HRGN> hrgn(RegionToHRGN(rgn));
	::FillRgn(m_hDC, hrgn, GetBrush(color, hatch));
}

void GdiDrawContext::DrawImage(const GRect& destRect, const void* pixelData, int width, int height, int bitsPerPixel, const void* paletteRGBQuads, int paletteCount, DmsRasterOp op)
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

	DWORD rop = (op == DmsRasterOp::SrcAnd) ? SRCAND : SRCCOPY;
	::StretchDIBits(m_hDC,
		destRect.left, destRect.top, destRect.Width(), destRect.Height(),
		0, 0, width, height,
		pixelData, bmi, DIB_RGB_COLORS, rop);
}

#endif // _WIN32
