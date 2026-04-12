// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__QT_DRAWCONTEXT_H)
#define __QT_DRAWCONTEXT_H

#include "DrawContext.h"

class QPainter;

//----------------------------------------------------------------------
// class  : QtDrawContext
//----------------------------------------------------------------------
// DrawContext implementation backed by QPainter.

class QtDrawContext : public DrawContext
{
public:
	QtDrawContext() : m_Painter(nullptr) {}
	explicit QtDrawContext(QPainter* painter) : m_Painter(painter) {}

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
	void DrawText(const GRect& rect, CharPtr text, int len, UInt32 format, DmsColor color) override;
	GPoint GetTextExtent(CharPtr text, int len) override;
	void SetTextColor(DmsColor color) override;
	void SetBkColor(DmsColor color) override;
	void SetBkMode(bool transparent) override;

	GRect GetClipRect() const override;
	void SetClipRegion(const Region& rgn) override;
	void SetClipRect(const GRect& rect) override;
	void ResetClip() override;

	void DrawButtonBorder(GRect& rect) override;
	void DrawReversedBorder(GRect& rect) override;

#if defined(_WIN32)
	HDC GetHDC() const override { return NULL; }
#endif

	QPainter* GetPainter() const { return m_Painter; }

private:
	QPainter* m_Painter;
};

#endif // __QT_DRAWCONTEXT_H
