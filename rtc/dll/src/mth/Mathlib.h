// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MTH_MATHLIB_H)
#define __RTC_MTH_MATHLIB_H

/******************************************************************************/
//                         Analytical Functions
/******************************************************************************/

template <class T> inline bool Even    (const T& t) { return t % 2 == 0; }
template <class T> inline bool Odd     (const T& t) { return ! Even(t); }
template <class T> inline bool PowerOf2(const T& t) { return (t & (t-1)) == 0; }
template <class T> inline T    sqr     (const T& v) { return v*v; }

#endif //!defined(__RTC_MTH_MATHLIB_H)
