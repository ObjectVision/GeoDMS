// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "vt/SequenceArray.h"
#include "ptr/SharedStr.h"
#include "utl/Case.h"

#include <ctype.h>
#include <string>

//================= UpperCase

inline char UpperCase(char ch)
{
	return toupper(ch);
}

void UpperCase(StringRef& result, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	result.resize_uninitialized( end-begin MG_DEBUG_ALLOCATOR_SRC("UpperCase"));
	char* res = result.begin();
	while (begin != end)
		*res++ = UpperCase(*begin++);
}

//================= LowerCase

inline char LowerCase(char ch)
{
	return tolower(ch);
}

void LowerCase(StringRef& result, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	result.resize_uninitialized( end-begin MG_DEBUG_ALLOCATOR_SRC("LowerCase"));
	char* res = result.begin();
	while (begin != end)
		*res++ = LowerCase(*begin++);
}

SharedStr AsLowerCase(CharPtr begin, CharPtr end)
{
	std::string tmp(begin, end);
	for (char& ch : tmp)
		ch = LowerCase(ch);
	return SharedStr(tmp);
}

SharedStr AsLowerCase(CharPtr zStr)
{
	return AsLowerCase(zStr, zStr + std::char_traits<char>::length(zStr));
}

