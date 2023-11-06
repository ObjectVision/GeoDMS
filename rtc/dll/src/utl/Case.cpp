// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/SequenceArray.h"
#include "utl/case.h"

#include <ctype.h>

//================= UpperCase

inline char UpperCase(char ch)
{
	return toupper(ch);
}

void UpperCase(StringRef& result, CharPtr begin, CharPtr end)
{
	dms_assert(end || !begin);
	result.resize_uninitialized( end-begin );
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
	result.resize_uninitialized( end-begin );
	char* res = result.begin();
	while (begin != end)
		*res++ = LowerCase(*begin++);
}

