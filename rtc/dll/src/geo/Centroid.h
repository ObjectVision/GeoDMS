// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_GEO_CENTROID_H
#define __RTC_GEO_CENTROID_H

//#include "geo/GeoSequence.h"

//----------------------------------------------------------------------
// Section      : Distance Measures
//----------------------------------------------------------------------

template <typename CalcType, typename PointType, typename ConstPointIter>
PointType Centroid(ConstPointIter polyBegin, ConstPointIter polyEnd)
{
	if (std::distance(polyBegin, polyEnd) <= 2)
		return UNDEFINED_VALUE(PointType);

	CalcType        cumulDoubleArea = 0;
	Point<CalcType> result(0, 0);
	
	// edges (s-2,s-1) and (s-1,0) include zero based s-1 and can be skipped
	--polyEnd;

	PointType polyBase  = *polyEnd;           // use this base for numerical stability
	PointType prevPoint = *polyBegin - polyBase; // usally zero

	while (++polyBegin != polyEnd)
	{
		PointType thisPoint = *polyBegin - polyBase;
		CalcType  a         = OutProduct<CalcType>(thisPoint, prevPoint);

		result          += Convert<Point<CalcType> >(prevPoint+thisPoint) * a;
		cumulDoubleArea += a;

		if (++polyBegin == polyEnd) break;
		prevPoint        = *polyBegin - polyBase;
		a                = OutProduct<CalcType>(prevPoint, thisPoint);

		result          += Convert<Point<CalcType> >(prevPoint+thisPoint) * a;
		cumulDoubleArea += a;
	}
	if (cumulDoubleArea != 0)
	{
		result /= (3 * cumulDoubleArea);
		return Convert<PointType>(result) + polyBase;
	}

	return UNDEFINED_VALUE(PointType);
}


template <typename CalcType, typename PointType>
inline PointType Centroid(SA_ConstReference<PointType> poly)
{
	return Centroid<CalcType, PointType>( begin_ptr(poly), end_ptr(poly) );
}



#endif // __RTC_GEO_CENTROID_H


