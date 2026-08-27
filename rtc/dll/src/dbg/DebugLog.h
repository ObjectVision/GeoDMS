// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_DBG_DEBUGLOG_H
#define __RTC_DBG_DEBUGLOG_H

#include "DbgInterface.h"
#include "ser/FileStreamBuff.h"
#include "ser/FormattedStream.h"


// *****************************************************************************
// CDebugLog
// *****************************************************************************

class CDebugLog {
public:
	RTC_CALL CDebugLog(WeakStr name);
	RTC_CALL ~CDebugLog();

	RTC_CALL CDebugLog(WeakStr name, bool tag); // called from the public constructor with a DatedFileName

	RTC_CALL void Flush(); // see DBG_FlushLogs

	// Append one line straight to the file, bypassing the msg dispatch and its g_DebugStream lock.
	// Only for the fatal handlers, which must not risk blocking on a lock this very thread may hold.
	RTC_CALL void WriteRawLine(CharPtr line);

private:
	static void DMS_CONV DebugMsgCallback(ClientHandle clientHandle, const MsgData* msgData, bool moreToCome);

	FileOutStreamBuff   m_FileBuff;
	FormattedOutStream  m_Stream;
};

#endif  // __RTC_DBG_DEBUGLOG_H
