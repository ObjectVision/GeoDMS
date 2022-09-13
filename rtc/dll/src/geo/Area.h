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


