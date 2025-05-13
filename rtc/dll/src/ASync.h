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
struct add_fake_copy_constructor
{
	mutable T m_data;
	mutable bool m_hasValue;
/*
	template <typename... Args>
	add_fake_copy_constructor(Args&&... args)
		: T(std::forward<Args>(args)...)
	{}
	*/
	add_fake_copy_constructor()
		: m_data(T()), m_hasValue(false)
	{}

	add_fake_copy_constructor(T&& org)
		: m_data(std::move(org)), m_hasValue(true)
	{}

	/*
		add_fake_copy_constructor(const T& rhs)
			: T(std::move(const_cast<T&>(rhs))), m_hasValue(true)
		{}
	*/

	add_fake_copy_constructor(const add_fake_copy_constructor<T>& rhs)
		: add_fake_copy_constructor<T>(rhs.get())
	{
	}

	//	add_fake_copy_constructor(const add_fake_copy_constructor<T>& rhs)
	//		: add_fake_copy_constructor<T>(std::move<add_fake_copy_constructor<T>>(const_cast<add_fake_copy_constructor<T>&>(rhs)))
	//	{}

	add_fake_copy_constructor(add_fake_copy_constructor<T>&& rhs)
		: add_fake_copy_constructor<T>(rhs.get())
	{
	}

	void operator =(const add_fake_copy_constructor<T>& rhs)
	{
		m_data = std::move(rhs.m_data); // value or zombie, take it anyways
		m_hasValue = rhs.m_hasValue;
		rhs.m_hasValue = false; // make sure the rhs is known as a zombie
	}
	void operator =(add_fake_copy_constructor<T>&& rhs)
	{
		m_data = std::move(rhs.m_data); // value or zombie, take it anyways
		m_hasValue = rhs.m_hasValue;
		rhs.m_hasValue = false; // make sure the rhs is known as a zombie
	}

	T get() const
	{
		if (!m_hasValue)
			throw std::runtime_error("Fake copy constructor: object not initialized");
		m_hasValue = false; // make sure the object is not used again
		return std::move(m_data);
	}

	operator T () const
	{ 
		return get();
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
