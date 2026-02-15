//<HEADER> 
// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_GEO_ISNOTUNDEF_H)
#define __RTC_GEO_ISNOTUNDEF_H

#include "geo/BitValue.h"
#include "geo/BaseBounds.h"

template <typename T> inline bool IsNotUndef(const T& v)
{
	return IsDefined(v);
}

template <bit_size_t N> inline bool IsNotUndef(bit_value<N>)
{
	return true;
}


#endif // !defined(__RTC_GEO_ISNOTUNDEF_H)
