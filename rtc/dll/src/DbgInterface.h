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

using MsgCallbackFunc = void (DMS_CONV *)(ClientHandle clientHandle, MsgData* data);
using TASyncContinueCheck = void (DMS_CONV *)();

RTC_CALL void MustCoalesceHeap(SizeT size);

extern "C" {

RTC_CALL void       DMS_CONV DMS_SetContextNotification(TContextNotification cnFunc, ClientHandle clientHandle);

RTC_CALL void       DMS_CONV DMS_SetASyncContinueCheck(TASyncContinueCheck asyncContinueCheckFunc);

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

RTC_CALL void DMS_ASyncContinueCheck();
RTC_CALL void DBG_TraceStr(CharPtr msg);

RTC_CALL bool DMS_Test(CharPtr name, CharPtr condStr, bool cond);

#endif  // __DBG_INTERFACE_H
