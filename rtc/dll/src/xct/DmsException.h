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

// DmsException.h: interface for the DmsException class.

#ifndef __RTC_XCT_DMSEXCEPTION_H
#define __RTC_XCT_DMSEXCEPTION_H 1

#include "ser/format.h"
#include "xct/ErrMsg.h"

enum ExceptionContextState {
	ECS_Normal,
	ECS_SE,
};

struct DmsException : private ErrMsgPtr
{
	[[noreturn]] static RTC_CALL void throwMsg(ErrMsgPtr msg);
	[[noreturn]] static RTC_CALL void throwMsgD(WeakStr);
	[[noreturn]] static RTC_CALL void throwMsgD(CharPtr);

	template <class ...Args>
	[[noreturn]] static void throwMsgF(CharPtr fmt, Args&& ...args) { throwMsgD(mgFormat2SharedStr(fmt, std::forward<Args>(args)...)); }

	ErrMsgPtr AsErrMsg() const { return *this;  }

	auto GetAsText() const { return get()->GetAsText(); }

	RTC_CALL virtual ~DmsException();

	RTC_CALL DmsException(ErrMsgPtr msg);

	ErrMsgPtr m_PrevUnrollingErrMsgPtr;
};

//----------------------------------------------------------------------
// cachable MemoryAllocFailure
//----------------------------------------------------------------------

struct MemoryAllocFailure: DmsException
{
	RTC_CALL MemoryAllocFailure();
};

//----------------------------------------------------------------------
// API funcs
//----------------------------------------------------------------------

extern "C" RTC_CALL void DMS_CONV DMS_ReportError (CharPtr msg);
extern "C" RTC_CALL void DMS_CONV DMS_DisplayError(CharPtr msg);

RTC_CALL SharedStr GetErrorContext(WeakStr msg);
RTC_CALL SharedStr GetFirstLine(WeakStr msg);
RTC_CALL SharedStr GetLastErrorMsgStr();
RTC_CALL ErrMsgPtr GetUnrollingErrorMsgPtr();
RTC_CALL unsigned int GetLastExceptionCode();

#endif // __RTC_XCT_DMSEXCEPTION_H
