// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  IndexRange<T>: a Couple-based index interval with undefined-aware
 *  semantics.
 */

#if !defined(__VT_INDEXRANGE_H)
#define __VT_INDEXRANGE_H

#include "vt/Couple.h"

//=======================================
// IndexRange
//=======================================


template <typename T>
struct IndexRange : Couple<T>
{
	using Couple<T>::first;
	using Couple<T>::second;

	IndexRange() {} // value-initialize
	IndexRange(T v1, T v2): Couple<T>(v1, v2)  {}

	IndexRange(Undefined): Couple<T>(Undefined() ) {}

	void operator +=(T inc)
	{
		first += inc;
		second += inc;
	}

	bool empty() const { return first == second; }
	T    size () const { return second - first; }
};

//----------------------------------------------------------------------
// Section      : Undefined
//----------------------------------------------------------------------

template <class T>
inline constexpr IndexRange<T> UndefinedValue(const IndexRange<T>*)
{
	return IndexRange<T>(Undefined());
}


#endif // !defined(__VT_INDEXRANGE_H)
