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

template <typename T>
class add_fake_copy_constructor
{
    mutable T     m_data;
    mutable bool  m_hasValue = false;

public:
    // Default
    add_fake_copy_constructor() = default;

    // Construct from an rvalue T
    add_fake_copy_constructor(T&& v) 
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(v)), m_hasValue(true)
    {}

    // “Fake” copy‐ctor
    add_fake_copy_constructor(const add_fake_copy_constructor& rhs)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(rhs.m_data)), m_hasValue(rhs.m_hasValue)
    {
        rhs.m_hasValue = false;
    }

    // Move‐ctor
    add_fake_copy_constructor(add_fake_copy_constructor&& rhs)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(rhs.m_data)), m_hasValue(rhs.m_hasValue)
    {
        rhs.m_hasValue = false;
    }


    // “Fake” copy‐assign
    void operator=(const add_fake_copy_constructor& rhs)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        m_data = std::move(rhs.m_data);
        m_hasValue = rhs.m_hasValue;
        rhs.m_hasValue = false;
    }

    //  Move‐assign
    void operator=(add_fake_copy_constructor&& rhs)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (this != &rhs) {
            m_data = std::move(rhs.m_data); // value or zombie, take it anyways
            m_hasValue = rhs.m_hasValue;
            rhs.m_hasValue = false; // make sure the rhs is known as a zombie
        }
    }

    // Take out the value (non‐const)
    T take() const
    {
        if (!m_hasValue)
            throw std::runtime_error("Object not initialized");
        m_hasValue = false; // make sure the object is not used again
        return std::move(m_data);
    }

    // value‐conversion
    operator T() const
    {
        return take();   // reuse your non‐const take()
    }

    // 10) Bool‐test
    explicit operator bool() const noexcept { return m_hasValue; }
};


template <typename Functor>
using future = std::invoke_result_t<Functor>;

template <typename R>
using make_copy_constructible = std::conditional_t<std::is_copy_constructible_v<R>, R, add_fake_copy_constructor< R >>;

template <typename Functor>
using movable_future = make_copy_constructible<future<Functor>>;

template <typename Functor>
auto throttled_async(Concurrency::task_group& gr, Functor&& f) -> concurrency::task<movable_future<Functor>>
{
	using R = movable_future<Functor>;

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
