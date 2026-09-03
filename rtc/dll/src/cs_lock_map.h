// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_CS_LOCK_MAP_H
#define __RTC_CS_LOCK_MAP_H

#include <optional>
#include "dbg/debug.h"

#include "LockLevels.h"

#if defined(MG_DEBUG)
	const bool MG_DEBUG_LOCKS = false;
#else
	const bool MG_DEBUG_LOCKS = false;
#endif

//=============================== ConcurrentMap (client is responsible for scoping and stack unwinding issues)

template <typename lock_key>
struct cs_lock_map
{
	using lock_type = leveled_std_section;
	using cs_imp = lock_type;

	struct lock_value 
	{ 
		lock_value(MG_SOURCE_INFO_DECL item_level_type itemLevel, ord_level_type level, CharPtr descr)
			:	m_Lock(itemLevel, level, descr)
#if defined(MG_DEBUG_LOCKLEVEL)
			,   md_LockNr(++cs_lock_map::md_LastLockNr)
#endif
#if	defined(MG_DEBUG_SOURCE_INFO)
			,	m_SrcInfo(srcInfo)
#endif
		{}
		~lock_value() { dms_assert(m_Counter == 0); }

#if defined(MG_DEBUG_LOCKLEVEL)
		UInt32 md_LockNr;
#endif
#if	defined(MG_DEBUG_SOURCE_INFO)
		CharPtr m_SrcInfo;
#endif
		cs_imp m_Lock;
		UInt32 m_Counter = 0;

		lock_value(lock_value&& oth) noexcept // func is called from insert(ptr, pair(key, lock_value)
			:	m_Lock(std::move(oth.m_Lock))
#if defined(MG_DEBUG_LOCKLEVEL)
			,	md_LockNr(oth.md_LockNr+1000000)
#endif
#if	defined(MG_DEBUG_SOURCE_INFO)
			, m_SrcInfo(oth.m_SrcInfo)
#endif

		{
			dms_assert(oth.m_Counter == 0); // only unlocked sections may be 'copied' or moved
		}
		lock_value(const lock_value& oth) = delete;// func is called from insert(ptr, pair(key, lock_value)			
		void operator = (const lock_value& oth) = delete;
	};

	using assoc_container = std::map<lock_key, lock_value, std::less<void>>;
	using assoc_ptr = typename assoc_container::iterator;

	assoc_container     scm_TileAccessLocks;
	leveled_std_section sc_TileAccessMapLock;

#if defined(MG_DEBUG_LOCKLEVEL)
	ord_level_type m_LockLevel;
	CharPtr m_Descr;
	inline static UInt32 md_LastLockNr = 0;
#else
	static const ord_level_type m_LockLevel = ord_level_type(0);
	inline static const CharPtr m_Descr = "";
#endif

	cs_lock_map(CharPtr descr, [[maybe_unused]] ord_level_type lockLevel = ord_level_type::ItemRegister)
		:	sc_TileAccessMapLock(item_level_type(0), ord_level_type::TileAccessMap, "cs_lock_map")
#if defined(MG_DEBUG_LOCKLEVEL)
		,	m_LockLevel(lockLevel)
		,	m_Descr(descr)
#endif
	{}

#if defined(MG_DEBUG_LOCKLEVEL)
	// #1233: a per-item lock enters the level checker exactly as scoped_lock_impl does. Until now
	// this map called m_Lock.lock() on the std::mutex directly and its locks were invisible to the
	// checker -- which also meant the item-level dimension of Allow had never been exercised by
	// anything at all. What is checked: a global section or a ceiling held while a per-item lock
	// is taken (refused), and anything taken under a per-item lock. Two per-item locks are not
	// ordered against each other; see Allow in Parallel.h for why that would only be false.
	struct level_entry
	{
		level_entry(item_level_type itemLevel, ord_level_type level, CharPtr descr)
			:	m_OldLevel(EnterLevel(level_type{ descr, level, itemLevel, false, &m_OldLevel }))
		{}
		~level_entry() { LeaveLevel(m_OldLevel); }
		level_entry(const level_entry&) = delete;
		level_entry& operator =(const level_entry&) = delete;

		level_type m_OldLevel;
	};
#endif

	template <typename KeyProxy>
	assoc_ptr GetorCreateMutex(MG_SOURCE_INFO_DECL KeyProxy&& key)
	{
		dms_assert(key);
		leveled_std_section::scoped_lock mapLock(sc_TileAccessMapLock); // TEST, Unlock if scm_TileAccessLocks[key].try_lock() fails to let other threads access the map to prevent deadlock.

		assoc_ptr ptr = scm_TileAccessLocks.lower_bound(key);
		if (ptr == scm_TileAccessLocks.end() || ptr->first != key)
		{
			using value_type = typename assoc_container::value_type;
			ptr = scm_TileAccessLocks.insert(ptr, value_type(key, lock_value(MG_SOURCE_INFO_USE GetItemLevel(key), m_LockLevel, m_Descr)));
			dms_assert(ptr->second.m_Counter == 0); // no other key active on newly created lock.
		}
		++(ptr->second.m_Counter);
		dms_assert(ptr->second.m_Counter != 0);
		return ptr;
	}

