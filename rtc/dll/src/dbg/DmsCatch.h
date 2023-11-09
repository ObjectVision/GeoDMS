// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__DBG_DMSCATCH_H)
#define __DBG_DMSCATCH_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"
#include "act/MainThread.h"
#include "dbg/SeverityType.h"
#include "dbg/DebugContext.h"
#include "ptr/SharedStr.h"
#include "ser/DebugOutStream.h"
#include "xct/ErrMsg.h"
//----------------------------------------------------------------------
// Exceptions
//----------------------------------------------------------------------

RTC_CALL ErrMsgPtr catchException(bool rethrowCancelation);
RTC_CALL ErrMsgPtr catchAndReportException();
RTC_CALL void catchAndProcessException();


#	if defined(_MSC_VER)

#include "excpt.h"

		// functions defined in DmsException.cpp

		RTC_CALL TCppExceptionTranslator SetCppTranslator(TCppExceptionTranslator trFunc); 

		[[noreturn]] RTC_CALL void call_trans_SE2DMSfunc();
		[[noreturn]] RTC_CALL void  call_HaltOnSE();
		RTC_CALL int signalHandling(unsigned int u, _EXCEPTION_POINTERS* pExp, bool passBorlandException);

		struct CppTranslatorContext
		{
			RTC_CALL CppTranslatorContext(TCppExceptionTranslator trFunc = nullptr);
			RTC_CALL ~CppTranslatorContext();

		private:
			TCppExceptionTranslator m_PrevCppTranslator;
		};

#		define DMS_EH_CONTEXT CppTranslatorContext tc;
#		define DMS_SE_CALL_BEGIN     __try {
#		define DMS_SE_CALL_END     } __except(signalHandling(GetExceptionCode(), GetExceptionInformation(), true)) { call_HaltOnSE(); }

#		define DMS_SE_CALLBACK_BEGIN __try { 
#		define DMS_SE_CALLBACK_END } __except( signalHandling(GetExceptionCode(), GetExceptionInformation(), false) ) { call_trans_SE2DMSfunc(); }

#	else

#		define DMS_EH_CONTEXT
#		define DMS_SE_CALL_BEGIN
#		define DMS_SE_CALL_END

#		define DMS_SE_CALLBACK_BEGIN
#		define DMS_SE_CALLBACK_END
#	endif	

#	define DMS_CALL_BEGIN try { DMS_EH_CONTEXT
#	define DMS_CALL_END } catch (...) { catchAndProcessException(); }
#	define DMS_CALL_END_NOTHROW } catch (...) { catchAndReportException(); }

#endif // __DBG_DMSCATCH_H
