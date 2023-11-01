// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_WAITER_H)
#define __SHV_WAITER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvBase.h"

//----------------------------------------------------------------------
// struct : Waiter
//----------------------------------------------------------------------

struct Waiter {
	Waiter() {}
	Waiter(AbstrMsgGenerator* ach) { start(ach); }
	~Waiter() { end(); }

	SHV_CALL void start(AbstrMsgGenerator* ach);
	SHV_CALL void end();
	SHV_CALL static bool IsWaiting(); 

	AbstrMsgGenerator* m_ContextGenerator = nullptr;
	bool m_is_counted = false;
};

using wating_event_callback = void (*)(void* clientHandle, AbstrMsgGenerator* ach);

SHV_CALL void register_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle);
SHV_CALL void unregister_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle);

#endif // !defined(__SHV_WAITER_H)
