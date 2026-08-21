// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__UTL_STRINGFUNC_H)
#define __UTL_STRINGFUNC_H

//----------------------------------------------------------------------

#include "RtcBase.h"
#include <string>

//----------------------------------------------------------------------

template <typename C1, typename C2>
UInt32 StrCount(C1 arg1, C2 arg2)
{
	auto a1b = arg1.begin(), a1e = arg1.end();
	auto a2b = arg2.begin(), a2e = arg2.end();

	UInt32 arg2size = a2e-a2b;
	UInt32 c = 0;
	for (auto p = a1b; p = std::search(p, a1e, a2b, a2e), p!=a1e; p += arg2size)
		++c;
	return c;
}

template <typename StringRef, typename C1, typename C2, typename C3>
void ReplaceAssign(StringRef res, C1 arg1, C2 arg2, C3 arg3)
{
	auto arg2len = arg2.size();
	if (!arg2len)
	{
		res = arg1;
		return;
	}
	auto arg3len = arg3.size();
	auto resLen  = arg1.size();
	if (arg3len != arg2len)
		resLen += StrCount(arg1, arg2) * (arg3len - arg2len);

	res.resize_uninitialized(resLen MG_DEBUG_ALLOCATOR_SRC("ReplaceAssign result buffer"));

	auto i = arg1.begin(), e = arg1.end();
	auto a2b = arg2.begin(), a2e = arg2.end();
	auto a3b = arg3.begin(), a3e = arg3.end();
	auto r = begin_ptr(res);

	while (true)
	{
		typename C1::const_iterator p = std::search(i, e, a2b, a2e);
		r = fast_copy(i, p, r);
		if (p == e) break;
		r = fast_copy(a3b, a3e, r);
		i = p + arg2len;
	}
	assert(r == end_ptr(res));
}


//----------------------------------------------------------------------

#endif // __UTL_STRINGFUNC_H