// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SET_CACHE_H)
#define __SET_CACHE_H

#include "set/QuickContainers.h"
#include "utl/Environment.h"

#include <functional>
#include <atomic>

/****************** struct Cache                  *******************/
template <typename Arg, typename Res>
struct duplicate_arg {
	const Arg& operator()(const Arg& arg, const Res&) const { return arg; }
};

struct duplicate_ref {
	const auto & operator()(const auto& ref) const { return ref; }
};

struct duplicate_nozombies {
	LispRef operator()(LispObj* ptr) const 
	{
		return LispRef(LispPtr(ptr), no_zombies{});
	}
};

template<
	typename Func,
	typename ArgOrder = std::less<typename Func::argument_type>,
	typename argument_reftype = param_type_t < typename Func::argument_type>,
	typename result_reftype = typename Func::result_reftype,
	typename KeyDuplFunc = duplicate_arg<typename Func::argument_type, typename Func::result_reftype>, 
	typename Ref2ResFunc = std::conditional_t<std::is_same_v<typename Func::result_type, result_reftype>, duplicate_ref, duplicate_nozombies>
>
struct Cache
{
	using function = Func;
	using argument_type = typename function::argument_type;
	using result_type = typename function::result_type;
	
	using map_type = std::map<argument_type, result_reftype, ArgOrder>;

	Cache() { MG_DEBUGCODE( md_NrCalls = md_NrMisses = 0; ) }

	result_type apply(argument_reftype arg)
	{
		auto cacheLock = std::lock_guard(mx_MapLock);

		MG_DEBUGCODE(md_NrCalls++; )
		dms_check_not_debugonly; 
		auto i = m_Map.lower_bound(arg);
		while (i != m_Map.end() && !m_Comp(arg, i->first))
		{
			auto sharedRef = m_Ref2ResFunc(i->second);
			if (sharedRef)
				return sharedRef;
			++i;
		}
		result_reftype res = m_Func(arg);
		i = m_Map.insert(i, typename map_type::value_type(m_KeyDupl(arg, res), res));
		MG_DEBUGCODE(md_NrMisses++; )
		return res;
	}

	SizeT size () const { return m_Map.size (); }
	bool  empty() const { return m_Map.empty(); }

	void remove(argument_reftype arg)
	{
		auto cacheLock = std::lock_guard(mx_MapLock);
		auto i = m_Map.lower_bound(arg);
		assert(i != m_Map.end());
		while (i->second->IsOwned())
		{
			++i;
			assert(i != m_Map.end());
		}
		m_Map.erase(i);
	}

private:
	MG_DEBUGCODE(SizeT md_NrCalls; )
	MG_DEBUGCODE(SizeT md_NrMisses; )

	Func         m_Func;
	ArgOrder     m_Comp;
	KeyDuplFunc  m_KeyDupl;
	Ref2ResFunc  m_Ref2ResFunc;
	map_type     m_Map;
	std::recursive_mutex   mx_MapLock;
};


/****************** struct StaticCache                  *******************/
/*
template<
	typename Func, 
	typename ArgOrder = std::less<typename Func::argument_type>,
	typename DuplFunc = duplicate_arg<typename Func::argument_type, typename Func::result_type> 
>
struct StaticCache
{
	using cache_type    = Cache<Func, ArgOrder, DuplFunc> ;
	using argument_type = typename cache_type::argument_type       ;
	using result_type   = typename cache_type::result_type         ;

	StaticCache()
		: m_PtrSection(item_level_type(0), ord_level_type::LispObjCache, "LispObjCache")
	{}

	result_type apply(const argument_type& arg)
	{
		leveled_std_section::scoped_lock lock(m_PtrSection);
		return m_Cache.apply(arg);
	}

	SizeT size() const { return m_Cache.size(); } // only defined if !IsNull()

	void remove(const argument_type& arg)           // only defined if !IsNull()
	{
		leveled_std_section::scoped_lock lock(m_PtrSection);

		m_Cache->remove(arg);
		if (m_Cache->empty())
			m_Cache.reset();
	}
	void clear() { m_Cache.reset(); }

private:
	cache_type m_Cache;
	leveled_std_section m_PtrSection;
};
*/
#endif // !defined(__SET_CACHE_H)
