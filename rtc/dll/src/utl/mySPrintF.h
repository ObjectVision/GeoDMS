// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__UTL_MYSPRINTF_H)
#define __UTL_MYSPRINTF_H

//----------------------------------------------------------------------

#include <stdarg.h>
#include <strstream>

#include "RtcBase.h"
#include "dbg/debug.h"
#include "ptr/SharedStr.h"

//----------------------------------------------------------------------

RTC_CALL SharedStr myVSSPrintF(CharPtr format, va_list argList);
RTC_CALL CharPtr RepeatedDots(SizeT n);

//----------------------------------------------------------------------

template<typename ...Args>
CharPtr myFixedBufferAsCString(char* buf, SizeT size, CharPtr format, Args&&... args) {
	std::ostrstream os(buf, size);
	os << mgFormat(format, std::forward<Args>(args)...) << std::ends;
	CharPtr str = os.str();
	assert(SizeT(os.pcount()) <= size);
	return str;
}

template<typename ...Args>
SizeT myFixedBufferWrite(char* buf, SizeT size, CharPtr format, Args&&... args) {
	std::ostrstream os(buf, size);
	os << mgFormat(format, std::forward<Args>(args)...);
 	assert(SizeT(os.pcount()) < size);
	return os.pcount();
}

template<typename ...Args>
CharPtrRange myFixedBufferAsCharPtrRange(char* buf, SizeT size, CharPtr format, Args&&... args) {
	SizeT sz = myFixedBufferWrite(buf, size, format, std::forward<Args>(args)...);
	return CharPtrRange(buf, buf + sz);
}


template<typename ...Args>
SharedStr mySSPrintF(CharPtr format, Args&&... args) {
	return mgFormat2SharedStr<Args...>(format, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------

template <UInt32 N>
struct myArrayPrintF
{
	template<typename ...Args>
	myArrayPrintF(CharPtr format, Args&&... args)
	{
		m_Size = myFixedBufferWrite<Args...>(m_Buff, N, format, std::forward<Args>(args)...);
	}
	SizeT m_Size;
	char m_Buff[N];
	operator CharPtrRange() const { return { m_Buff, m_Buff + m_Size }; }
};

#endif // __UTL_MYSPRINTF_H