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

#ifndef __RTC_GEO_CALCWIDTH_H
#define __RTC_GEO_CALCWIDTH_H

#if defined(MG_DEBUG)
bool MG_TRACE_LABELPOS = false;
#endif

//**************************************************************************

template <typename ScalarType>
bool ComesAfterZero(const Point<ScalarType>& p)
{
	return 0 <  p.Row()
		|| 0 == p.Row()
		&& 0 <  p.Col();
}

template <typename ScalarType, typename ConstPointIter>
ScalarType CalcWidth(ConstPointIter polyBegin, ConstPointIter polyEnd, Point<ScalarType> basePoint)
{
	DBG_START("CalcWidth", "", MG_TRACE_LABELPOS);

	dms_assert(std::distance(polyBegin, polyEnd) > 2);
	typedef Point<ScalarType> PointType;

	PointType prevPoint   = Convert<PointType>(polyEnd[-1]) - basePoint;
	bool      prevPositive= ComesAfterZero(prevPoint);

	std::vector<ScalarType> cols; cols.reserve(4);
	while(polyBegin != polyEnd)
	{
		PointType thisPoint    = Convert<PointType>(*polyBegin) - basePoint; // use base again
		bool      thisPositive = ComesAfterZero(thisPoint);

		++polyBegin;
		if	(thisPositive != prevPositive)
		{
			DBG_TRACE(("thisPoint %s, %d", AsString(thisPoint).c_str(), thisPositive));
			DBG_TRACE(("prevPoint %s, %d", AsString(prevPoint).c_str(), prevPositive));

			/*		Afleiding van conditie dat snijpunt (P,T) met x-as strict rechts van oorsprong ligt
					0 < TX - TY / RC[T->P]
					0 < TX - TY * (PX-TX) / (PY-TY)
					0 < [TX * (PY-TY)- TY * (PX-TX)] / (PY-TY) // bring TX into quotient
					0 < [TX*PY- TY*PX] / (PY-TY)               // cancel TX*TY
					0 < OutProduct(T,P) / (PY-TY)              // rewrite using OutProduct
					0 < OutProduct(T,P) * (PY-TY)              // rewrite assuming PY-TY != 0, thus (PY-TY)^2 > 0
			*/
			if	(thisPoint.Row() != prevPoint.Row())
			{
				ScalarType 
					col =
						(	prevPoint.Col() * thisPoint.Row() 
						-	thisPoint.Col() * prevPoint.Row()
						) / (thisPoint.Row() - prevPoint.Row());
				cols.push_back(col);
			}
			else
				cols.push_back(0);

			DBG_TRACE(("back %s", AsString(cols.back()).c_str()));

		}
		prevPoint    = thisPoint;
		prevPositive = thisPositive;
	}
	dms_assert(cols.size() % 2 == 0);
	if (cols.size() %2 != 0)
		return 0;

	ScalarType result = 0;
	auto
		i = cols.begin(),
		e = cols.end();
	std::sort(i, e);
	while( i != e )
	{
		result += (i[1] - i[0]);
		i += 2;
	}

	return result;
}

template<typename ScalarType> using ScanPointCalcResource = std::vector<ScalarType>;

template <typename ScalarType, typename ConstPointIter>
ScalarType GetScanPoint(ConstPointIter polyBegin, ConstPointIter polyEnd, ScalarType row, ScalarType xMeasure, ScanPointCalcResource<ScalarType>& cols)
{
	dms_assert(std::distance(polyBegin, polyEnd) > 2);
	typedef Point<ScalarType>                                ResPointType;
	typedef std::iterator_traits<ConstPointIter>::value_type InpPointType;


	ResPointType basePoint = shp2dms_order<ScalarType>(polyBegin->Col(), row);              // use *polyBegin as base for numerical stability
	ResPointType prevPoint = Convert<ResPointType>(polyEnd[-1]) - basePoint; // probably shp2dmsorder(0, polybegin->Row() - row)

//	row -= point.Row();

	cols.clear(); cols.reserve(4);
	ConstPointIter polyCurr = polyBegin;
	while (polyCurr != polyEnd)
	{
		InpPointType thisPoint = Convert<ResPointType>(*polyCurr) - basePoint; // use base again
		++polyCurr;
		if	( (0 <  thisPoint.Row() ) != (0 < prevPoint.Row() ) )
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
			ScalarType 
				col =
					(	prevPoint.Col() * thisPoint.Row()
					-	thisPoint.Col() * prevPoint.Row()
					) / (thisPoint.Row() - prevPoint.Row());
			cols.push_back(col);			
		}
		prevPoint = thisPoint;
	}
	if (!cols.size() || cols.size() %2 != 0)
		return UNDEFINED_VALUE(ScalarType);

	ScalarType width = 0;
	auto
		b = cols.begin(),
		e = cols.end();
	std::sort(b, e);
	auto i = b;
	while (i != e)
	{
		dms_assert(i[1] >= i[0]);

		width += i[1] - i[0];
		i += 2;
	}
	width *= xMeasure;
	i = b;
	ScalarType currWidth = 0;
	while(i != e )
	{
		dms_assert(i[1] >= i[0]);

		currWidth += i[1] - i[0];
		if (currWidth >= width)
			return basePoint.Col() + (i[1] + (width - currWidth));
		i += 2;
	}
	return UNDEFINED_VALUE(ScalarType);
}


#endif // __RTC_GEO_CALCWIDTH_H


