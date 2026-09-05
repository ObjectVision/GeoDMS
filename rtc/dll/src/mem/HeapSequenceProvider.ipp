// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_HEAPSEQUENCEPROVIDER_IPP)
#define __RTC_MEM_HEAPSEQUENCEPROVIDER_IPP

#include "vt/IndexRange.h"
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
		// Allocate and copy first, then swap, as Shrink does: the swap hands the old buffer, with its
		// capacity, to newSeq, whose destructor frees it. The previous form moved seq out before
		// allocating; alloc_data's move leaves the source with its pointers and capacity 0, so an
		// allocation that threw (memory exhausted, ObjectVision/BAG-Tools#2) left seq with size above
		// capacity: an assert in Debug, and in Release a buffer that the next successful reserve
		// leaked, since free() skips a capacity of 0.
		managed_alloc_data<V> newSeq(seq.begin(), seq.end(), newSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
		seq.swap(newSeq);
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