	void ReleaseMutexRef(assoc_ptr ptr)
	{
		leveled_std_section::scoped_lock mapLock(sc_TileAccessMapLock);

		dms_assert(ptr != scm_TileAccessLocks.end());

		dms_assert(ptr->second.m_Counter != 0);
		if (!--(ptr->second.m_Counter))
		{
			scm_TileAccessLocks.erase(ptr); // does require that m_Lock is unlocked.
		}
	}

	void UnLockAndReleaseMutexRef(assoc_ptr ptr)
	{
		dms_assert(ptr != scm_TileAccessLocks.end());
		ptr->second.m_Lock.unlock();
		dms_assert(ptr->second.m_Counter != 0);

		ReleaseMutexRef(ptr);
	}

	template <typename KeyProxy, typename ...Args>
	assoc_ptr Lock(MG_SOURCE_INFO_DECL KeyProxy&& key, Args&&... args)
	{
		assoc_ptr ptr = GetorCreateMutex(MG_SOURCE_INFO_USE std::forward<KeyProxy>(key));
		try {
			ptr->second.m_Lock.lock(std::forward<Args>(args)...);
		}
		catch (...)
		{
			ReleaseMutexRef(ptr);
			throw;
		}
		return ptr;
	}

	template <typename KeyProxy, typename ...Args>
	std::optional<assoc_ptr >TryLock(MG_SOURCE_INFO_DECL KeyProxy&& key, Args&&... args)
	{
		assoc_ptr ptr = GetorCreateMutex(MG_SOURCE_INFO_USE std::forward<KeyProxy>(key));
		try {
			if (!ptr->second.m_Lock.try_lock(std::forward<Args>(args)...))
				return {};
		}
		catch (...)
		{
			ReleaseMutexRef(ptr);
			throw;
		}
		return ptr;
	}

	template <typename KeyProxy, typename Func, typename Result>
	Result GetItemLockProp(KeyProxy&& key, Func&& func, Result&& defaultValue)
	{
		leveled_std_section::scoped_lock mapLock(sc_TileAccessMapLock);

		auto ptr = scm_TileAccessLocks.lower_bound(key);
		if (ptr == scm_TileAccessLocks.end() || key < ptr->first)
			return defaultValue;
		dms_assert(ptr->first == key);

		return func(ptr->second);
	}

	template <typename KeyProxy>
	bool IsLocked(KeyProxy&& key)
	{
		return GetItemLockProp(key,
			[](lock_value& lv)
			{
				return lv.m_Lock.isLocked();
			}
			, false
		);
	}

	struct ScopedLock
	{
		template <typename KeyProxy>
		ScopedLock(MG_SOURCE_INFO_DECL cs_lock_map& map, KeyProxy&& key)
			:	m_Map(&map)
		{
#if defined(MG_DEBUG_LOCKLEVEL)
			// Entered BEFORE the acquire, like scoped_lock_impl: an ordering violation is reported
			// before this thread can park on it. Only a determined item has a level; a passor or a
			// not-yet-determined item reports 0 and stays invisible, as it always was.
			if (auto itemLevel = GetItemLevel(key); itemLevel != item_level_type(0))
				m_LevelEntry.emplace(itemLevel, map.m_LockLevel, map.m_Descr);
#endif
			m_AssocPtr = map.Lock(MG_SOURCE_INFO_USE std::forward<KeyProxy>(key));
			dms_assert(m_AssocPtr != m_Map->scm_TileAccessLocks.end()); // _ITERATOR_DEBUG_LEVEL == 2: also checks that it belongs to map
		}

		~ScopedLock()
		{
			dms_assert(m_AssocPtr != m_Map->scm_TileAccessLocks.end()); // _ITERATOR_DEBUG_LEVEL == 2: also checks that it belongs to map
			m_Map->UnLockAndReleaseMutexRef(m_AssocPtr); // the level is left afterwards, when m_LevelEntry is destroyed
		}

	protected:
#if defined(MG_DEBUG_LOCKLEVEL)
		std::optional<level_entry> m_LevelEntry;
#endif
		assoc_ptr            m_AssocPtr;
		WeakPtr<cs_lock_map> m_Map;
	};
	struct ScopedTryLock
	{
		template <typename KeyProxy>
		ScopedTryLock(MG_SOURCE_INFO_DECL cs_lock_map& map, KeyProxy&& key)
			: m_Map(&map)
		{
#if defined(MG_DEBUG_LOCKLEVEL)
			if (auto itemLevel = GetItemLevel(key); itemLevel != item_level_type(0))
				m_LevelEntry.emplace(itemLevel, map.m_LockLevel, map.m_Descr);
#endif
			m_AssocPtr = map.TryLock(MG_SOURCE_INFO_USE std::forward<KeyProxy>(key));
#if defined(MG_DEBUG_LOCKLEVEL)
			if (!m_AssocPtr)
				m_LevelEntry.reset(); // nothing acquired, so nothing is held
#endif
		}

		~ScopedTryLock()
		{
			if (has_lock())
				m_Map->UnLockAndReleaseMutexRef(*m_AssocPtr);
		}
		bool has_lock() const { return m_AssocPtr.has_value(); }
		operator bool() const { return has_lock(); }

	protected:
#if defined(MG_DEBUG_LOCKLEVEL)
		std::optional<level_entry> m_LevelEntry;
#endif
		assoc_ptr m_A2;
		std::optional<assoc_ptr> m_AssocPtr;
		WeakPtr<cs_lock_map>     m_Map;
	};
};


#endif // __RTC_CS_LOCK_MAP_H
