// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  mySSPrintF: heap-allocating string formatting to a SharedStr — a thin
 *  forward to the std::format based mgFormat2SharedStr (ptr/SharedStr.h).
 *  The fixed-buffer formatting family (and its <strstream> dependency)
 *  lives in utl/FixedBufferFormat.h.
 */

#if !defined(__UTL_STRFORMAT_H)
#define __UTL_STRFORMAT_H

#include "ptr/SharedStr.h"
#include "utl/MgFormat.h"

//----------------------------------------------------------------------

template<typename ...Args>
SharedStr mgFormat2SharedStr(CharPtr msg, Args&&... args)
{
	return SharedStr(mgFormat2string<Args...>(msg, std::forward<Args>(args)...));
}

template<typename ...Args>
SharedStr mySSPrintF(CharPtr format, Args&&... args) {
	return mgFormat2SharedStr<Args...>(format, std::forward<Args>(args)...);
}

#endif // __UTL_STRFORMAT_H
