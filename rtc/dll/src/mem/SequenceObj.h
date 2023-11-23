// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_SEQUENCEOBJ_H)
#define __RTC_MEM_SEQUENCEOBJ_H

#include "mem/AbstrSequenceProvider.h"

//----------------------------------------------------------------------
// interfaces to allocated (file mapped?) arrays
//----------------------------------------------------------------------

template <typename T>
struct DestroyablePtr : ptr_base<T, boost::noncopyable>
{
	using base_type = ptr_base<T, boost::noncopyable>;
	using typename base_type::pointer;

	DestroyablePtr(T* ptr = nullptr) : base_type(ptr) {}
	~DestroyablePtr() { reset(); }
	void operator =(pointer ptr) { reset(ptr);  }

	void reset(pointer ptr = nullptr)
	{
		if (this->has_ptr())
		{
			dms_assert(this->get_ptr() != ptr);
			this->get_ptr()->Destroy();
		}
		this->m_Ptr = ptr;
	}
};

//----------------------------------------------------------------------
// class  : sequence_obj
//----------------------------------------------------------------------

template <typename V>
struct sequence_obj : private boost::noncopyable
{
public:
	typedef abstr_sequence_provider<V>              provider_t;

	typedef typename sequence_traits<V>::value_type value_type;
	typedef typename sequence_traits<V>::seq_t      seq_t;
	typedef typename sequence_traits<V>::cseq_t     cseq_t;

	typedef typename seq_t::iterator                iterator;
	typedef typename cseq_t::const_iterator         const_iterator;

	typedef typename seq_t::size_type               size_type;

	typedef typename param_type<value_type>::type   param_t;

	typedef typename seq_t::reference               reference;
	typedef typename cseq_t::const_reference        const_reference;
	typedef typename seq_t::difference_type         difference_type;

	sequence_obj() {}
	explicit sequence_obj(provider_t* provider) : m_Provider(provider) {}
	explicit sequence_obj(alloc_data<V> allocData) : m_Data(std::move(allocData)) {}
	sequence_obj(sequence_obj&& rhs) noexcept
		:	sequence_obj()
	{
		MGD_CHECKDATA(!IsLocked());
		operator =(std::move(rhs));
	}
	~sequence_obj() 
	{
		MGD_CHECKDATA(!IsLocked());
		ResetAllocator();
	}
	void operator = (sequence_obj&& rhs)  noexcept
	{
		ResetAllocator();

		m_Data = std::move(rhs.m_Data); rhs.m_Data = {};
		m_Provider.swap( rhs.m_Provider );
		dms_assert(rhs.m_Provider.is_null());

#if defined(MG_DEBUG_DATA)
		md_IsLocked = rhs.md_IsLocked;
		rhs.md_IsLocked = false;
#endif

	}
	void swap(sequence_obj<V>& rhs) noexcept { m_Data.swap(rhs.m_Data); m_Provider.swap(rhs.m_Provider); MG_DEBUG_DATA_CODE(std::swap(md_IsLocked, rhs.md_IsLocked); ) }

	SizeT Size    () const { return m_Provider ? m_Provider->Size    (m_Data) : 0; }
	SizeT Capacity() const { return m_Provider ? m_Provider->Capacity(m_Data) : 0; }
	SizeT Empty   () const { return Size() == 0; }

	void   reserve(SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG) { MGD_CHECKDATA(IsLocked()); m_Provider->reserve(m_Data, newSize MG_DEBUG_ALLOCATOR_SRC_PARAM); }
	void   resizeSO(SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) { MGD_CHECKDATA(IsLocked()); m_Provider->resizeSP(m_Data, newSize, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM); }

	// operations on locked data
	SizeT          size () const { MGD_CHECKDATA(IsLocked()); return m_Data.size(); }
	bool           empty() const { MGD_CHECKDATA(IsLocked()); return m_Data.empty(); }
	SizeT       capacity() const { MGD_CHECKDATA(IsLocked()); return m_Data.m_Capacity; }
	iterator       begin()       { MGD_CHECKDATA(IsLocked()); assert(!m_Provider || CanWrite()); return m_Data.begin(); }
	iterator       end  ()       { MGD_CHECKDATA(IsLocked()); assert(!m_Provider || CanWrite()); return m_Data.end();   }
	const_iterator begin() const { MGD_CHECKDATA(IsLocked()); return m_Data.begin(); }
	const_iterator end  () const { MGD_CHECKDATA(IsLocked()); return m_Data.end();   }

	reference       operator [](size_type i)       { dms_assert(i < size()); return begin()[i]; }
	const_reference operator [](size_type i) const { dms_assert(i < size()); return begin()[i]; }
	reference       back() { dms_assert(!empty()); return end()[-1]; }
	const_reference back() const { dms_assert(!empty()); return end()[-1]; }

