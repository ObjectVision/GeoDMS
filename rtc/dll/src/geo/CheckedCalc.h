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
#include "mci/ValueWrap.h"

//----------------------------------------------------------------------

template <typename T> struct mul_type;
template <> struct mul_type<UInt8 > { typedef UInt16 type; };
template <> struct mul_type<UInt16> { typedef UInt32 type; };
template <> struct mul_type<UInt32> { typedef UInt64 type; };

template <> struct mul_type<Int8 > { typedef Int16 type; };
template <> struct mul_type<Int16> { typedef Int32 type; };
template <> struct mul_type<Int32> { typedef Int64 type; };

//----------------------------------------------------------------------

template <IntegralValue T>
const ValueClass* NextAddIntegral()
{
	constexpr auto nrBits = nrbits_of_v<T>;
	constexpr bool isSigned = is_signed_v<T>;
	if constexpr (nrBits < 8)
	{
		if constexpr (nrBits == 1)
			return ValueWrap<UInt2>::GetStaticClass();
		else if constexpr (nrBits == 2)
			return ValueWrap<UInt4>::GetStaticClass();
		else
		{
			static_assert(nrBits == 4);
			return ValueWrap<UInt8>::GetStaticClass();
		}
	}
	else
	{
		if constexpr (nrBits <= 16)
		{
			if constexpr (nrBits == 8)
				if constexpr (isSigned)
					return ValueWrap<Int16>::GetStaticClass();
				else
					return ValueWrap<UInt16>::GetStaticClass();
			else
			{
				static_assert(nrBits == 16);
				if constexpr (isSigned)
					return ValueWrap<Int32>::GetStaticClass();
				else
					return ValueWrap<UInt32>::GetStaticClass();
			}
		}
		else
		{
			if constexpr (nrBits == 32)
				if constexpr (isSigned)
					return ValueWrap<Int64>::GetStaticClass();
				else
					return ValueWrap<UInt64>::GetStaticClass();
			else
			{
				static_assert(nrBits == 64);
				return nullptr;
			}
		}
	}
}

template <IntegralValue T>
const ValueClass* NextSubIntegral()
{
	constexpr auto nrBits = nrbits_of_v<T>;
	constexpr bool isSigned = is_signed_v<T>;
	if constexpr (nrBits < 8)
	{
		if constexpr (nrBits == 1)
			return ValueWrap<UInt2>::GetStaticClass();
		else if constexpr (nrBits == 2)
			return ValueWrap<UInt4>::GetStaticClass();
		else
		{
			static_assert(nrBits == 4);
			return ValueWrap<UInt8>::GetStaticClass();
		}
	}
	else
	{
		if constexpr (nrBits <= 16)
		{
			if constexpr (nrBits == 8)
				if constexpr (isSigned)
					return ValueWrap<Int16>::GetStaticClass();
				else
					return ValueWrap<Int8>::GetStaticClass();
			else
			{
				static_assert(nrBits == 16);
				if constexpr (isSigned)
					return ValueWrap<Int32>::GetStaticClass();
				else
					return ValueWrap<Int16>::GetStaticClass();
			}
		}
		else
		{
			if constexpr (nrBits == 32)
				if constexpr (isSigned)
					return ValueWrap<Int64>::GetStaticClass();
				else
					return ValueWrap<Int32>::GetStaticClass();
			else
			{
				static_assert(nrBits == 64);
				if constexpr (isSigned)
					return nullptr;
				else
					return ValueWrap<Int64>::GetStaticClass();
			}
		}
	}
}

//----------------------------------------------------------------------

template <typename T> 
typename boost::disable_if<is_signed<T>, T >::type
CheckedAdd(T a, T b)
{
	assert(a>=0);
	assert(b>=0);
	T r = a+b;
	assert(r>=0);
	if (r < a || r < b)
		throwDmsErrD("Overflow in addition");
	return r;
}

template <typename T> T
CheckedMul(T a,T b, bool suggestAlternative)
{
	typename mul_type<T>::type r = a;
	r *= b;
	if (r != T(r))
		throwOverflow("multiplying", a, "and", b, suggestAlternative, "mul_or_null", NextAddIntegral<T>());
	return r;
}

template <>
inline UInt64 CheckedMul<UInt64>(UInt64 a, UInt64 b, bool suggestAlternative)
{
	UInt64 res = a * b; 
	if ((a && (res / a != b)) || (b && (res / b != a)))
		throwDmsErrD("Overflow in multiplication");
	return res;
}

template <>
inline Int64 CheckedMul<Int64>(Int64 a, Int64 b, bool suggestAlternative)
{
	Int64 res = a * b; 
	if ((a && (res / a != b)) || (b && (res / b != a)))
		throwDmsErrD("Overflow in multiplication");
	return res;
}

#endif // __RTC_GEO_CHECKEDCALC_H
