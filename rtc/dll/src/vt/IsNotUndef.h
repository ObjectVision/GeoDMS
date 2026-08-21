// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  IsNotUndef(v): the per-type not-undefined predicate, including the
 *  bit_value overloads.
 */

#if !defined(__VT_ISNOTUNDEF_H)
#define __VT_ISNOTUNDEF_H

#include "vt/BitValue.h"
#include "vt/BaseBounds.h"

template <typename T> inline bool IsNotUndef(const T& v)
{
	return IsDefined(v);
}

template <bit_size_t N> inline bool IsNotUndef(bit_value<N>)
{
	return true;
}


#endif // !defined(__VT_ISNOTUNDEF_H)
