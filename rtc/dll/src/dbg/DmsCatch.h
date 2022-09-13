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

RTC_CALL ErrMsg catchException(bool rethrowCancelation);
RTC_CALL ErrMsg catchAndReportException();
RTC_CALL void catchAndProcessException();


#	if defined(_MSC_VER)

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
#	else
#		define DMS_EH_CONTEXT
#	endif	

#include "excpt.h"

#	define DMS_SE_CALL_BEGIN     __try {
#	define DMS_SE_CALL_END     } __except(signalHandling(GetExceptionCode(), GetExceptionInformation(), true)) { call_HaltOnSE(); }

#	define DMS_SE_CALLBACK_BEGIN __try { 
#	define DMS_SE_CALLBACK_END } __except( signalHandling(GetExceptionCode(), GetExceptionInformation(), false) ) { call_trans_SE2DMSfunc(); }

#	define DMS_CALL_BEGIN try { DMS_EH_CONTEXT
#	define DMS_CALL_END } catch (...) { catchAndProcessException(); }
#	define DMS_CALL_END_NOTHROW } catch (...) { catchAndReportException(); }


#endif // __DBG_DMSCATCH_H
