// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__GEO_INDEX_RANGE_H)
#define __GEO_INDEX_RANGE_H

#include "geo/Couple.h"

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
inline IndexRange<T> UndefinedValue(const IndexRange<T>*)
{
	return IndexRange<T>(Undefined());
}


#endif // !defined(__GEO_INDEX_RANGE_H)
