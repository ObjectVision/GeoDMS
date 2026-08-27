// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __DBG_INTERFACE_H
#define __DBG_INTERFACE_H

#include "RtcBase.h"

class CDebugContext;
class CDebugLog;

using CoalesceHeapFuncType = bool (*)(std::size_t, CharPtr);
enum class MsgCategory : UInt8;


struct MsgData;

//----------------------------------------------------------------------
// class  : ContextNotification
//----------------------------------------------------------------------

typedef void (DMS_CONV *TContextNotification)(ClientHandle clientHandle, CharPtr description);

void ProgressMsg(CharPtr msg);

// The following typedef defines the type MsgCallbackFunc
// which is a pointer to a procedure type that takes 
//		st: an integer representing a severity,
//		msg: a PChar representing a DMS generated message
//		clientHandle: a client suppplied DWord to identify a client object that handles the message

using MsgCallbackFunc = void (DMS_CONV *)(ClientHandle clientHandle, const MsgData* data, bool moreToCome);
using TASyncContinueCheck = void (DMS_CONV *)();

void MustCoalesceHeap(SizeT size);

RTC_CALL auto SetASyncContinueCheck(TASyncContinueCheck asyncContinueCheckFunc) -> TASyncContinueCheck;
RTC_CALL void ASyncContinueCheck();

extern "C" {

RTC_CALL void       DMS_CONV DMS_SetContextNotification(TContextNotification cnFunc, ClientHandle clientHandle);


RTC_CALL void       DMS_CONV DMS_RegisterMsgCallback(MsgCallbackFunc fcb, ClientHandle clientHandle);
RTC_CALL void       DMS_CONV DMS_ReleaseMsgCallback(MsgCallbackFunc fcb, ClientHandle clientHandle);

RTC_CALL CDebugLog* DMS_CONV DBG_DebugLog_Open(CharPtr fileName);
RTC_CALL void       DMS_CONV DBG_DebugLog_Close(CDebugLog*);


RTC_CALL void       DMS_CONV DBG_DebugReport();

// Push every open session log to disk. A log line only reaches the file when the ofstream buffer
// happens to fill or when ~CDebugLog closes it, so any exit that skips destructors -- above all a
// fail-fast -- loses the tail, which is exactly the part that says why the run ended (#1191).
RTC_CALL void       DMS_CONV DBG_FlushLogs();

// Install the fatal-path diagnostics: a std::terminate handler (and, on MSVC, a purecall handler)
// that name the in-flight exception and the thread's context chain, flush the logs, and then end the
// process deliberately instead of letting it die at ucrtbase!abort with a bare 0xC0000409.
//
// MUST be called on every thread that can throw: MSVC keeps the terminate handler PER THREAD, so an
// install on the main thread alone leaves worker threads with the default handler (verified). The
// portable_task_group workers call this themselves; RtcStreamLock covers the main thread.
RTC_CALL void       DMS_CONV DBG_InstallFatalHandlers();

// Report an exception that reached a boundary which must not throw: a noexcept function, a thread
// function, a destructor. Call ONLY from inside a catch handler -- it re-raises with a bare `throw;`
// to classify what it is holding. task_canceled is logged as a trace (it is the designed outcome of
// abandoning work at teardown), anything else as an error. Never throws itself. See #1191.
RTC_CALL void       DMS_CONV DBG_ReportBoundaryException(CharPtr where);

RTC_CALL void       DMS_CONV DMS_SetCoalesceHeapFunc(CoalesceHeapFuncType coalesceHeapFunc);
RTC_CALL bool       DMS_CONV DMS_CoalesceHeap(std::size_t requiredSize);

}

// not called from outside

void MsgDispatch(SeverityTypeID st, MsgCategory msgCat, CharPtr msg);

RTC_CALL void DBG_TraceStr(CharPtr msg);

RTC_CALL bool DMS_Test(CharPtr name, CharPtr condStr, bool cond);

#endif  // __DBG_INTERFACE_H
