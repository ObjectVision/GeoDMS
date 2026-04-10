// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"
#include "parallel/portable_task_group.h"

#include <cassert>

portable_task_group::portable_task_group(unsigned concurrency)
{
	m_workers.reserve(concurrency);
	for (unsigned i = 0; i < concurrency; ++i)
	{
		m_workers.emplace_back([this]
		{
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
				task();
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
