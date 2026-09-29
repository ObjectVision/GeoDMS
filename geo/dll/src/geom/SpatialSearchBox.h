// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The query box for a SpatialIndex search that iterates one geometry against an
 *  index of the other: shared by the reversed connect (#1228) and the matrix
 *  operators (#1236).
 */

#if !defined(__GEO_SPATIALSEARCHBOX_H)
#define __GEO_SPATIALSEARCHBOX_H

#include "geom/GeoDist.h"
#include "geom/Range.h"
#include "vt/MinMax.h"

#include <cmath>

// The search box for a query against a spatial index: `box` grown by `dist`. The
// growing is done in Float64 and clamped to the coordinate type BEFORE the cast
// back, so an unbounded distance cannot overflow an integer coordinate type.
//
// For integer coordinates one extra unit is added on either side because the index
// tests a POINT leaf half-open -- IsIntersecting(Range, Point) admits the lower bound
// but not the upper one, so a point exactly on the upper edge would never be seen.
// Float coordinates get one representable step instead (GEO-A41). The box only
// preselects candidates; each of them is still measured.
template <typename T, typename R>
Range<Point<T>> InflatedSearchBox(const Range<Point<T>>& box, R dist)
{
	auto d = Float64(dist);
	if constexpr (std::is_integral_v<T>)
	{
		auto lo = [](Float64 v) { return Max<Float64>(Float64(MinValue<T>()), v - 1.0); };
		auto hi = [](Float64 v) { return Min<Float64>(Float64(MaxValue<T>()), v + 1.0); };

		return Range<Point<T>>
		(	Point<T>(T(lo(std::floor(Float64(box.first .first ) - d))), T(lo(std::floor(Float64(box.first .second) - d))))
		,	Point<T>(T(hi(std::ceil (Float64(box.second.first ) + d))), T(hi(std::ceil (Float64(box.second.second) + d))))
		);
	}
	else
	{
		// Float coordinates need neither rounding nor a whole unit: one representable step outward on either side
		// covers the half-open test of a point leaf and the rounding of the Float64 bound into T. A whole unit, as
		// for integers, made a query box of lat/lon data at least two degrees wide, so that each query measured
		// most of the points of a country (GEO-A41).
		auto lo = [](Float64 v) { return std::nextafter(T(Max<Float64>(Float64(MinValue<T>()), v)), MinValue<T>()); };
		auto hi = [](Float64 v) { return std::nextafter(T(Min<Float64>(Float64(MaxValue<T>()), v)), MaxValue<T>()); };

		return Range<Point<T>>
		(	Point<T>(lo(Float64(box.first .first ) - d), lo(Float64(box.first .second) - d))
		,	Point<T>(hi(Float64(box.second.first ) + d), hi(Float64(box.second.second) + d))
		);
	}
}

// sqrt of a squared distance, rounded generously (SafeBet) so that the box built
// from it cannot fall just short of a point at exactly that distance.
template <typename R>
R SqrtBet(R sqrDist)
{
	if (!(sqrDist > 0))
		return R(0);
	return SafeBet(sqrt(sqrDist));
}

#endif // __GEO_SPATIALSEARCHBOX_H
