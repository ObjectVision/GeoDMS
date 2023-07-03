// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "Waiter.h"

#include <set>
#include <tuple>

#include <assert.h>
#include "act/MainThread.h"

using callback_record = std::tuple< wating_event_callback, wating_event_callback, void* >;

static std::atomic<UInt32> s_WaiterCount = 0;
static std::set<callback_record> s_WaitingCallbacks;


void Waiter::start()
{
	assert(IsMainThread());
	if (m_is_counted)
		return;
	m_is_counted = true;

	if (s_WaiterCount++)
		return;
	for (const auto& we : s_WaitingCallbacks)
		if (std::get<0>(we))
			std::get<0>(we)(std::get<2>(we));
}

void Waiter::end()
{
	assert(IsMainThread());
	if (!m_is_counted)
		return;
	m_is_counted = false;

	if (--s_WaiterCount)
		return;

	for (const auto& we : s_WaitingCallbacks)
		if (std::get<1>(we))
			std::get<1>(we)(std::get<2>(we));
}


void register_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle)
{
	assert(IsMainThread());
	s_WaitingCallbacks.insert(callback_record( starting, ending, clientHandle ));
}

void unregister_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle)
{
	assert(IsMainThread());
	s_WaitingCallbacks.erase(callback_record(starting, ending, clientHandle));
}

