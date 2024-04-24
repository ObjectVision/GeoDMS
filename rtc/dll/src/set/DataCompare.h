// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_SET_DATACOMPARE_H)
#define __RTC_SET_DATACOMPARE_H

#include "geo/Undefined.h"

// *****************************************************************************
//                      INDEX operations
// *****************************************************************************

template <typename T, bool CheckUndefined> struct DataCompareImpl;

template <typename T> struct DataCompareImpl<T, false> {
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return std::forward<U1>(left) < std::forward<U2>(right);
	}
};

template <typename T> struct DataCompareImpl<T, true> {
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return IsDefined(right) && ((left < right) || !IsDefined(left));
	}
};

template <typename T>
struct DataCompare : DataCompareImpl<T, has_undefines_v<T> && !has_min_as_null_v<T> && has_fixed_elem_size_v<T>> {};

template <typename T>
struct DataCompare<Point<T> >
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
	DataCompare<T> m_ElemComp;
};


#endif // __RTC_SET_DATACOMPARE_H
