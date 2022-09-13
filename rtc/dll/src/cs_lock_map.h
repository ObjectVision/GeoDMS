//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#ifndef __RTC_CS_LOCK_MAP_H
#define __RTC_CS_LOCK_MAP_H

#include <concrt.h>

#include <optional>
#include "dbg/debug.h"
#include "utl/Environment.h"

#include "set/QuickContainers.h"

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

	cs_lock_map(CharPtr descr)
		:	sc_TileAccessMapLock(item_level_type(0), ord_level_type::TileAccessMap, "cs_lock_map")
#if defined(MG_DEBUG_LOCKLEVEL)
		,	m_LockLevel(ord_level_type::ItemRegister)
		,	m_Descr(descr)
#endif
	{}

	template <typename KeyProxy>
	assoc_ptr GetorCreateMutex(MG_SOURCE_INFO_DECL KeyProxy&& key)
	{
		dms_assert(key);
		leveled_std_section::scoped_lock mapLock(sc_TileAccessMapLock); // TEST, Unlock if scm_TileAccessLocks[key].try_lock() fails to let other threads access the map to prevent deadlock.

		assoc_ptr ptr = scm_TileAccessLocks.lower_bound(key);
		if (ptr == scm_TileAccessLocks.end() || ptr->first != key)
		{
			ptr = scm_TileAccessLocks.insert(ptr, assoc_container::value_type(key, lock_value(MG_SOURCE_INFO_USE GetItemLevel(key), m_LockLevel, m_Descr)));
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
			:	m_AssocPtr(map.Lock(MG_SOURCE_INFO_USE std::forward<KeyProxy>(key)))
			,	m_Map(&map)
		{
			dms_assert(m_AssocPtr != m_Map->scm_TileAccessLocks.end()); // _ITERATOR_DEBUG_LEVEL == 2: also checks that it belongs to map
		}

		~ScopedLock()
		{
			dms_assert(m_AssocPtr != m_Map->scm_TileAccessLocks.end()); // _ITERATOR_DEBUG_LEVEL == 2: also checks that it belongs to map
			m_Map->UnLockAndReleaseMutexRef(m_AssocPtr);
		}

	protected:
		assoc_ptr            m_AssocPtr;
		WeakPtr<cs_lock_map> m_Map;
	};
	struct ScopedTryLock
	{
		template <typename KeyProxy>
		ScopedTryLock(MG_SOURCE_INFO_DECL cs_lock_map& map, KeyProxy&& key)
			: m_AssocPtr(map.TryLock(MG_SOURCE_INFO_USE std::forward<KeyProxy>(key)))
			, m_Map(&map)
		{}

		~ScopedTryLock()
		{
			if (has_lock())
				m_Map->UnLockAndReleaseMutexRef(*m_AssocPtr);
		}
		bool has_lock() const { return m_AssocPtr.has_value(); }
		operator bool() const { return has_lock(); }

	protected:
		assoc_ptr m_A2;
		std::optional<assoc_ptr> m_AssocPtr;
		WeakPtr<cs_lock_map>     m_Map;
	};
};


#endif // __RTC_CS_LOCK_MAP_H
