// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_GEO_CENTROIDORMID_H
#define __RTC_GEO_CENTROIDORMID_H

#include "geo/Centroid.h"
#include "geo/SelectPoint.h"

template <typename ScalarType, typename PointType>
PointType CentroidOrMid(SA_ConstReference<PointType> poly, ScanPointCalcResource<ScalarType>& calcResource)
{
	PointType centroid = Centroid<Float64, PointType>(poly);
	if (IsInside(poly, centroid))
		return centroid;
	return Mid<ScalarType>(begin_ptr( poly ), end_ptr( poly ), calcResource );
}


#endif // __RTC_GEO_CENTROIDORMID_H


