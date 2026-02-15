
// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_GEO_MINMAX_H)
#define __RTC_GEO_MINMAX_H

#include "geo/ElemTraits.h"
#include "utl/swap.h"

//----------------------------------------------------------------------
// Section      : lexicographical ordering: MakeMin, MakeMax, etc.
//----------------------------------------------------------------------
// Full name    : Aggregation helper definitioin
// Purpose      : fast operation for aggregation operations
//----------------------------------------------------------------------

template <class T>
inline T Min(T a, T b) { return (a<b) ? a : b; }

template <class T>
inline T Max(T a, T b) { return (a<b) ? b : a; }

template <class T>
inline bool MakeMin(T& a, std::type_identity_t<T> b) { if (!(b<a)) return false; a=b; return true; }

template <class T>
inline bool MakeMax(T& a, std::type_identity_t<T> b) { if (!(a<b)) return false; a=b; return true; }

template <class T> 
inline void MakeRange(T& a, T& b) { if (b<a) omni::swap(a, b); }

#endif // !defined(__RTC_GEO_MINMAX_H)
