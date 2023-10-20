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