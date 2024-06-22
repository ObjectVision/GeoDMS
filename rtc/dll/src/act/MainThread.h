// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_ACT_MAINTHREAD_H)
#define __RTC_ACT_MAINTHREAD_H

#include "RtcBase.h"

enum {
	WM_QT_ACTIVATENOTIFIERS = 0x402,
	UM_PROCESS_QUEUE = 0x8010,
	UM_PROCESS_MAINTHREAD_OPERS = 0x8003,
	UM_SCALECHANGE = 0x8011,
	UM_COPYDATA = 0x8004,
};

/********** helper funcs  **********/

RTC_CALL void   SetMainThreadID();
RTC_CALL void   SetMetaThreadID();
RTC_CALL bool   IsMainThread();
RTC_CALL bool   IsMetaThread();
RTC_CALL bool   NoOtherThreadsStarted();
RTC_CALL bool   IsElevatedThread();
RTC_CALL UInt32 GetCallCount();
RTC_CALL UInt32 GetThreadID();
RTC_CALL void PostMainThreadOper(std::function<void()>&& func);
RTC_CALL void SendMainThreadOper(std::function<void()>&& func);
RTC_CALL void ProcessMainThreadOpers();
RTC_CALL bool IsProcessingMainThreadOpers();
RTC_CALL void ConfirmMainThreadOperProcessing();

struct MainThreadBlocker
{
	RTC_CALL MainThreadBlocker();
	RTC_CALL ~MainThreadBlocker();
};

#endif // __RTC_ACT_MAINTHREAD_H