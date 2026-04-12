// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_DRAWCONTEXT_H)
#define __SHV_DRAWCONTEXT_H

#include "ShvBase.h"
#include "geo/color.h"

struct GRect;
struct Region;

//----------------------------------------------------------------------
// class  : DrawContext
//----------------------------------------------------------------------
// Abstract drawing context.
// GetHDC() provides backward compat for callers not yet migrated.
// Abstract drawing methods allow backend-agnostic rendering.

class DrawContext
{
public:
	virtual ~DrawContext() = default;
	virtual HDC GetHDC() const = 0;

	virtual void FillRect(const GRect& rect, DmsColor color) = 0;
	virtual void FillRegion(const Region& rgn, DmsColor color) = 0;
	virtual void InvertRect(const GRect& rect) = 0;
	virtual void DrawFocusRect(const GRect& rect) = 0;
};

//----------------------------------------------------------------------
// class  : GdiDrawContext
//----------------------------------------------------------------------
// Non-owning wrapper around an HDC. Lifetime of the HDC is managed
// by the caller (DcHandle, PaintDcHandle, etc.)

class GdiDrawContext : public DrawContext
{
public:
	GdiDrawContext() : m_hDC(NULL) {}
	explicit GdiDrawContext(HDC hdc) : m_hDC(hdc) {}

	HDC GetHDC() const override { return m_hDC; }
	void SetHDC(HDC hdc) { m_hDC = hdc; }

	void FillRect(const GRect& rect, DmsColor color) override;
	void FillRegion(const Region& rgn, DmsColor color) override;
	void InvertRect(const GRect& rect) override;
	void DrawFocusRect(const GRect& rect) override;

private:
	HDC m_hDC;
};

#endif // __SHV_DRAWCONTEXT_H
