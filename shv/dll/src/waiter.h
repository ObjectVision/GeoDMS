// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_WAITER_H)
#define __SHV_WAITER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// struct : Waiter
//----------------------------------------------------------------------

struct Waiter {
	~Waiter() { end(); }

	SHV_CALL void start();
	SHV_CALL void end();

	bool m_is_counted = false;
};

using wating_event_callback = void (*)(void* clientHandle);

SHV_CALL void register_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle);
SHV_CALL void unregister_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle);

#endif // !defined(__SHV_WAITER_H)
