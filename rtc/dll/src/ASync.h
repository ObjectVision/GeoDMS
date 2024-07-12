// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_ASYNC_H)
#define __RTC_ASYNC_H

#include "RtcBase.h"
#include "utl/MemGuard.h"
#include "utl/scoped_exit.h"

#include <future>

RTC_CALL std::atomic<UInt32>& throttle_counter();
RTC_CALL UInt32 GetNrVCPUs();

template <typename Functor>
auto throttled_async(Functor f) -> std::future<decltype(f())>
{
	throttle_counter()++;
	if (throttle_counter() <= GetNrVCPUs() && !IsLowOnFreeRAM() && IsMultiThreaded1())
		return std::async(std::launch::async, [f]
		{ 
			auto x = make_scoped_exit([] {throttle_counter()--;  });  
			return f();
		});
	throttle_counter()--;

	std::promise<decltype(f())> p;
	p.set_value(f()); // don't be lazy and don't trash.
	return p.get_future();
}

#endif // __RTC_ASYNC_H
