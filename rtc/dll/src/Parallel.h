#pragma once

#if !defined(__RTC_PARALLEL_H)
#define __RTC_PARALLEL_H

#include "dbg/Check.h"

#include <concrt.h>
#include <ppl.h>
#include <thread>
#include <mutex>
#include <shared_mutex>

#define THREAD_LOCAL thread_local

RTC_CALL bool IsMultiThreaded0(); /// RSF_SuspendForGUI
RTC_CALL bool IsMultiThreaded1();
RTC_CALL bool IsMultiThreaded2();
RTC_CALL bool IsMultiThreaded3();

//#define MG_ITEMLEVEL
enum class item_level_type : UInt32 {};
enum class ord_level_type : UInt32;

#if defined(MG_DEBUG)
#define MG_DEBUG_LOCKLEVEL
#endif

#if defined(MG_DEBUG_LOCKLEVEL)
#define MG_ITEMLEVEL

struct LevelCheckBlocker {
	RTC_CALL LevelCheckBlocker();
	RTC_CALL ~LevelCheckBlocker();
	RTC_CALL static bool HasLevelCheckBlocked();
};

struct level_type {
	CharPtr         m_Descr = nullptr;
	ord_level_type  m_Level = ord_level_type(0);
	item_level_type m_ItemLevel = item_level_type(0);
	bool        m_Shared= false;
	level_type* m_Prev = nullptr;

	bool Allow(const level_type& other) const {
		dms_assert(other.m_Level > ord_level_type(0));
		if (LevelCheckBlocker::HasLevelCheckBlocked())
			return true;
		if (m_Level == ord_level_type(0))
			return true;
		if (m_ItemLevel > other.m_ItemLevel)
			return true;
		if (m_ItemLevel < other.m_ItemLevel)
			return false;
		if (m_Level < other.m_Level)
			return true;
		if (m_Level > other.m_Level)
			return false;
		return m_Shared && other.m_Shared;
	}
	bool Shared(const level_type& other) const {
		if (m_ItemLevel < other.m_ItemLevel)
			return false;
		if (m_ItemLevel > other.m_ItemLevel)
			return false;
		if (m_Level < other.m_Level)
			return false;
		if (m_Level > other.m_Level)
			return false;
		return m_Shared && other.m_Shared;
	}
};

RTC_CALL level_type EnterLevel(level_type level);
RTC_CALL void LeaveLevel(level_type& oldLevel);

#endif

template <typename base_type>
struct leveled_section : base_type
{
	leveled_section(item_level_type itemLevel, ord_level_type level, CharPtr descr) noexcept
#if defined(MG_DEBUG_LOCKLEVEL)
		: m_ItemLevel(itemLevel)
		, m_Level(level)
		, m_Descr(descr)
#endif
	{}

	leveled_section(leveled_section&& oth) noexcept
#if defined(MG_DEBUG_LOCKLEVEL)
		: m_ItemLevel(oth.m_ItemLevel)
		, m_Level(oth.m_Level)
		, m_Descr(oth.m_Descr)
#endif
	{
#if defined(MG_DEBUG_LOCKLEVEL)
		bool wasUnlocked = oth.try_lock();
		dms_assert(wasUnlocked);
		oth.unlock();
#endif
	}

	bool isLocked()
	{
		if (!this->try_lock())
			return true;
		this->unlock();
		return false;
	}

#if defined(MG_DEBUG_LOCKLEVEL)
	item_level_type m_ItemLevel;
	ord_level_type m_Level;
	CharPtr        m_Descr;
#endif

};



template<typename mutex_type, typename base_lock>
struct scoped_lock_impl
{
	scoped_lock_impl(leveled_section<mutex_type>& cs)
		:
#if defined(MG_DEBUG_LOCKLEVEL)
		m_OldLevel(EnterLevel(level_type{ cs.m_Descr, cs.m_Level, cs.m_ItemLevel, false, &m_OldLevel })),
#endif
		m_BaseLock(cs)
	{}

	~scoped_lock_impl()
	{
#if defined(MG_DEBUG_LOCKLEVEL)
		LeaveLevel(m_OldLevel);
#endif
	}

#if defined(MG_DEBUG_LOCKLEVEL)
private:
	level_type m_OldLevel;
public:
#endif
	base_lock m_BaseLock;
};

template<typename mutex_type>
struct shared_lock_impl
{
	shared_lock_impl() noexcept
		: m_CS(nullptr)
	{}

	/// <summary>
	///     Constructs a <c>scoped_lock_read</c> object and acquires the <c>reader_writer_lock</c> object passed in the
	///     <paramref name="_Reader_writer_lock"/> parameter as a reader. If the lock is held by another thread as a writer or there
	///     are pending writers, this call will block.
	/// </summary>
	/// <param name="_Reader_writer_lock">
	///     The <c>reader_writer_lock</c> object to acquire as a reader.
	/// </param>

