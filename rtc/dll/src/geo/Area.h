// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __RTC_GEO_AREA_H
#define __RTC_GEO_AREA_H

//#include "geo/GeoSequence.h"

//----------------------------------------------------------------------
// Section      : Distance Measures
//----------------------------------------------------------------------

template <typename ReturnType, typename ConstPointIter>
ReturnType imp_Area(ConstPointIter polyBegin, ConstPointIter polyEnd)
{
	typedef typename std::iterator_traits<ConstPointIter>::value_type PointType;
	if (std::distance(polyBegin, polyEnd) <= 2)
		return 0; // no interior anyway

	ReturnType cumulDoubleArea = 0;

	// edges (s-2,s-1) and (s-1,0) include zero based s-1 and can be skipped
	--polyEnd;

	PointType polyBase  = *polyEnd; // use this base for numerical stability
	PointType prevPoint = *polyBegin - polyBase; 

	while (++polyBegin != polyEnd)
	{
		PointType thisPoint = *polyBegin - polyBase;
		cumulDoubleArea += OutProduct<ReturnType>(thisPoint, prevPoint);

		if (++polyBegin == polyEnd) break;
		prevPoint = *polyBegin - polyBase;
		cumulDoubleArea += OutProduct<ReturnType>(prevPoint, thisPoint);
	}

	return cumulDoubleArea / 2;
}

template <typename ReturnType, typename ConstPointIter>
ReturnType Area(ConstPointIter polyBegin, ConstPointIter polyEnd)
{
	return imp_Area<ReturnType>(polyBegin, polyEnd);
}

template <typename ReturnType, typename PointType>
inline ReturnType Area(SA_ConstReference<PointType> poly)
{
	return imp_Area<ReturnType>(poly.begin(), poly.end());
}



#endif // __RTC_GEO_AREA_H


