// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_HEAPSEQUENCEPROVIDER_H)
#define __RTC_MEM_HEAPSEQUENCEPROVIDER_H

#include "AbstrSequenceProvider.h"
#include "Parallel.h"

//----------------------------------------------------------------------
// heap_sequence_provider
//----------------------------------------------------------------------

RTC_CALL void throwInsertError(SizeT seqSize, SizeT n);

template <typename V>
class heap_sequence_provider : public abstr_sequence_provider<V>
{
	using typename abstr_sequence_provider<V>::alloc_t;

	typedef typename sequence_traits<V>::value_type     value_type;
	typedef typename param_type<value_type>::type       param_t;
	typedef typename sequence_traits<V>::container_type container_t; 
	typedef typename sequence_obj<V>::iterator          iterator;

public:
//	override abstr_sequence_provider
	void Destroy() override;

	void Open(alloc_t& seq, SizeT nrElem, dms_rw_mode rwMode, bool isTmp MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{
		dms_assert(rwMode != dms_rw_mode::unspecified);
		if (rwMode == dms_rw_mode::write_only_mustzero)
			clear(seq);
		if (IsDefined(nrElem))
			resizeSP(seq, nrElem, rwMode != dms_rw_mode::write_only_all MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}
	SizeT Size    (const alloc_t& seq) const override { return seq.size    (); }
	SizeT Capacity(const alloc_t& seq) const override { return seq.capacity(); }

	void reserve (alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG) override;
	void resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) override;
	void cut     (alloc_t& seq, SizeT newSize) override;
	void clear   (alloc_t& seq) override;
	void free    (alloc_t& seq) override  { clear(seq); }
	
	bool CanWrite() const override { return true; }
//	bool IsOpen()   const override { return true; }
	bool IsHeapAllocated() const override   { return true; }

	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const override;
	RTC_CALL static abstr_sequence_provider<V>* CreateProvider();
	RTC_CALL static SizeT max_size();

private:
	heap_sequence_provider()  {}
	~heap_sequence_provider() {}
	heap_sequence_provider(const heap_sequence_provider&); // Dont call

	void Shrink(alloc_t& seq);

	void SetSize(alloc_t& seq, SizeT newSize) override
	{
		seq.setsize(newSize);
		dms_assert(seq.size() == newSize);
	}
	void Check(alloc_t& seq)
	{
		dms_assert(seq.size() <= seq.m_Capacity);
	}

	template <typename Iter    > static iterator iter(Iter               i) { return &*i; }
	template <typename T       > static iterator iter(T*                 p) { return   p; }
	template <typename T       > static iterator iter(SA_Iterator<T>     i) { return i; }
	template <int N, typename B> static iterator iter(bit_iterator<N, B> i) { return i; }
};


#endif // __RTC_MEM_HEAPSEQUENCEPROVIDER_H
