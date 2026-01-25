// Copyright (C) 1998-2025 Object Vision b.v. 
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

#include <unordered_set>

template<typename Func>
struct UnorderedSetCache
{
	using function = Func;
	using argument_type = typename function::argument_type;
	using result_type = typename function::result_type;
	using argument_reftype = param_type_t < typename function::argument_type>;
	using arg_hasher = typename function::hasher;
	using arg_compare = typename function::equality_compare;
	struct equality_compare
	{
		using is_transparent = int;

		bool operator()(argument_type left, argument_type right) const
		{
			return m_ArgComp(left, right);
		}
		bool operator()(argument_type left, result_type rightPtr) const
		{
			assert(rightPtr);
			return m_ArgComp(left, rightPtr->GetKey());
		}
		bool operator()(result_type leftPtr, argument_type right) const
		{
			assert(leftPtr);
			return m_ArgComp(leftPtr->GetKey(), right);
		}
		bool operator()(result_type leftPtr, result_type rightPtr) const
		{
			assert(leftPtr);
			assert(rightPtr);
			return m_ArgComp(leftPtr->GetKey(), rightPtr->GetKey());
		}

		arg_compare m_ArgComp;
	};

	struct hasher
	{
		using is_transparent = int;

		std::size_t operator()(argument_type v) const
		{
			return m_ArgHasher(v);
		}
		std::size_t operator()(result_type ptr) const
		{
			assert(ptr);
			return m_ArgHasher(ptr->GetKey());
		}
		arg_hasher m_ArgHasher;
	};

	using uset_type = std::unordered_set<result_type, hasher, equality_compare>;

	LispRef apply(argument_reftype arg)
	{
		while (true)
		{
			auto cacheLock = std::lock_guard(mx_MapLock);

			MG_DEBUGCODE(md_NrCalls++; )
				dms_check_not_debugonly;
			auto i = m_USet.find(arg);
			if (i != m_USet.end() && m_EqComp(arg, *i))
			{
				auto sharedRef = LispRef(LispPtr(*i), no_zombies{});
				if (sharedRef)
					return sharedRef;
				continue; // retry if the reference is zombie
			}
			result_type res = m_Func(arg);
			m_USet.insert(res);
			MG_DEBUGCODE(md_NrMisses++; )
			return res;
		}
	}

	//	SizeT size () const { return m_Map.size (); }
	bool  empty() const { return m_USet.empty(); }

	void remove(argument_reftype arg)
	{
		auto cacheLock = std::lock_guard(mx_MapLock);

		auto i = m_USet.find(arg);
		assert(i != m_USet.end());
		assert(!(*i)->IsOwned());
		assert(m_EqComp(arg, *i));
		m_USet.erase(i);
	}

private:

#if defined(MG_DEBUG)
	friend struct LispCaches;

	SizeT md_NrCalls = 0;
	SizeT md_NrMisses = 0;
#endif

	hasher           m_Hasher;
	equality_compare m_EqComp;
	Func             m_Func;
	uset_type        m_USet;
	std::mutex       mx_MapLock;
};


template<typename Func>
struct UnorderedMapCache
{
	using function = Func;
	using argument_type = typename function::argument_type;
	using result_type = typename function::result_type;
	using argument_reftype = param_type_t < typename function::argument_type>;
	using arg_hasher = typename function::hasher;
	using arg_compare = typename function::equality_compare;

	using umap_type = std::unordered_map<argument_type, result_type, arg_hasher, arg_compare>;

	UnorderedMapCache() noexcept
		: m_EqComp(), m_Hasher()
	{}

	LispRef apply(argument_reftype arg)
	{
		auto cacheLock = std::lock_guard(mx_MapLock);

		MG_DEBUGCODE(md_NrCalls++; )
			dms_check_not_debugonly;
		auto i = m_UMap.find(arg);
		if (i != m_UMap.end() && m_EqComp(arg, i->first))
			return i->second;
		result_type res = m_Func(arg);
		m_UMap.insert({ std::move(arg), res });
		MG_DEBUGCODE(md_NrMisses++; )
		return res;
	}

	//	SizeT size () const { return m_Map.size (); }
	bool  empty() const { return m_UMap.empty(); }

	void remove(argument_reftype arg)
	{
		auto cacheLock = std::lock_guard(mx_MapLock);

		auto i = m_UMap.find(arg);
		assert(i != m_UMap.end());
		assert(!(*i)->IsOwned());
		assert(m_EqComp(arg, *i));
		m_UMap.erase(i);
	}

private:

#if defined(MG_DEBUG)
	SizeT md_NrCalls = 0;
	SizeT md_NrMisses = 0;
#endif

	arg_hasher  m_Hasher;
	arg_compare m_EqComp;
	Func        m_Func;
	umap_type   m_UMap;
	std::recursive_mutex mx_MapLock;
};


#endif // !defined(__SET_CACHE_H)
