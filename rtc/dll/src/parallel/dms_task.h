// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

// Portable fire-and-forget async task, replacing:
//   using dms_task = concurrency::task<void>;  (from OperationContext.h / ppltasks.h)
//
// Usage: dms_task t(callable);  // runs callable asynchronously
// Like concurrency::task<void>, the destructor does NOT block; the thread is detached.

#include <thread>
#include <utility>

class dms_task {
public:
	dms_task() = default;

	template <typename F>
	explicit dms_task(F&& f)
	{
		std::thread(std::forward<F>(f)).detach();
	}

	dms_task(dms_task&&) = default;
	dms_task& operator=(dms_task&&) = default;

	dms_task(const dms_task&) = delete;
	dms_task& operator=(const dms_task&) = delete;
};
