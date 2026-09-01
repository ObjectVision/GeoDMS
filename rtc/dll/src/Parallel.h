#pragma once

#if !defined(__RTC_PARALLEL_H)
#define __RTC_PARALLEL_H

#include "dbg/Diagnostics.h"
#include "parallel/portable_task_group.h"

#include <thread>
#include <mutex>
#include <shared_mutex>

#define THREAD_LOCAL thread_local

RTC_CALL bool IsMultiThreaded0(); /// RSF_SuspendForGUI
RTC_CALL bool IsMultiThreaded1();
RTC_CALL bool IsMultiThreaded2();
RTC_CALL bool IsMultiThreaded3();
bool IsMultiThreaded1or2();
UInt32 GetNrVCPUs();
RTC_CALL UInt32 MaxConcurrentTreads();
RTC_CALL UInt32 MaxAllowedConcurrentTreads(); // make it constant to avoid rounding off errors to depend on architecture or settings.

//#define MG_ITEMLEVEL
enum class item_level_type : UInt32 {};
enum class ord_level_type : UInt32;

#if defined(MG_DEBUG)
#define MG_DEBUG_LOCKLEVEL
#endif

#if defined(MG_DEBUG_LOCKLEVEL)
#define MG_ITEMLEVEL

// LevelCheckBlocker is gone (#1233 P3). It existed for one site, InterestReporter::Report, which
// nested two sections that shared an ordinal; giving UpdatingInterestSet its own level made the
// order checkable and left the blocker unused. It is not replaced: an escape hatch that switches
// the ordering check off wholesale means the checker cannot see anything a blocked scope does,
// which is the opposite of what it is for. A section that genuinely may be taken in either order
// needs an ordinal that says so, not a way to stop asking.

struct level_type {
	CharPtr         m_Descr = nullptr;
	ord_level_type  m_Level = ord_level_type(0);
	item_level_type m_ItemLevel = item_level_type(0);
	bool        m_Shared= false;
	level_type* m_Prev = nullptr;

	bool Allow(const level_type& other) const {
		dms_assert(other.m_Level > ord_level_type(0));
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

// True when this thread holds no leveled section and has declared no ceiling. This is the
// complement of a callee ceiling: a dispatch point whose callees are deliberately UNCONSTRAINED
// (the main-thread operation queue) is only sound while the dispatcher itself holds nothing,
// and this is what lets it assert that (#1227).
RTC_CALL bool CurrentThreadHoldsNoLevelLock() noexcept;

//----------------------------------------------------------------------
// Declared lock-level ceilings (#1227)
//----------------------------------------------------------------------
//
// level_type above records what a thread ACTUALLY holds. A ceiling is that same thing DECLARED
// instead of taken: "for the rest of this scope, treat me as holding this level". It is therefore
// not a second bookkeeping -- DmsLockCeiling enters and leaves the very thread-local level that
// every leveled_section acquire enters, through the same EnterLevel / LeaveLevel, and it is
// checked by the same level_type::Allow. Whatever a real acquire would reject while that level is
// held, the declaration rejects too, and there is one ordering rule rather than two to keep in
// step.
//
// What the declaration buys is a check at a DISPATCH BOUNDARY. Virtual Object / Actor methods, the
// operator registry, std::function callbacks (PostMainThreadOper) and eight DLL boundaries make a
// static call graph degenerate to "anything reaches anything", so there is no whole-program
// analysis to be had here. A declaration turns that whole-program question into a modular one:
// what a scope promises is checked against what its callees actually take, however far apart the
// two are written and without either side knowing anything about the other. Clang's Thread Safety
// Analysis is the static equivalent; neither the pinned MSVC v145 nor GCC implements it, and
// checking the promise against what really happens needs no new compiler.
//
// Declare only what a scope does NOT itself take. A function that takes the lock already publishes
// its level through scoped_lock_impl, so declaring it again says nothing -- and declaring it
// exclusively would make Allow reject that function's own acquire (equal level, not both shared).
// The declaration is for the scope that promises a bound it never acquires: "everything from here
// on stays at the token registry or inner", so that a call which registers a token fails.
//
// Two things it deliberately does not do:
//  - It is SCOPE-shaped. The killer case of #1227, reportF("{}", item->GetNameLock()), is an
//    unnamed argument temporary whose LIFETIME -- not whose scope -- spans the call, and no
//    annotation on either side states that. The exact per-thread usage count of sym/Token.h covers
//    that one, unconditionally and in Release.
//  - It inherits Allow's item-level short-circuit. A ceiling is declared at item level 0, so a
//    per-item lock taken in between (item level >= 1, via Actor::DetermineLastSupplierChange) makes
//    Allow return true for everything after it. That blind spot is Allow's own, is recorded as such
//    in #1227, and is not worked around here.
struct DmsLockCeiling
{
	DmsLockCeiling(ord_level_type level, bool isShared, CharPtr descr) noexcept
		: m_OldLevel(EnterLevel(level_type{ descr, level, item_level_type(0), isShared, &m_OldLevel }))
	{}
	~DmsLockCeiling() { LeaveLevel(m_OldLevel); }

	DmsLockCeiling(const DmsLockCeiling&) = delete;
	DmsLockCeiling& operator =(const DmsLockCeiling&) = delete;

private:
	level_type m_OldLevel;
};

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
	void lock();
	RTC_CALL void lock_shared();
	void unlock();
	RTC_CALL void unlock_shared();

	// Exclusive acquire that gives up: false means the shared usages did not drop to zero within
	// the given time and nothing was acquired. Used by the teardown drain, which must not be able
	// to park forever (#1191).
	RTC_CALL bool try_lock_for(UInt32 milliSeconds);

	bool try_lock_shared();
	bool is_unique_locked() const { return m_Count < 0;  }

	// Outstanding shared usages; 0 when free or exclusively locked. Diagnostic only: the value can
	// change the moment it is read.
	int shared_use_count() const { return m_Count > 0 ? m_Count : 0; }

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
