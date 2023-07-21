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

#ifndef __RTC_GEO_CHECKEDCALC_H
#define __RTC_GEO_CHECKEDCALC_H

#include <boost/utility/enable_if.hpp>

#include "geo/ElemTraits.h"

//----------------------------------------------------------------------

template <typename T> struct mul_type;
template <> struct mul_type<UInt8 > { typedef UInt16 type; };
template <> struct mul_type<UInt16> { typedef UInt32 type; };
template <> struct mul_type<UInt32> { typedef UInt64 type; };

template <> struct mul_type<Int8 > { typedef Int16 type; };
template <> struct mul_type<Int16> { typedef Int32 type; };
template <> struct mul_type<Int32> { typedef Int64 type; };

//----------------------------------------------------------------------

template <typename T> 
typename boost::disable_if<is_signed<T>, T >::type
CheckedAdd(T a, T b)
{
	dms_assert(a>=0);
	dms_assert(b>=0);
	T r = a+b;
	dms_assert(r>=0);
	if (r < a || r < b)
		throwDmsErrD("Overflow in addition");
	return r;
}

template <typename T> T
CheckedMul(T a,T b)
{
	typename mul_type<T>::type r = a;
	r *= b;
	if (r != T(r))
		throwDmsErrD("Overflow in multiplication");
	return r;
}

template <>
inline UInt64 CheckedMul<UInt64>(UInt64 a, UInt64 b)
{
	UInt64 res = a * b; 
	if ((a && (res / a != b)) || (b && (res / b != a)))
		throwDmsErrD("Overflow in multiplication");
	return res;
}

template <>
inline Int64 CheckedMul<Int64>(Int64 a, Int64 b)
{
	Int64 res = a * b; 
	if ((a && (res / a != b)) || (b && (res / b != a)))
		throwDmsErrD("Overflow in multiplication");
	return res;
}

inline UInt64 CheckedMul(UInt64 a, UInt64 b, UInt64*)
{
	return CheckedMul<UInt64>(a, b);
}

inline UInt64 CheckedMul(UInt32 a, UInt32 b, UInt64*)
{
	return UInt64(a) * UInt64(b);
}

inline UInt32 CheckedMul(UInt32 a, UInt32 b, UInt32*)
{
	return CheckedMul<UInt32>(a, b);
}

#endif // __RTC_GEO_CHECKEDCALC_H
