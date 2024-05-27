// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
	Couple() : first(), second() {} // value initialization
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
