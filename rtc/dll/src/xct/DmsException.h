// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

// DmsException.h: interface for the DmsException class.

#if !defined(__RTC_XCT_DMSEXCEPTION_H)
#define __RTC_XCT_DMSEXCEPTION_H

#include "ser/format.h"
#include "xct/ErrMsg.h"

enum ExceptionContextState {
	ECS_Normal,
	ECS_SE,
};

RTC_CALL extern void (*s_OnTerminationFunc)();

struct DmsException : std::exception, private ErrMsgPtr
{
	[[noreturn]] static RTC_CALL void throwMsg(ErrMsgPtr msg);
	[[noreturn]] static RTC_CALL void throwMsgD(WeakStr);
	[[noreturn]] static RTC_CALL void throwMsgD(CharPtr);

	template <class ...Args>
	[[noreturn]] static void throwMsgF(CharPtr fmt, Args&& ...args) { throwMsgD(mgFormat2SharedStr(fmt, std::forward<Args>(args)...)); }

	ErrMsgPtr AsErrMsg() const { return *this;  }
	RTC_CALL const char* what() const noexcept override;
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
