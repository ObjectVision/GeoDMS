// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_DMS_COMBINABLE_H)
#define __RTC_DMS_COMBINABLE_H

#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

// Portable replacement for Concurrency::combinable<T>.
// Each thread gets its own default-constructed T via local().
// combine_each(func) iterates over all thread-local values.

template<typename T>
class dms_combinable {
	std::mutex m_mutex;
	std::unordered_map<std::thread::id, T> m_locals;

public:
	T& local() {
		auto id = std::this_thread::get_id();
		std::lock_guard lock(m_mutex);
		return m_locals[id];
	}

	template<typename F>
	void combine_each(F func) {
		for (auto& [id, val] : m_locals)
			func(val);
	}

	void clear() {
		m_locals.clear();
	}
};

#endif // __RTC_DMS_COMBINABLE_H
