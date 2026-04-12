// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "DrawContext.h"

#include "DcHandle.h"
#include "GeoTypes.h"
#include "Region.h"
#include "GdiRegionUtil.h"

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
