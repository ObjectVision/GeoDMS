// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Fixed-buffer formatting: the myFixedBuffer* helpers and myArrayPrintF<N>
 *  format into a caller-supplied or embedded char buffer without heap
 *  allocation (used e.g. for status-bar text), plus the va_list based
 *  myVSSPrintF and the RepeatedDots progress helper. This is the only
 *  header that drags in the deprecated <strstream>; include it only where
 *  fixed-buffer formatting is actually used (split out of mySPrintF.h,
 *  which sat in four PCH closures — see header-hygiene-2026-08.md §5B).
 */

#if !defined(__UTL_FIXEDBUFFERFORMAT_H)
#define __UTL_FIXEDBUFFERFORMAT_H

#include <stdarg.h>
// std::ostrstream (from the deprecated <strstream>) is still the simplest in-place,
// fixed-buffer formatter used by the myFixedBuffer* helpers below. Pre-defining the
// libstdc++ backward-header guard suppresses its deprecation banner without changing
// behavior; harmless on MSVC, which has no backward_warning.h.
// TODO: migrate to a custom fixed-buffer std::streambuf and drop <strstream>.
#ifndef _BACKWARD_BACKWARD_WARNING_H
#define _BACKWARD_BACKWARD_WARNING_H
#endif
#include <strstream>

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

#endif // __UTL_FIXEDBUFFERFORMAT_H
