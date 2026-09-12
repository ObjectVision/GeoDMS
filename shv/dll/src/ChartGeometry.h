// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_CHARTGEOMETRY_H
#define __SHV_CHARTGEOMETRY_H

#include "ShvBase.h"
#include "geom/IsInside.h"

//----------------------------------------------------------------------
// chart-space hit tests shared by the chart layers (#75, #1273)
//----------------------------------------------------------------------
// A chart layer aggregates its entities into shapes drawn in chart-space world
// coordinates: the bars of a histogram, the slices of a pie. A rectangle or lasso
// selection then asks whether such a shape touches the selection shape, and the
// answers below are exact for polygons: a shape is hit when a vertex of either lies
// inside the other, or when two of their edges cross.

inline bool SegmentsCross(CrdPoint a, CrdPoint b, CrdPoint c, CrdPoint d)
{
	auto orient = [](CrdPoint p, CrdPoint q, CrdPoint r) -> int
	{
		CrdType v = (q.first - p.first) * (r.second - p.second)
		          - (q.second - p.second) * (r.first - p.first);
		return (v > 0) - (v < 0);
	};
	return orient(a, b, c) * orient(a, b, d) < 0
	    && orient(c, d, a) * orient(c, d, b) < 0;
}

inline bool PolygonIntersectsRect(const CrdPoint* first, const CrdPoint* last, const CrdRect& rect)
{
	// any polygon vertex inside the rect?
	for (const CrdPoint* i = first; i != last; ++i)
		if (IsIncluding(rect, *i))
			return true;

	// any rect corner inside the polygon?
	CrdPoint corners[4] = {
		rect.first,
		CrdPoint(rect.first.first,  rect.second.second),
		rect.second,
		CrdPoint(rect.second.first, rect.first.second)
	};
	for (auto corner : corners)
		if (IsInside(first, last, corner))
			return true;

	// any polygon edge crossing a rect edge?
	for (const CrdPoint* i = first; i != last; ++i)
	{
		const CrdPoint* j = i + 1; if (j == last) j = first;
		for (UInt32 c = 0; c != 4; ++c)
			if (SegmentsCross(*i, *j, corners[c], corners[(c+1) % 4]))
				return true;
	}
	return false;
}

inline bool PolygonsIntersect(const CrdPoint* aFirst, const CrdPoint* aLast, const CrdPoint* bFirst, const CrdPoint* bLast)
{
	if (aFirst == aLast || bFirst == bLast)
		return false;

	// a vertex of either polygon inside the other?
	for (const CrdPoint* i = aFirst; i != aLast; ++i)
		if (IsInside(bFirst, bLast, *i))
			return true;
	for (const CrdPoint* i = bFirst; i != bLast; ++i)
		if (IsInside(aFirst, aLast, *i))
			return true;

	// crossing edges?
	for (const CrdPoint* i = aFirst; i != aLast; ++i)
	{
		const CrdPoint* j = i + 1; if (j == aLast) j = aFirst;
		for (const CrdPoint* k = bFirst; k != bLast; ++k)
		{
			const CrdPoint* l = k + 1; if (l == bLast) l = bFirst;
			if (SegmentsCross(*i, *j, *k, *l))
				return true;
		}
	}
	return false;
}

// squared distance from p to the segment [a, b]
inline CrdType SqrDistToSegment(CrdPoint p, CrdPoint a, CrdPoint b)
{
	CrdType dx = b.first - a.first, dy = b.second - a.second;
	CrdType len2 = dx * dx + dy * dy;
	CrdType t = 0.0;
	if (len2 > 0)
	{
		t = ((p.first - a.first) * dx + (p.second - a.second) * dy) / len2;
		if (t < 0) t = 0; else if (t > 1) t = 1;
	}
	CrdType ex = a.first + t * dx - p.first, ey = a.second + t * dy - p.second;
	return ex * ex + ey * ey;
}

// does the disc (center, radius) touch the polygon?
inline bool PolygonIntersectsCircle(const CrdPoint* first, const CrdPoint* last, CrdPoint center, CrdType radius)
{
	if (first == last)
		return false;
	if (IsInside(first, last, center))
		return true;
	CrdType r2 = radius * radius;
	for (const CrdPoint* i = first; i != last; ++i)
	{
		const CrdPoint* j = i + 1; if (j == last) j = first;
		if (SqrDistToSegment(center, *i, *j) <= r2)
			return true;
	}
	return false;
}

#endif // __SHV_CHARTGEOMETRY_H
