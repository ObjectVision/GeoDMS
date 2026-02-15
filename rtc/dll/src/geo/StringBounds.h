// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__GEO_STRINGBOUNDS_H)
#define __GEO_STRINGBOUNDS_H

//----------------------------------------------------------------------
// Section      : String specific functions 
//----------------------------------------------------------------------

RTC_CALL SizeT StrLen(CharPtr str);

inline SizeT StrLen(CharPtr b, CharPtr e)
{
	CharPtr i = b;
	while (i != e && *i)
		++i;
	return i - b;
}

inline SizeT StrLen(CharPtr str, SizeT maxLen)
{
	return StrLen(str, str + maxLen);
}

#endif //__GEO_STRINGBOUNDS_H