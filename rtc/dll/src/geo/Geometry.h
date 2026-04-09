// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_GEO_GEOMETRY_H)
#define __RTC_GEO_GEOMETRY_H

#include "RtcBase.h"
#include "geo/ConversionBase.h"
#include "geo/PointOrder.h"
#include "Range.h"

//----------------------------------------------------------------------
// define 4 PointTypes and related RectTypes
//----------------------------------------------------------------------

#define DEFINE_POINT(PointType, RectType, ScalarType) \
	typedef Point<ScalarType> PointType;  \
	typedef Range<PointType>  RectType;   \

DEFINE_POINT(SPoint, SRect, Int16 )
DEFINE_POINT(WPoint, WRect, UInt16)
DEFINE_POINT(IPoint, IRect, Int32 )
DEFINE_POINT(UPoint, URect, UInt32)
DEFINE_POINT(DPoint, DRect, Float64)
DEFINE_POINT(FPoint, FRect, Float32)
DEFINE_POINT(I64Point, I64Rect, Int64)
DEFINE_POINT(U64Point, U64Rect, UInt64)

#undef DEFINE_POINT

template <typename T> inline
I64Rect AsI64Rect(const Range<T>& src)
{
	return I64Rect(
		shp2dms_order(I64Point(ThrowingConvert<Int64>(src.first) , 0)), 
		shp2dms_order(I64Point(ThrowingConvert<Int64>(src.second), 1))
	);
}

template <typename T> inline
I64Rect AsI64Rect(const Range<Point<T>>& src)
{
	return I64Rect(
		ThrowingConvert<Point<Int64>>(src.first), 
		ThrowingConvert<Point<Int32>>(src.second)
	);
}


#endif // __RTC_GEO_GEOMETRY_H


