// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_UTL_SCOPED_EXIT_H
#define __RTC_UTL_SCOPED_EXIT_H

#include "RtcBase.h"
#include <optional>

template <typename Func>
struct scoped_exit {
	scoped_exit(Func&& f) : m_Func(std::move(f)) {}
	~scoped_exit() { m_Func(); }

	scoped_exit(const scoped_exit&) = delete;
	scoped_exit(scoped_exit&&) = delete;
	void operator = (const scoped_exit&) = delete;
	void operator = (scoped_exit&&) = delete;
private:
	Func m_Func;
};

template <typename Func>
struct movable_scoped_exit {
	movable_scoped_exit() 
	{}
	movable_scoped_exit(Func&& f) noexcept(std::is_nothrow_move_constructible_v<Func>)
		: m_Func(std::move(f)) 
	{}

	~movable_scoped_exit() { if (m_Func) (*m_Func)(); }

	movable_scoped_exit(movable_scoped_exit&& rhs)
	:	m_Func(std::move(rhs.m_Func))
	{
		rhs.m_Func.reset();
	}
	movable_scoped_exit(const movable_scoped_exit&) = delete;
	void operator = (const movable_scoped_exit&) = delete;
	void operator = (movable_scoped_exit&&) = delete;

protected:
	std::optional<Func> m_Func;
};

template <typename Func>
struct releasable_scoped_exit : movable_scoped_exit<Func>
{
	using movable_scoped_exit<Func>::movable_scoped_exit;

	void release() { this->m_Func.reset(); }
};


template <typename Func>
auto make_scoped_exit(Func&& f) { return scoped_exit<std::decay_t<Func>>(std::forward<Func>(f)); }

template <typename Func>
auto make_movable_scoped_exit(Func&& f) { return movable_scoped_exit<std::decay_t<Func>>(std::forward<Func>(f)); }

template <typename Func>
auto make_releasable_scoped_exit(Func&& f) { return releasable_scoped_exit<Func>(std::forward<Func>(f)); }

//template <typename Func>
//std::any make_any_scoped_exit(Func&& f) { return make_noncopyable_any<movable_scoped_exit<Func>>(std::forward<Func>(f)); }

template <typename Func>
auto make_shared_exit(Func&& f) { return std::make_shared<scoped_exit<Func>>(std::forward<Func>(f)); }

template <typename Func>
auto make_shared_releasable_exit(Func&& f) { return std::make_shared<movable_scoped_exit<Func>>(std::forward<Func>(f)); }


#endif // __RTC_UTL_SCOPED_EXIT_H