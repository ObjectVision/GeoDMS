// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_GEO_DYNAMICPOINT_H
#define __RTC_GEO_DYNAMICPOINT_H

#include "geo/Point.h"

//----------------------------------------------------------------------
// Section      : Distance Measures
//----------------------------------------------------------------------

template <typename ReturnType, typename ConstPointPtr>
ReturnType ArcLength(ConstPointPtr arcBegin, ConstPointPtr arcEnd)
{
	ReturnType length = 0;

restart:
	if (arcBegin != arcEnd)
	{
		if (!IsDefined(*arcBegin))
		{
			++arcBegin;
			goto restart;
		}
		for (ConstPointPtr j = arcBegin, i = j; ++j != arcEnd; i = j)
		{
			assert(IsDefined(*i));
			if (!IsDefined(*j))
			{
				do {
					i = ++j; // past the undefined point
					if (i == arcEnd)
						goto exit;
				} while (!IsDefined(*i));
			}
			else
				length += std::sqrt(SqrDist<ReturnType>(*i, *j));
		}
	}
exit:
	return length;
}

template <typename ReturnType, typename PointType>
ReturnType ArcLength(SA_ConstReference<PointType> arc)
{
	return ArcLength<ReturnType>(arc.begin(), arc.end());
}

template <typename PointType, typename ConstPointPtr>
PointType DynamicPoint(ConstPointPtr arcBegin, ConstPointPtr arcEnd, Float64 ratio)
{
	dms_assert(ratio >= 0.0 && ratio <= 1.0);
	dms_assert(arcBegin != arcEnd);

	Float64 
		length = ArcLength<Float64>(arcBegin, arcEnd) * ratio,
		sofar  = 0;
	if (length == 0.0)
		return *arcBegin;

	for(ConstPointPtr j = arcBegin; ++j != arcEnd; arcBegin = j)
	{
		Float64 delta = sqrt(SqrDist<Float64>(*arcBegin, *j));
		sofar += delta;
		if (sofar >= length)
			return *j + (*arcBegin - *j) * typesafe_cast<typename scalar_of<PointType>::type>( (sofar - length) / delta );
	}
	return *arcBegin;
}

template <typename PointType>
inline PointType DynamicPoint(SA_ConstReference<PointType> arc, Float64 ratio)
{
	return DynamicPoint<PointType>(arc.begin(), arc.end(), ratio);
}

#endif // __RTC_GEO_DYNAMICPOINT_H


