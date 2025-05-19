// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_ASYNC_H)
#define __RTC_ASYNC_H

#include "RtcBase.h"
#include "utl/AddFakeCopyCTor.h"
#include "utl/MemGuard.h"
#include "utl/scoped_exit.h"

#include "ppl.h"
#include "ppltasks.h"

template <typename Functor>
using funtor_result_t = std::invoke_result_t<Functor>;

template <typename R>
using make_copy_constructible = std::conditional_t<std::is_copy_constructible_v<R>, R, add_fake_copy_constructor< R >>;

template <typename Functor>
using movable_functor_result_t = make_copy_constructible<funtor_result_t<Functor>>;

template <typename Functor>
auto throttled_async(Concurrency::task_group& gr, Functor&& f) -> Concurrency::task<movable_functor_result_t<Functor>>
{
	using R = movable_functor_result_t<Functor>;

	// 1) Create an event and wrap it in a task
	concurrency::task_completion_event<R> tce;
	concurrency::task<R> t{ tce };

	if (IsMultiThreaded1() && !IsLowOnFreeRAM())
	{
//		return std::async(std::launch::async, [fn = std::forward<Functor>(f)]
		gr.run(
			[tce, f = std::forward<Functor>(f)] ()
			{
				try {
					if constexpr (std::is_void_v<R>)
					{
						f();
						tce.set();
					}
					else
						tce.set(f());
				}
				catch (Concurrency::task_canceled&)
				{
					throw;
				}
				catch (...) {
					tce.set_exception(std::current_exception());
				}
			}
		);
	}
	else
	{
		if constexpr (std::is_void_v<R>)
		{
			f();
			tce.set();
		}
		else
			tce.set(f()); // don't be lazy and don't trash.
	}

	return t;
}

#endif // __RTC_ASYNC_H
