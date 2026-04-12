// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_DRAWCONTEXT_H)
#define __SHV_DRAWCONTEXT_H

#include "ShvBase.h"

//----------------------------------------------------------------------
// class  : DrawContext
//----------------------------------------------------------------------
// Abstract drawing context. Step 2a: wraps HDC for backward compat.
// Step 2b will add abstract drawing methods (FillRect, TextOut, etc.)
// Step 2c will add QtDrawContext backed by QPainter.

class DrawContext
{
public:
	virtual ~DrawContext() = default;
	virtual HDC GetHDC() const = 0;
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

private:
	HDC m_hDC;
};

#endif // __SHV_DRAWCONTEXT_H
