// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_SER_POINTSTREAM_H
#define __RTC_SER_POINTSTREAM_H

#include "ser/FormattedStream.h"
#include "geo/Point.h"
#include "geo/PointOrder.h"

//----------------------------------------------------------------------
// Section      : Serialization support for Points
//----------------------------------------------------------------------

template <class T> inline
BinaryOutStream& operator <<(BinaryOutStream& os, const Point<T>& p)
{
	os << p.Row() << p.Col();
	return os;
}

template <class T> inline
BinaryInpStream& operator >>(BinaryInpStream& is, Point<T>& p)
{
	is >> p.Row() >> p.Col();
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
	T first, second;
	is >> "{" >> first >> ", " >> second >> "}";

	dmsPoint_SetFirstCfgValue(p, first);
	dmsPoint_SetSecondCfgValue(p, second);

	reportF(SeverityTypeID::ST_Warning, "depreciated syntax for point data used.\n"
		"Use the %s operation to unambiguously create points.\n"
		, g_cfgColFirst ? "point_xy" : "point_yx"
	);

	cfg2dms_order_inplace(p);
	return is;
}

#endif // __RTC_SER_POINTSTREAM_H
