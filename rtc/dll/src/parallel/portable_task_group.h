// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_PORTABLE_TASK_GROUP_H)
#define __RTC_PORTABLE_TASK_GROUP_H

#include "RtcBase.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#if defined(WIN32)
#include "pplinterface.h"
using task_canceled = Concurrency::task_canceled;
#else
struct task_canceled : std::exception
{};
#endif

class portable_task_group {
	std::vector<std::thread> m_workers;
	std::queue<std::function<void()>> m_tasks;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::condition_variable m_idle_cv;
	std::atomic<bool> m_canceling{false};
	std::atomic<bool> m_stop{false};
	unsigned m_active{0};

public:
	RTC_CALL explicit portable_task_group(unsigned concurrency);
	RTC_CALL void run(std::function<void()> f);
	RTC_CALL void cancel();
	RTC_CALL void wait();
	bool is_canceling() const { return m_canceling.load(std::memory_order_relaxed); }
	RTC_CALL ~portable_task_group();
};

RTC_CALL portable_task_group& GetPortableTaskGroup();
RTC_CALL void InitPortableTaskGroup(unsigned concurrency);
RTC_CALL void DestroyPortableTaskGroup();

#endif // __RTC_PORTABLE_TASK_GROUP_H
