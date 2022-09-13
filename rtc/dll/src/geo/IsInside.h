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

#ifndef __RTC_GEO_ISINSIDE_H
#define __RTC_GEO_ISINSIDE_H

//**************************************************************************


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
		if	( (0 <  thisPoint.Row() ) != (0 < prevPoint.Row() ) ) // is this line crossing x-axis ??
		{
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
			{
				if (0 > outProd)
					c = !c;
			}
			else
			{
				if (0 < outProd)
					c = !c;
			}
		}
		prevPoint = thisPoint;
	}
	return c;
}

template <typename PointType>
bool IsInside(SA_ConstReference<PointType> poly, PointType point)
{
	return IsInside(poly.begin(), poly.end(), point);
}

#endif // __RTC_GEO_ISINSIDE_H


