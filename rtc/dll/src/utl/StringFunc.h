//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
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
	UInt32 arg2len = arg2.size();
	if (!arg2len)
	{
		res = arg1;
		return;
	}
	UInt32 arg3len = arg3.size();
	UInt32 resLen  = arg1.size();
	if (arg3len != arg2len)
		resLen += StrCount(arg1, arg2) * (arg3len - arg2len);

	res.resize_uninitialized(resLen);

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
	dms_assert(r == end_ptr(res));
}


//----------------------------------------------------------------------

#endif // __UTL_STRINGFUNC_H