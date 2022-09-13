#pragma once

#if !defined(__RTC_ASYNC_H)
#define __RTC_ASYNC_H

#include "RtcBase.h"
#include "utl/MemGuard.h"
#include "utl/scoped_exit.h"

#include <future>

RTC_CALL std::atomic<UInt32>& throttle_counter();
RTC_CALL UInt32 GetNrVCPUs();

/*
template <typename Functor>
auto throttled_async(Functor f) -> std::future<decltype(f())>
{
	if (throttle_counter()++ < GetNrVCPUs())
	{
		return std::async([f]() { auto x = make_scoped_exit([] {throttle_counter()--;  });  auto result = f(); ; return result;  });
	}
	throttle_counter()--;
	auto result = f();
	return std::future<decltype(f())>(result);
}
*/

template <typename Functor>
auto throttled_async(Functor f) -> std::future<decltype(f())>
{
	throttle_counter()++;
	if (throttle_counter() <= GetNrVCPUs() && !IsLowOnFreeRAM())
		return std::async([f]
		{ 
			auto x = make_scoped_exit([] {throttle_counter()--;  });  
			return f();
		});
	throttle_counter()--;
	std::promise<decltype(f())> prom;
	prom.set_value(f());
	return prom.get_future();
}

#endif // __RTC_ASYNC_H
