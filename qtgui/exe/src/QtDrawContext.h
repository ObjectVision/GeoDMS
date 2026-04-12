// Copyright (C) 1998-2024 Object Vision b.v. 
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
// Used when rendering through Qt instead of GDI.

class QtDrawContext : public DrawContext
{
public:
	QtDrawContext() : m_Painter(nullptr) {}
	explicit QtDrawContext(QPainter* painter) : m_Painter(painter) {}

	HDC GetHDC() const override { return NULL; } // Qt has no HDC; callers should use abstract methods instead

	void FillRect(const GRect& rect, DmsColor color) override;
	void FillRegion(const Region& rgn, DmsColor color) override;
	void InvertRect(const GRect& rect) override;
	void DrawFocusRect(const GRect& rect) override;

	QPainter* GetPainter() const { return m_Painter; }

private:
	QPainter* m_Painter;
};

#endif // __QT_DRAWCONTEXT_H
