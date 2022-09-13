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
	if (arcBegin != arcEnd)
		for(ConstPointPtr j = arcBegin, i = j; ++j != arcEnd; i = j)
			length += sqrt(SqrDist<ReturnType>(*i, *j));

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


