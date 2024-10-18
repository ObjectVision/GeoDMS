// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_DBG_TIMER_H)
#define __RTC_DBG_TIMER_H

#include <time.h>
#include <atomic>

#include "geo/MinMax.h"

struct Timer
{
	void Reset()
	{
		time_t curr_time = time(nullptr);
		last_time = curr_time;
	}

	bool PassedSecs_impl(time_t nrSecs)
	{
		time_t curr_time = time(nullptr);

		if (curr_time < last_time + nrSecs)
			return false;

		// avoid double ticketing
		time_t old_time = last_time.exchange(curr_time);
		return curr_time >= old_time + nrSecs;
	}

	bool PassedSec()
	{
		return PassedSecs_impl(1);
	}

	bool PassedSecs()
	{
		bool result = PassedSecs_impl(nextWaitTime);
		if (result)
		{
			if (nextWaitTime < 300)
				nextWaitTime *= 2;
			else
				nextWaitTime = 600;
		}
		return result;
	}

//	time_t begin_time;
	std::atomic<time_t> last_time = time(nullptr);

	UInt32 nextWaitTime = 5; // in seconds
};

#endif // __RTC_DBG_TIMER_H