	explicit shared_lock_impl(leveled_section<mutex_type>& cs)
		:
#if defined(MG_DEBUG_LOCKLEVEL)
		m_OldLevel(EnterLevel(level_type{ cs.m_Descr, cs.m_Level, cs.m_ItemLevel, true, &m_OldLevel })),
#endif
		m_CS(&cs)
	{
		cs.lock_shared();
	}

	/// <summary>
	///     Destroys a <c>scoped_lock_read</c> object and releases the lock supplied in its constructor.
	/// </summary>

	~shared_lock_impl()
	{
		if (m_CS)
		{
			m_CS->unlock_shared();
#if defined(MG_DEBUG_LOCKLEVEL)
			LeaveLevel(m_OldLevel);
#endif
		}
	}

	shared_lock_impl(shared_lock_impl&& src) noexcept // no copy constructor
		: m_CS(src.m_CS)
	{
		if (m_CS)
		{
#if defined(MG_DEBUG_LOCKLEVEL)
			LeaveLevel(src.m_OldLevel);
			m_OldLevel = EnterLevel(level_type{ m_CS->m_Descr, m_CS->m_Level, m_CS->m_ItemLevel, true, &m_OldLevel });
#endif
			src.m_CS= nullptr;
		}
	}

	shared_lock_impl(const shared_lock_impl& src)
		: m_CS(src.m_CS)
	{
		if (m_CS)
		{
#if defined(MG_DEBUG_LOCKLEVEL)
			m_OldLevel = EnterLevel(level_type{ m_CS->m_Descr, m_CS->m_Level, m_CS->m_ItemLevel,true, &m_OldLevel });
#endif
			m_CS->lock_shared();
		}
	}

	void operator=(shared_lock_impl&& src) noexcept {
#if defined(MG_DEBUG_LOCKLEVEL)
		if (src.m_CS)
			LeaveLevel(src.m_OldLevel);
		if (m_CS)
			LeaveLevel(m_OldLevel);
#endif
		std::swap(m_CS, src.m_CS);
#if defined(MG_DEBUG_LOCKLEVEL)
		if (src.m_CS)
			src.m_OldLevel = EnterLevel(level_type{ src.m_CS->m_Descr, src.m_CS->m_Level, src.m_CS->m_ItemLevel, true, &src.m_OldLevel });
		if (m_CS)
			m_OldLevel = EnterLevel(level_type{ m_CS->m_Descr, m_CS->m_Level, m_CS->m_ItemLevel, true, &m_OldLevel });
#endif
	}
	void operator=(const shared_lock_impl& src) {
		operator = (shared_lock_impl(src));
	}


#if defined(MG_DEBUG_LOCKLEVEL)
	level_type m_OldLevel;
#endif
	leveled_section<mutex_type>* m_CS = nullptr;
};

struct counted_mutex {
	RTC_CALL void lock();
	RTC_CALL void lock_shared();
	RTC_CALL void unlock();
	RTC_CALL void unlock_shared();

	RTC_CALL bool try_lock_shared();
	bool is_unique_locked() const { return m_Count < 0;  }

private:
	int m_Count = 0; // -1 for unique lock, +n for n sharable locks
	MG_DEBUGCODE(dms_thread_id md_OwningThreadID = 0; )
};


struct leveled_std_section : leveled_section< std::mutex >
{
	using leveled_section::leveled_section;
	using scoped_lock = scoped_lock_impl< std::mutex, std::scoped_lock<std::mutex>>;
	using unique_lock = scoped_lock_impl< std::mutex, std::unique_lock<std::mutex>>;
};

struct leveled_shared_section : leveled_section< std::shared_mutex >
{
	using leveled_section::leveled_section;
	using scoped_lock = scoped_lock_impl<std::shared_mutex, std::scoped_lock<std::shared_mutex>>;
	using unique_lock = scoped_lock_impl<std::shared_mutex, std::unique_lock<std::shared_mutex>>;
	using shared_lock = shared_lock_impl<std::shared_mutex>;
};

struct leveled_counted_section : leveled_section< counted_mutex >
{
	using leveled_section::leveled_section;
	using scoped_lock = scoped_lock_impl<counted_mutex, std::scoped_lock<counted_mutex>>;
	using unique_lock = scoped_lock_impl<counted_mutex, std::unique_lock<counted_mutex>>;
	using shared_lock = shared_lock_impl<counted_mutex>;
};

using leveled_critical_section = leveled_std_section;

//==============================================================

//==============================================================

#endif // __RTC_PARALLEL_H
