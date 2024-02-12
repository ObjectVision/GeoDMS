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

#ifndef __RTC_SER_POINTSTREAM_H
#define __RTC_SER_POINTSTREAM_H

#include "ser/FormattedStream.h"
#include "geo/Point.h"
#include "geo/PointOrder.h"

//----------------------------------------------------------------------
// Section      : Serialization support for Points
//----------------------------------------------------------------------

template <class T> inline
BinaryOutStream& operator <<(BinaryOutStream& os, const Point<T>& r)
{
	//	os << r.first << r.second;
	os.Buffer().WriteBytes((const char*)&r, sizeof(Point<T>));
	return os;
}

template <class T> inline
BinaryInpStream& operator >>(BinaryInpStream& is, Point<T>& r)
{
	//	is >> r.first >> r.second;
	is.Buffer().ReadBytes((char*)&r, sizeof(Point<T>));
	return is;
}

template <typename T> inline
FormattedOutStream& operator << (FormattedOutStream& os, const Point<T>& p)
{
	os
		<<	"{" 
		<<	dmsPoint_GetFirstCfgValue(p)
		<<	(HasThousandSeparator(os.GetFormattingFlags()) ? "; " : ", ")
		<<	dmsPoint_GetSecondCfgValue(p)
		<<	"}"
	;
	return os;
}

template <typename T> inline
FormattedInpStream& operator >> (FormattedInpStream& is, Point<T>& p)
{
	is >> "{" >> p.first >> ", " >> p.second >> "}";

	reportF(SeverityTypeID::ST_Warning, "depreciated syntax for point data used.\n"
		"Use the %s operation to unambiguously create points.\n"
		, g_cfgColFirst ? "point_xy" : "point_yx"
	);

	cfg2dms_order_inplace(p);
	return is;
}

#endif // __RTC_SER_POINTSTREAM_H
