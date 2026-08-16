// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_ACT_WAITER_H)
#define __RTC_ACT_WAITER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"
#include "dbg/DebugContext.h"

//----------------------------------------------------------------------
// config section
//----------------------------------------------------------------------

RTC_CALL bool IsBusy();

//----------------------------------------------------------------------
// struct : Waiter
//----------------------------------------------------------------------

struct Waiter {
	Waiter() {}
	Waiter(AbstrMsgGenerator* ach) { start(ach); }
	~Waiter() { end(); }

	RTC_CALL void start(AbstrMsgGenerator* ach);
	RTC_CALL void end();
	RTC_CALL static bool IsWaiting();

	AbstrMsgGenerator* m_ContextGenerator = nullptr;
	bool m_is_counted = false;
};

using wating_event_callback = void (*)(void* clientHandle, AbstrMsgGenerator* ach);

RTC_CALL void register_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle);
RTC_CALL void unregister_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle);

#endif // !defined(__RTC_ACT_WAITER_H)
