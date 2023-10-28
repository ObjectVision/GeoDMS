//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#if !defined(__RTC_GEO_GEOMETRY_H)
#define __RTC_GEO_GEOMETRY_H

#include "RtcBase.h"
#include "geo/PointOrder.h"

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


