// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_DRAWCONTEXT_H)
#define __SHV_DRAWCONTEXT_H

#include "ShvBase.h"
#include "geo/color.h"
#include "GeoTypes.h"

struct GRect;
struct GPoint;
struct Region;

//----------------------------------------------------------------------
// PenStyle enum (portable replacement for PS_SOLID, PS_DASH, etc.)
//----------------------------------------------------------------------

enum class DmsPenStyle : Int16 {
	Solid = 0,
	Dash = 1,
	Dot = 2,
	DashDot = 3,
	DashDotDot = 4,
	Null = 5,      // invisible pen
};

//----------------------------------------------------------------------
// HatchStyle enum (portable replacement for HS_HORIZONTAL, etc.)
//----------------------------------------------------------------------

enum class DmsHatchStyle : Int32 {
	Solid = -1,          // solid fill (no hatch)
	Horizontal = 0,
	Vertical = 1,
	ForwardDiagonal = 2,
	BackwardDiagonal = 3,
	Cross = 4,
	DiagonalCross = 5,
};

//----------------------------------------------------------------------
// class  : DrawContext
//----------------------------------------------------------------------
// Abstract drawing context for all rendering operations.
// Implementations: QtDrawContext (QPainter), GdiDrawContext (Win32, transitional).

class DrawContext
{
public:
	virtual ~DrawContext() = default;

	// === Rectangles & Regions ===
	virtual void FillRect(const GRect& rect, DmsColor color) = 0;
	virtual void FillRegion(const Region& rgn, DmsColor color) = 0;
	virtual void InvertRect(const GRect& rect) = 0;
	virtual void DrawFocusRect(const GRect& rect) = 0;
	virtual void InvertRegion(const Region& rgn) = 0;

	// === Lines ===
	virtual void DrawLine(GPoint from, GPoint to, DmsColor color, int width = 1) = 0;
	virtual void DrawPolyline(const GPoint* pts, int count, DmsColor color, int width = 1, DmsPenStyle style = DmsPenStyle::Solid) = 0;

	// === Polygons ===
	virtual void DrawPolygon(const GPoint* pts, int count, DmsColor fillColor, DmsHatchStyle hatch = DmsHatchStyle::Solid) = 0;
	virtual void DrawEllipse(const GRect& boundingRect, DmsColor color) = 0;

	// === Text ===
	virtual void TextOut(GPoint pos, CharPtr text, int len, DmsColor color) = 0;
	virtual void TextOutW(GPoint pos, const wchar_t* text, int len, DmsColor color) = 0;
	virtual void DrawText(const GRect& rect, CharPtr text, int len, UInt32 format, DmsColor color) = 0;
	virtual GPoint GetTextExtent(CharPtr text, int len) = 0;
	virtual void SetTextColor(DmsColor color) = 0;
	virtual void SetBkColor(DmsColor color) = 0;
	virtual void SetBkMode(bool transparent) = 0;

	// === Font ===
	virtual void SetFont(CharPtr fontName, int pixelHeight, UInt16 angleDegTenths = 0) = 0;
	virtual void SetTextAlign(bool centerH, bool baseline) = 0;

	// === Clipping ===
	virtual GRect GetClipRect() const = 0;
	virtual void SetClipRegion(const Region& rgn) = 0;
	virtual void SetClipRect(const GRect& rect) = 0;
	virtual void ResetClip() = 0;

	// === 3D Borders ===
	virtual void DrawButtonBorder(GRect& rect) = 0;
	virtual void DrawReversedBorder(GRect& rect) = 0;
	void DrawBorder(GRect& rect, bool reversed) { if (reversed) DrawReversedBorder(rect); else DrawButtonBorder(rect); }

	// === Backward compat (transitional, will be removed) ===
#if defined(_WIN32)
	virtual HDC GetHDC() const = 0;
#endif
};

#if defined(_WIN32)
//----------------------------------------------------------------------
// class  : GdiDrawContext
//----------------------------------------------------------------------
// Non-owning wrapper around an HDC. Lifetime of the HDC is managed
// by the caller (DcHandle, PaintDcHandle, etc.)
// Transitional: will be removed when all rendering uses Qt.

class GdiDrawContext : public DrawContext
{
public:
	GdiDrawContext() : m_hDC(NULL), m_OwnedFont(NULL) {}
	explicit GdiDrawContext(HDC hdc) : m_hDC(hdc), m_OwnedFont(NULL) {}
	~GdiDrawContext() override;

	HDC GetHDC() const override { return m_hDC; }
	void SetHDC(HDC hdc) { m_hDC = hdc; }

	void FillRect(const GRect& rect, DmsColor color) override;
	void FillRegion(const Region& rgn, DmsColor color) override;
	void InvertRect(const GRect& rect) override;
	void DrawFocusRect(const GRect& rect) override;
	void InvertRegion(const Region& rgn) override;

	void DrawLine(GPoint from, GPoint to, DmsColor color, int width) override;
	void DrawPolyline(const GPoint* pts, int count, DmsColor color, int width, DmsPenStyle style) override;
	void DrawPolygon(const GPoint* pts, int count, DmsColor fillColor, DmsHatchStyle hatch) override;
	void DrawEllipse(const GRect& boundingRect, DmsColor color) override;

	void TextOut(GPoint pos, CharPtr text, int len, DmsColor color) override;
	void TextOutW(GPoint pos, const wchar_t* text, int len, DmsColor color) override;
	void DrawText(const GRect& rect, CharPtr text, int len, UInt32 format, DmsColor color) override;
	GPoint GetTextExtent(CharPtr text, int len) override;
	void SetTextColor(DmsColor color) override;
	void SetBkColor(DmsColor color) override;
	void SetBkMode(bool transparent) override;
	void SetFont(CharPtr fontName, int pixelHeight, UInt16 angleDegTenths) override;
	void SetTextAlign(bool centerH, bool baseline) override;

	GRect GetClipRect() const override;
	void SetClipRegion(const Region& rgn) override;
	void SetClipRect(const GRect& rect) override;
	void ResetClip() override;

	void DrawButtonBorder(GRect& rect) override;
	void DrawReversedBorder(GRect& rect) override;

private:
	HDC   m_hDC;
	HFONT m_OwnedFont;
};
#endif // _WIN32

#endif // __SHV_DRAWCONTEXT_H#endif // __SHV_DRAWCONTEXT_H
