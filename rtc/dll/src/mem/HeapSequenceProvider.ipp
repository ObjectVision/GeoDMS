// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_HEAPSEQUENCEPROVIDER_IPP)
#define __RTC_MEM_HEAPSEQUENCEPROVIDER_IPP

#include "geo/IndexRange.h"
#include "mem/HeapSequenceProvider.h"
#include "mem/ManagedAllocData.h"

// =================================================== class heap_sequence_provider : public abstr_sequence_provider<V>

template <typename V>
SizeT heap_sequence_provider<V>::max_size()
{
	return managed_alloc_data<V>().max_size();
}

template <typename V>
void heap_sequence_provider<V>::reserve(alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert(seq.m_Capacity >= seq.size());
	if (newSize > seq.m_Capacity)
	{
		auto oldSeq = std::move(seq);
		seq = managed_alloc_data<V>(oldSeq.begin(), oldSeq.end(), newSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
		free(oldSeq); // oldSeq is now copied, so we can safely clear it
	}
}

template <typename V>
void heap_sequence_provider<V>::resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
{ 
	Check(seq);
	if (newSize <= seq.size())
		cut(seq, newSize);
	else
	{
		if (newSize > seq.m_Capacity) // ALLOC || !seq.begin())
			reserve(seq, Max<SizeT>(newSize, 2*seq.m_Capacity) MG_DEBUG_ALLOCATOR_SRC_PARAM);

		assert(newSize <= seq.m_Capacity);

		iterator newEnd = seq.begin() + newSize;
		raw_awake_or_init(seq.end(), newEnd, mustClear);

		SetSize(seq, newSize);
	}
}

template <typename V>
void heap_sequence_provider<V>::cut(alloc_t& seq, SizeT newSize)
{ 
	Check(seq);
	dms_assert(newSize <= seq.size());

	iterator newEnd = seq.begin() + newSize;
	destroy_range(newEnd, seq.end());

	SetSize(seq, newSize);
}

template <typename V>
void heap_sequence_provider<V>::clear(alloc_t& seq) 
{ 
	Check(seq); 
	managed_alloc_data<V> oldSeq;
	seq.swap(oldSeq);
}

template <typename V>
abstr_sequence_provider<V>* heap_sequence_provider<V>::CreateProvider()
{
	static heap_sequence_provider<V> theProvider;
	return &theProvider;
}

template <typename V>
void heap_sequence_provider<V>::Destroy()
{
}

template <typename V>
abstr_sequence_provider<IndexRange<SizeT>>* heap_sequence_provider<V>::CloneForSeqs() const
{
	return heap_sequence_provider<IndexRange<SizeT> >::CreateProvider();
}

// =================================================== heap_sequence_provider private implementation

template <typename V>
void heap_sequence_provider<V>::Shrink(alloc_t& seq) 
{
	if (seq.size()< seq.m_Capacity / 4)
	{
		if (!seq.size())
			clear(seq);
		else
		{
			managed_alloc_data<V> newSeq(seq.begin(), seq.end(), seq.size() MG_DEBUG_ALLOCATOR_SRC("heap_sequence_provider<V>::Shrink"));
			seq.swap(newSeq);
		}
	}
}

#endif //!defined(__RTC_MEM_HEAPSEQUENCEPROVIDER_IPP)
