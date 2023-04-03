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

#if !defined(__RTC_GEO_COUPLE_H)
#define __RTC_GEO_COUPLE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "dbg/Check.h"
#include "geo/ElemTraits.h"
#include "geo/Undefined.h"
#include "utl/swap.h"

//----------------------------------------------------------------------
// class  : Couple<T>
//----------------------------------------------------------------------

template <class T>
struct Couple
{
	typedef T value_type;
	typedef typename param_type<T>::type value_cref;

//	Constructors (specified)
	Couple() {} // default initialisastion results in valid possibly non-zero objects too
	Couple(value_cref a, value_cref b): first(a), second(b) {}
	Couple(Undefined): first(UNDEFINED_VALUE(T)), second(UNDEFINED_VALUE(T))  {}

//	Other Operations (specified)
	void operator = (const Couple& rhs)
	{
		first  = rhs.first;
		second = rhs.second;
	}
	void swap(Couple& oth)
	{
		omni::swap(first,  oth.first );
		omni::swap(second, oth.second);
	}

	T first, second;
};

//----------------------------------------------------------------------
// Section      : Value identity
//----------------------------------------------------------------------

template <typename T>
bool operator == (const Couple<T>& lhs, const Couple<T>& rhs)
{
	return
		lhs.first  == rhs.first
	&&	lhs.second == rhs.second;
}

template <typename T>
bool operator != (const Couple<T>& lhs, const Couple<T>& rhs)
{
	return
		lhs.first  != rhs.first
	||	lhs.second != rhs.second;
}

//----------------------------------------------------------------------
// Section      :	ordering
//----------------------------------------------------------------------

template <typename T>
bool operator < (const Couple<T>& lhs, const Couple<T>& rhs)
{
	return
		lhs.first < rhs.first ||
		(!(rhs.first  < lhs.first)
			&& lhs.second < rhs.second
			);
}

//----------------------------------------------------------------------
// Section      : Undefined
//----------------------------------------------------------------------

template <class T>
inline void MakeUndefined(Couple<T>& v)
{
	MakeUndefined(v.first);
	MakeUndefined(v.second);
}

template <class T>
inline void MakeUndefinedOrZero(Couple<T>& v)
{
	MakeUndefinedOrZero(v.first);
	MakeUndefinedOrZero(v.second);
}

template <class T>
inline bool IsDefined(const Couple<T>& x)
{
	return IsDefined(x.first) || IsDefined(x.second);
}

//----------------------------------------------------------------------
// Section      : casting
//----------------------------------------------------------------------

template <class T>
inline Float64 AsFloat64(const Couple<T>& x )
{
	throwIllegalAbstract(MG_POS, "Point::AsFloat64");
}

#endif // __RTC_GEO_COUPLE_H
