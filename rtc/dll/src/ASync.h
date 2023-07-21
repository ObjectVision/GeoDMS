#pragma once

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
	if (throttle_counter() <= GetNrVCPUs() && !IsLowOnFreeRAM())
		return std::async(std::launch::async, [f]
		{ 
			auto x = make_scoped_exit([] {throttle_counter()--;  });  
			return f();
		});
	throttle_counter()--;
	return std::async(std::launch::deferred, f);
}

#endif // __RTC_ASYNC_H
