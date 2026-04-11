// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

// Portable fire-and-forget async task, replacing concurrency::task<void> from ppltasks.h.
// Usage: dms_task t(callable);  // runs callable asynchronously
// The task runs detached; the dms_task object does not block on destruction.

#include <future>
#include <utility>

class dms_task {
	std::future<void> m_future;
public:
	dms_task() = default;

	template <typename F>
	explicit dms_task(F&& f)
		: m_future(std::async(std::launch::async, std::forward<F>(f)))
	{}

	dms_task(dms_task&&) = default;
	dms_task& operator=(dms_task&&) = default;

	dms_task(const dms_task&) = delete;
	dms_task& operator=(const dms_task&) = delete;
};
