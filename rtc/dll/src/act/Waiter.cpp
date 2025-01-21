// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/Waiter.h"

#include <set>
#include <tuple>

#include <assert.h>
#include "act/MainThread.h"

using callback_record = std::tuple< wating_event_callback, wating_event_callback, void* >;

static std::atomic<UInt32> s_WaiterCount = 0;
static std::set<callback_record> s_WaitingCallbacks;


//----------------------------------------------------------------------
// config section
//----------------------------------------------------------------------

static bool g_BusyMode;

bool IsBusy()
{
	return g_BusyMode;
}

void SetBusy(bool v)
{
	g_BusyMode = v;
}

void Waiter::start(AbstrMsgGenerator* ach)
{
	assert(IsMainThread());
	if (m_is_counted)
		return;
	m_is_counted = true;
	m_ContextGenerator = ach;

	if (s_WaiterCount++)
		return;

	SetBusy(true);
	for (const auto& we : s_WaitingCallbacks)
		if (std::get<0>(we))
			std::get<0>(we)(std::get<2>(we), m_ContextGenerator);

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
	{
		auto onEndWaitingFunc = std::get<1>(we);
		if (!onEndWaitingFunc)
			continue;
		auto clientHandle = std::get<2>(we);
		onEndWaitingFunc(clientHandle, m_ContextGenerator);
	}
	SetBusy(false);
}

bool Waiter::IsWaiting()
{
	return s_WaiterCount;
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

