// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Point-in-polygon test (IsInside) over point iterators.
 */

#if !defined(__GEOM_ISINSIDE_H)
#define __GEOM_ISINSIDE_H

//**************************************************************************

// Does the edge from prevPoint to thisPoint, both relative to the point tested, cross the horizontal half-line from that point
// to the right? Only an edge with one end strictly above the point's row and the other not can, so only an edge whose y-range
// holds that row. IsInside applies this test to every edge of a polygon; PolygonEdgeIndex (geo/dll/src/geom) applies it to the
// edges listed for the point's row, so that the two give the same answer (#1320).
template <typename PointType>
bool IsCrossingRightOfOrigin(PointType prevPoint, PointType thisPoint)
{
	if ((0 < thisPoint.Row()) == (0 < prevPoint.Row())) // not crossing the x-axis
		return false;

	/*		Afleiding van conditie dat snijpunt (P,T) met x-as strict rechts van oorsprong ligt
			0 < TX - TY / RC[T->P]
			0 < TX - TY * (PX-TX) / (PY-TY)
			0 < [TX * (PY-TY)- TY * (PX-TX)] / (PY-TY) // bring TX into quotient
			0 < [TX*PY- TY*PX] / (PY-TY)               // cancel TX*TY
			0 < OutProduct(T,P) / (PY-TY)              // rewrite using OutProduct
			0 < OutProduct(T,P) * (PY-TY)              // rewrite assuming PY-TY != 0, thus (PY-TY)^2 > 0
	*/
	dms_assert(thisPoint.Row() != prevPoint.Row() );
	Float64 outProd = OutProduct<Float64>(thisPoint, prevPoint);
	if (prevPoint.Row() < thisPoint.Row())
		return 0 > outProd;
	return 0 < outProd;
}

template <typename ConstPointIter, typename PointType>
bool IsInside(ConstPointIter polyBegin, ConstPointIter polyEnd, PointType point)
{
	if (std::distance(polyBegin, polyEnd) <= 2)
		return false; // no interior anyway

	bool c = false;   // count #segments right of point

	PointType prevPoint = polyEnd[-1] - point; // use point as base for numerical stability

	while (polyBegin != polyEnd)
	{
		PointType thisPoint = *polyBegin - point; // use base again
		++polyBegin;
		if (IsCrossingRightOfOrigin(prevPoint, thisPoint))
			c = !c;
		prevPoint = thisPoint;
	}
	return c;
}

template <typename PointType>
bool IsInside(SA_ConstReference<PointType> poly, PointType point)
{
	return IsInside(poly.begin(), poly.end(), point);
}

#endif // __GEOM_ISINSIDE_H


