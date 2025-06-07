// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_GEO_STRINGARRAY_H)
#define __RTC_GEO_STRINGARRAY_H

#include "geo/SequenceArray.h"

//----------------------------------------------------------------------
// Section      : Assignments to/from sequence_array<char>::reference
//----------------------------------------------------------------------

inline void Assign(StringRef lhs, WeakStr rhs) 
{ 
	if (IsDefined(rhs))
		lhs.assign(rhs.begin(), rhs.send() MG_DEBUG_ALLOCATOR_SRC("Assign"));
	else
		lhs.assign(Undefined());
}

inline void Assign(StringRef lhs, CharPtr rhs)
{ 
	if (rhs)
		lhs.assign(rhs, rhs + StrLen(rhs) MG_DEBUG_ALLOCATOR_SRC("Assign"));
	else
		lhs.assign(Undefined());
}

inline void Assign(SharedStr& lhs, StringCRef rhs) 
{ 
	if (rhs.IsDefined())
		lhs = SharedStr(CharPtrRange(begin_ptr(rhs), end_ptr(rhs)) MG_DEBUG_ALLOCATOR_SRC("Assign"));
	else
		lhs = SharedStr(Undefined());
}

inline bool MakeLowerBound(StringRef a, StringCRef b)
{
	if (!(b<a)) return false;
	a=b;
	return true;
}

inline bool MakeUpperBound(StringRef a, StringCRef b)
{
	if (!(a<b)) return false;
	a=b;
	return true;
}
#endif //__RTC_GEO_STRINGARRAY_H