	template <typename Initializer>
	void appendInitializer(SizeT n, Initializer&& initFunc) 
	{ 
		MGD_CHECKDATA(IsLocked()); 
		MGD_CHECKDATA(m_Provider);
		SizeT oldSize = size();
		dms_assert(oldSize + n > oldSize); // No overflow
		m_Provider->grow(m_Data, n, false MG_DEBUG_ALLOCATOR_SRC_SA);
		dms_assert(m_Data.size() == oldSize + n);
		initFunc(m_Data.begin() + oldSize, m_Data.end());
	}
	void appendRange(const_iterator first, const_iterator last)
	{
		MGD_CHECKDATA(IsLocked());
		MGD_CHECKDATA(m_Provider);
		SizeT oldSize = size();
		SizeT n = last - first;
		dms_assert(oldSize + n >= oldSize); // No overflow
		m_Provider->grow(m_Data, n, false MG_DEBUG_ALLOCATOR_SRC_SA);
		dms_assert(m_Data.size() == oldSize + n);
		fast_copy(first, last, m_Data.begin() + oldSize);
	}

	void push_back(param_t v)
	{ 
		MGD_CHECKDATA(IsLocked());
		MGD_CHECKDATA(m_Provider);
		SizeT oldSize = size();
		dms_assert(oldSize + 1 > oldSize); // No overflow
		m_Provider->grow(m_Data, 1, false MG_DEBUG_ALLOCATOR_SRC_SA);
		dms_assert(m_Data.size() == oldSize + 1);
		m_Data.back() = v;
	}
	void erase(iterator b, iterator e) { MGD_CHECKDATA(IsLocked()); destroy_range(b, e);  raw_move(e, end(), b); cut(size() - (e - b)); }
	void cut(SizeT newNrElems)         { MGD_CHECKDATA(IsLocked()); MGD_CHECKDATA(m_Provider); m_Provider->cut(m_Data, newNrElems); }
	void clear ()                      { MGD_CHECKDATA(IsLocked()); MGD_CHECKDATA(m_Provider); m_Provider->clear(m_Data); dms_assert(empty()); }

	void ResetAllocator(provider_t* provider = nullptr)
	{
		MGD_CHECKDATA(!IsLocked());

		if (m_Provider)
		{

			m_Provider->free(m_Data);
			assert(m_Data.empty());
		}
		m_Provider = provider;
	}

//	bool IsOpen    () const { return m_Provider && m_Provider->IsOpen  (); }
	bool CanWrite  () const { return m_Provider && m_Provider->CanWrite(); }
	bool IsAssigned() const { return m_Provider; }
	bool IsHeapAllocated() const { return m_Provider && m_Provider->IsHeapAllocated(); }

	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const { return m_Provider->CloneForSeqs(); }

	void Open (SizeT nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG) { MGD_CHECKDATA(!md_IsLocked); m_Provider->Open(m_Data, nrElem, rwMode, isTmp, sfwa MG_DEBUG_ALLOCATOR_SRC_PARAM); }
//	void Close () { MGD_CHECKDATA(!IsLocked()); m_Provider->Close( m_Data); dms_assert(Empty()); }
	void Lock  (dms_rw_mode rwMode) const { MGD_CHECKDATA(!IsLocked()); if (m_Provider) m_Provider->Lock(m_Data, rwMode); MG_DEBUG_DATA_CODE(md_IsLocked = true ); }
	void UnLock() const { MGD_CHECKDATA( IsLocked() || !m_Provider); if (m_Provider) m_Provider->UnLock(m_Data); MG_DEBUG_DATA_CODE(md_IsLocked = false); }
//	void Drop  () { MGD_CHECKDATA(!IsLocked() && m_Provider); m_Provider->Drop  (m_Data); dms_assert(Empty()); }
	WeakStr GetFileName() const { return m_Provider->GetFileName(); }

private:
	sequence_obj(const sequence_obj<V>&);

	DestroyablePtr< provider_t > m_Provider;
	mutable alloc_data<V>        m_Data;
#if defined(MG_DEBUG_DATA)
	mutable bool md_IsLocked = false;
public:
	bool IsLocked() const { return md_IsLocked; }
#endif
};

template <typename V>
auto GetSeq(sequence_obj<V>& so) -> sequence_traits<V>::seq_t
{ 
	MGD_CHECKDATA(so.IsLocked()); 
	dms_assert(so.CanWrite()); 
	return { so.begin(), so.end() }; 
}

template <typename V>
auto GetConstSeq(const sequence_obj<V>& cso) -> sequence_traits<V>::cseq_t
{ 
	MGD_CHECKDATA(cso.IsLocked());
	return { cso.begin(), cso.end() };
}


template<typename V>
bool IsDefined(const sequence_obj<V>& v) 
{ 
	return true; 
}

template<typename V>
void MakeUndefined(sequence_obj<V>& v) 
{ 
	throwIllegalAbstract(MG_POS, "MakeUndefined");
}

#endif // __RTC_MEM_SEQUENCEOBJ_H
