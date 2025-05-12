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
#include <semaphore>
#include "ppl.h"

RTC_CALL extern std::counting_semaphore<> s_MtSemaphore;



template <typename Functor>
auto throttled_async(Concurrency::task_group& gr, Functor&& f) -> std::future<std::invoke_result_t<Functor>>
{
	using R = std::invoke_result_t<Functor>;

	if (IsMultiThreaded1() && !IsLowOnFreeRAM() && s_MtSemaphore.try_acquire())
		{
//			return std::async(std::launch::async, [fn = std::forward<Functor>(f)]
		auto p = std::make_shared<std::promise<R>>();
			gr.run(
				[p, f = std::forward<Functor>(f)] ()
				{
					try {
						auto x = make_scoped_exit([] {s_MtSemaphore.release(); });
						if constexpr (std::is_void_v<R>)
						{
							f();
							p->set_value();
						}
						else
							p->set_value(f());
					}
					catch (Concurrency::task_canceled&)
					{
						throw;
					}
					catch (...) {
						p->set_exception(std::current_exception());
					}
				}
			);
			return p->get_future();
		}

	std::promise<R> p;
	if constexpr (std::is_void_v<R>)
	{
		f();
		p.set_value();
	}
	else
		p.set_value(f()); // don't be lazy and don't trash.

	return p.get_future();
}

#endif // __RTC_ASYNC_H
