// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_MEM_LOCKEDSEQUENCEOBJ_H)
#define __RTC_MEM_LOCKEDSEQUENCEOBJ_H

#include "mem/HeapSequenceProvider.h"

template <typename V>
struct locked_sequence : sequence_traits<V>::polymorph_vec_t
{
	using base_type = typename sequence_traits<V>::polymorph_vec_t;
	using const_iterator = typename base_type::const_iterator;

	locked_sequence(bool mustClear = true)
		: base_type( heap_sequence_provider<typename elem_of<V>::type>::CreateProvider() )
	{
		this->Lock(mustClear ? dms_rw_mode::write_only_mustzero : dms_rw_mode::write_only_all);
	}
	locked_sequence(SizeT sz MG_DEBUG_ALLOCATOR_SRC_ARG)
		: locked_sequence<V>(true)
	{
		this->resizeSO(sz, true MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}

	locked_sequence(SizeT sz, const V& v MG_DEBUG_ALLOCATOR_SRC_ARG)
		: locked_sequence<V>(false)
	{
		this->resizeSO(sz, false MG_DEBUG_ALLOCATOR_SRC_PARAM);
		fast_fill(this->begin(), this->end(), v);
	}

	locked_sequence(IterRange<const_iterator> range MG_DEBUG_ALLOCATOR_SRC_ARG)
		: locked_sequence<V>(false)
	{
		this->append(range.first, range.second MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}

	locked_sequence(const_iterator first, const_iterator last MG_DEBUG_ALLOCATOR_SRC_ARG)
		: locked_sequence<V>(false)
	{
		this->append(first, last MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}

	locked_sequence(const locked_sequence<V>& rhs MG_DEBUG_ALLOCATOR_SRC_ARG)
		: locked_sequence<V>(rhs.begin(), rhs.end() MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{}

	locked_sequence(locked_sequence<V>&& other)
		: locked_sequence<V>(false)
	{
		this->swap(other);
	}

	locked_sequence<V>& operator=(const locked_sequence<V>& other)
	{
		auto ohterCopy = locked_sequence<V>(other.begin(), other.end());
		this->swap(ohterCopy);
		return *this;
	}
	locked_sequence<V>& operator=(locked_sequence<V>&& other) noexcept
	{
		this->swap(other);
		return *this;
	}

	~locked_sequence()
	{
		this->UnLock();
	}
};


#endif // __RTC_MEM_LOCKEDSEQUENCEOBJ_H
