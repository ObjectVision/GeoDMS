// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_SET_DATACOMPARE_H)
#define __RTC_SET_DATACOMPARE_H

#include "geo/Undefined.h"

template<typename T> constexpr bool equality_must_check_undefines_v = has_undefines_v<T> && std::is_floating_point_v<scalar_of_t<T>>;
template<typename T> constexpr bool compare_must_check_undefines_v = has_undefines_v<T> && !has_min_as_null_v<T> && has_fixed_elem_size_v<T>;

// *****************************************************************************
//                      INDEX operations
// *****************************************************************************

template <typename T, bool CheckUndefined> struct DataLessThanCompareImpl;

template <typename T> struct DataLessThanCompareImpl<T, false> : std::less<void>
{};

template <typename T> struct DataLessThanCompareImpl<T, true> {
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return IsDefined(right) && ((left < right) || !IsDefined(left));
	}
};


template <typename T>
struct DataLessThanCompare : DataLessThanCompareImpl<T, compare_must_check_undefines_v<T> > {};

template <typename T>
struct DataLessThanCompare<Point<T> >
{
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return m_ElemComp(left.Row(), right.Row())
			||	!m_ElemComp(right.Row(), left.Row())
			&& m_ElemComp(left.Col(), right.Col())
			;
	}
	DataLessThanCompare<T> m_ElemComp;
};

template <typename T, bool CheckUndefined> struct DataEqualityCompareImpl;

template <typename T>
struct DataEqualityCompareImpl<T, false> : std::equal_to<void>
{};

template <typename T>
struct DataEqualityCompareImpl<T, true>
{
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return left == right || !IsDefined(left) && !IsDefined(right);
	}
};

template <typename T>
struct DataEqualityCompare : DataEqualityCompareImpl<T, equality_must_check_undefines_v<T> > {};

template <typename T>
struct DataEqualityCompare<Point<T> >
{
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return m_ElemComp(left.Row(), right.Row())
			&& m_ElemComp(left.Col(), right.Col())
		;
	}
	DataEqualityCompare<T> m_ElemComp;
};


#endif // __RTC_SET_DATACOMPARE_H
