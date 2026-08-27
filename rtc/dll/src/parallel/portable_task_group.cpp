// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"
#include "parallel/portable_task_group.h"

#include "DbgInterface.h" // DBG_InstallFatalHandlers

#include <cassert>

portable_task_group::portable_task_group(unsigned concurrency)
{
	m_workers.reserve(concurrency);
	for (unsigned i = 0; i < concurrency; ++i)
	{
		m_workers.emplace_back([this]
		{
			// MSVC keeps the terminate handler PER THREAD, so the install done for the main thread does
			// NOT cover these workers: without this line an exception escaping task() below kills the
			// process at ucrtbase!abort (0xC0000409) with no report at all, which is how #1191 presented.
			// This is where such an exception is most likely to originate: a worker abandoned mid-flight
			// by a teardown throws task_canceled the moment it touches an expired TreeItem weak_ptr.
			DBG_InstallFatalHandlers();

			while (true)
			{
				std::function<void()> task;
				{
					std::unique_lock lock(m_mutex);
					m_cv.wait(lock, [this] { return m_stop || m_canceling || !m_tasks.empty(); });

					if ((m_stop || m_canceling) && m_tasks.empty())
						return;
					if (m_canceling)
					{
						// drain remaining tasks without executing
						std::queue<std::function<void()>> empty;
						m_tasks.swap(empty);
						m_idle_cv.notify_all();
						return;
					}
					task = std::move(m_tasks.front());
					m_tasks.pop();
					++m_active;
				}
				try {
					task();
				}
				catch (...)
				{
					// Last boundary before the thread function itself: an escape from here terminates
					// the process (#1191). Each task entry point restores its own bookkeeping before
					// letting anything through, so all that is left to do here is report -- and to keep
					// this worker alive, since killing it would permanently shrink the pool.
					DBG_ReportBoundaryException("portable_task_group worker");
				}

				// Must run on the throwing path too: skipping it leaves m_active above zero forever,
				// and wait() -- which blocks until m_tasks is empty AND m_active == 0 -- never returns.
				{
					std::lock_guard lock(m_mutex);
					--m_active;
				}
				m_idle_cv.notify_all();
			}
		});
	}
}

void portable_task_group::run(std::function<void()> f)
{
	{
		std::lock_guard lock(m_mutex);
		if (m_stop || m_canceling)
			return;
		m_tasks.push(std::move(f));
	}
	m_cv.notify_one();
}

void portable_task_group::cancel()
{
	m_canceling.store(true, std::memory_order_relaxed);
	m_cv.notify_all();
}

void portable_task_group::wait()
{
	std::unique_lock lock(m_mutex);
	m_idle_cv.wait(lock, [this] { return m_tasks.empty() && m_active == 0; });
}

portable_task_group::~portable_task_group()
{
	{
		std::lock_guard lock(m_mutex);
		m_stop = true;
	}
	m_cv.notify_all();
	for (auto& w : m_workers)
		if (w.joinable())
			w.join();
}

// Global singleton management

static portable_task_group* s_thePortableTaskGroup = nullptr;

portable_task_group& GetPortableTaskGroup()
{
	assert(s_thePortableTaskGroup);
	return *s_thePortableTaskGroup;
}

void InitPortableTaskGroup(unsigned concurrency)
{
	assert(!s_thePortableTaskGroup);
	s_thePortableTaskGroup = new portable_task_group(concurrency);
}

void DestroyPortableTaskGroup()
{
	assert(s_thePortableTaskGroup);
	delete s_thePortableTaskGroup;
	s_thePortableTaskGroup = nullptr;
}

[[noreturn]] RTC_CALL void throwTaskCanceled()
{
	throw task_canceled{};
}
