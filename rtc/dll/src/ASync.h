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

#if defined(MG_DEBUG)
#define MG_DEBUG_FAKE_COPY_CONSTRUCTOR
#endif

template <typename T>
class add_fake_copy_constructor
{
    mutable T     m_data;
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
    mutable bool  m_hasValue = false;
#endif

public:
    // Default
    add_fake_copy_constructor() = default;

    // Construct from an rvalue T
    add_fake_copy_constructor(T&& v) 
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(v))
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        , m_hasValue(true)
#endif
    {}

    // “Fake” copy‐ctor
    add_fake_copy_constructor(const add_fake_copy_constructor& rhs)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(rhs.m_data))
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        , m_hasValue(rhs.m_hasValue)
#endif
    {
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        rhs.m_hasValue = false;
#endif
    }

    // Move‐ctor
    add_fake_copy_constructor(add_fake_copy_constructor&& rhs)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_data(std::move(rhs.m_data))
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        , m_hasValue(rhs.m_hasValue)
#endif
    {
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        rhs.m_hasValue = false;
#endif
    }


    // “Fake” copy‐assign
    void operator=(const add_fake_copy_constructor& rhs)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        m_data = std::move(rhs.m_data);
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        m_hasValue = rhs.m_hasValue;
        rhs.m_hasValue = false;
#endif
    }

    //  Move‐assign
    void operator=(add_fake_copy_constructor&& rhs)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (this != &rhs) {
            m_data = std::move(rhs.m_data); // value or zombie, take it anyways
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
            m_hasValue = rhs.m_hasValue;
            rhs.m_hasValue = false; // make sure the rhs is known as a zombie
#endif
        }
    }

    // Take out the value (non‐const)
    T take() const noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        assert(m_hasValue);// throw std::runtime_error("Object not initialized");
#if defined(MG_DEBUG_FAKE_COPY_CONSTRUCTOR)
        m_hasValue = false; // make sure the object is not used again
#endif
        return std::move(m_data);
    }

    // value‐conversion
    operator T() const noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        return take();   // reuse your non‐const take()
    }
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
