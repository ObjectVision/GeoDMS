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


