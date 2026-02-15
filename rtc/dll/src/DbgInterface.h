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

RTC_CALL void MustCoalesceHeap(SizeT size);

RTC_CALL auto SetASyncContinueCheck(TASyncContinueCheck asyncContinueCheckFunc) -> TASyncContinueCheck;
RTC_CALL void ASyncContinueCheck();

extern "C" {

RTC_CALL void       DMS_CONV DMS_SetContextNotification(TContextNotification cnFunc, ClientHandle clientHandle);


RTC_CALL void       DMS_CONV DMS_RegisterMsgCallback(MsgCallbackFunc fcb, ClientHandle clientHandle);
RTC_CALL void       DMS_CONV DMS_ReleaseMsgCallback(MsgCallbackFunc fcb, ClientHandle clientHandle);

RTC_CALL CDebugLog* DMS_CONV DBG_DebugLog_Open(CharPtr fileName);
RTC_CALL void       DMS_CONV DBG_DebugLog_Close(CDebugLog*);


RTC_CALL void       DMS_CONV DBG_DebugReport();

RTC_CALL void       DMS_CONV DMS_SetCoalesceHeapFunc(CoalesceHeapFuncType coalesceHeapFunc);
RTC_CALL bool       DMS_CONV DMS_CoalesceHeap(std::size_t requiredSize);

}

// not called from outside

void MsgDispatch(SeverityTypeID st, MsgCategory msgCat, CharPtr msg);

RTC_CALL void DBG_TraceStr(CharPtr msg);

RTC_CALL bool DMS_Test(CharPtr name, CharPtr condStr, bool cond);

#endif  // __DBG_INTERFACE_H
