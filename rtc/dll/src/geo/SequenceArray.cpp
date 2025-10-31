// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/SequenceArray.h"

#include "geo/Conversions.h"
#include "geo/StringArray.h"
#include "mem/HeapSequenceProvider.ipp"
#include "mem/SeqLock.h"

#include "mem/LockedSequenceObj.h"
#include "ser/BinaryStream.h"
#include "ser/SequenceArrayStream.h"
#include "set/VectorFunc.h"

//=======================================
// SequenceArray<T>::const_reference
//=======================================

//	compare contents
template <typename T>
bool SA_ConstReference<T>::operator ==(SA_ConstReference<T> rhs) const
{ 
	return (IsDefined() == rhs.IsDefined()) && equal(begin(), end(), rhs.begin(), rhs.end());
}

template <typename T>
bool SA_ConstReference<T>::operator < (SA_ConstReference<T> rhs) const
{ 
	return rhs.IsDefined() && (lex_compare(begin(), end(), rhs.begin(), rhs.end()) || !IsDefined());
}

template <typename T>
bool SA_ConstReference<T>::operator ==(SA_Reference<T> rhs) const
{
	return IsDefined()
		? rhs.IsDefined()
		? equal(begin(), end(), rhs.cbegin(), rhs.cend())
		: false
		: !rhs.IsDefined();
}

template <typename T>
bool SA_ConstReference<T>::operator < (SA_Reference<T> rhs) const
{
	return IsDefined()
		? rhs.IsDefined()
		? lex_compare(begin(), end(), rhs.cbegin(), rhs.cend())
		: false
		: rhs.IsDefined();
}

template <typename T>
bool SA_ConstReference<T>::operator ==(const sequence_value_type& rhs) const
{
	return IsDefined()
		? ::IsDefined(rhs)
		? ::equal(begin(), end(), cbegin_ptr(rhs), cend_ptr(rhs))
		: false
		: !::IsDefined(rhs);
}

template <typename T>
bool SA_ConstReference<T>::operator < (const sequence_value_type& rhs) const
{
	return IsDefined()
		? ::IsDefined(rhs)
		? lex_compare(begin(), end(), cbegin_ptr(rhs), cend_ptr(rhs))
		: false
		: ::IsDefined(rhs);
}

// ctor usally called from const_reference sequence_array<T>::operator [](size_type) const;
template <typename T>
SA_ConstReference<T>::SA_ConstReference(const SequenceArrayType* sa, const_seq_iterator cSeqPtr)
	:	m_Container(sa)
	,	m_CSeqPtr(cSeqPtr) 
{
	assert_iter(m_SeqPtr != seq_iterator());
	assert( !is_null() );
}

//=======================================
// SequenceArray<T>::reference
//=======================================

//	compare contents
template <typename T>
bool SA_Reference<T>::operator ==(SA_ConstReference<T> rhs) const
{ 
	return IsDefined()
		?	rhs.IsDefined()
			?	equal(cbegin(), cend(), rhs.begin(), rhs.end())
			:	false
		:	! rhs.IsDefined();
}

template <typename T>
bool SA_Reference<T>::operator < (SA_ConstReference<T> rhs) const
{ 
	return IsDefined()
		?	rhs.IsDefined()
			?	lex_compare(cbegin(), cend(), rhs.begin(), rhs.end())
			:	false
		:	rhs.IsDefined(); 
}

template <typename T>
bool SA_Reference<T>::operator ==(SA_Reference<T> rhs) const
{
	return IsDefined()
		? rhs.IsDefined()
		? equal(cbegin(), cend(), rhs.cbegin(), rhs.cend())
		: false
		: !rhs.IsDefined();
}

template <typename T>
bool SA_Reference<T>::operator < (SA_Reference<T> rhs) const
{
	return IsDefined()
		? rhs.IsDefined()
		? lex_compare(cbegin(), cend(), rhs.cbegin(), rhs.cend())
		: false
		: rhs.IsDefined();
}

template <typename T>
bool SA_Reference<T>::operator ==(const sequence_value_type& rhs) const
{
	return IsDefined()
		? ::IsDefined(rhs)
		? ::equal(cbegin(), cend(), cbegin_ptr(rhs), cend_ptr(rhs))
		: false
		: !::IsDefined(rhs);
}

template <typename T>
bool SA_Reference<T>::operator < (const sequence_value_type& rhs) const
{
	return IsDefined()
		? ::IsDefined(rhs)
		? lex_compare(cbegin(), cend(), cbegin_ptr(rhs), cend_ptr(rhs))
		: false
		: ::IsDefined(rhs);
}

template <typename T>
void SA_Reference<T>::assign(const_iterator first, const_iterator last MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert( !is_null() );

	m_Container->allocateSequenceRange(m_SeqPtr, first, last MG_DEBUG_ALLOCATOR_SRC_PARAM);
	assert(size() == last - first);
}

template <typename T>
void SA_Reference<T>::assign(Undefined) 
{
	assert( !is_null() );

	m_Container->m_ActualDataSize -= size();
	*m_SeqPtr = Undefined();
}

template <typename T>
void SA_Reference<T>::erase(iterator first, iterator last)
{
	assert( !is_null() );

	m_Container->cut_seq(m_SeqPtr, std::copy(last, end(), first) - begin());
}

template <typename T>
void SA_Reference<T>::clear()
{
	assert( !is_null() );

	m_Container->cut_seq(m_SeqPtr, 0);
}

template <typename T>
void SA_Reference<T>::resize(size_type seqSize, const value_type& initValue MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert( !is_null() );

	if (!::IsDefined(seqSize))
		assign(Undefined());
	else
		if (seqSize != size() || (seqSize == 0 && !IsDefined()))
			m_Container->allocateSequence(m_SeqPtr, seqSize, [initValue](auto i, auto e) { fast_fill(i, e, initValue);  } MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename T>
RTC_CALL void SA_Reference<T>::resize_uninitialized(size_type seqSize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert(!is_null());

	if (!::IsDefined(seqSize))
		assign(Undefined());
	else
		if (seqSize != size() || (seqSize == 0 && !IsDefined()))
			m_Container->allocateSequence(m_SeqPtr, seqSize, [](auto i, auto e) {} MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename T>
void SA_Reference<T>::push_back(const value_type& value MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert( !is_null() );
	assert(size() == 0 || end()== m_Container->m_Values.end()); // not an actual requirement for the following, but to avoid fragmentation it's good to obey this
	m_Container->allocateSequence(m_SeqPtr, size()+1, [value](auto i, auto e) { assert(i + 1 == e);  *i = value; } MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename T>
void SA_Reference<T>::push_back(MG_DEBUG_ALLOCATOR_TXT_ARG)
{
	assert(!is_null());
	assert(size() == 0 || end() == m_Container->m_Values.end()); // not an actual requirement for the following, but to avoid fragmentation it's good to obey this
	m_Container->allocateSequence(m_SeqPtr, size() + 1, [](auto i, auto e) {} MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename T>
void SA_Reference<T>::operator =(SA_ConstReference<T> rhs) 
{ 
	if (rhs.IsDefined()) 
		assign(rhs.begin(), rhs.end() MG_DEBUG_ALLOCATOR_SRC("SA_Reference<T>::operator =(SA_ConstReference<T> rhs)"));
	else 
		assign(Undefined()); 
}

template <typename T>
void SA_Reference<T>::operator =(SA_Reference rhs) 
{ 
	assert(!is_null() ); 
	if (rhs.IsDefined())
		assign(rhs.begin(), rhs.end() MG_DEBUG_ALLOCATOR_SRC("SA_Reference<T>::operator =(SA_Reference rhs)"));
	else
		assign(Undefined());
}

template <typename T>
void SA_Reference<T>::operator =(const typename sequence_traits<T>::container_type& rhs) 
{ 
	if (::IsDefined(rhs))
		assign(begin_ptr(rhs), end_ptr(rhs) MG_DEBUG_ALLOCATOR_SRC("SA_Reference<T>::operator =(const typename sequence_traits<T>::container_type& rhs)"));
	else
		assign(Undefined());
}


template <typename T>
void SA_Reference<T>::swap(SA_Reference<T>& rhs)
{
	assert(!    is_null() );
	assert(!rhs.is_null() );
	if (m_Container == rhs.m_Container)
		omni::swap(*m_SeqPtr, *rhs.m_SeqPtr);
	else
	{
		SizeT n1 = size();
		SizeT n2 = rhs.size();
		if (n1 < n2)
		{
			swap_range(begin(), end(), rhs.begin());
			this->append(rhs.begin()+n1, rhs.end() MG_DEBUG_ALLOCATOR_SRC("SA_Reference<T>::swap"));
			rhs.erase(rhs.begin()+n1, rhs.end());
		}
		else
		{
			swap_range(begin(), begin()+n2, rhs.begin());
			rhs.append(begin()+n2, end() MG_DEBUG_ALLOCATOR_SRC("SA_Reference<T>::swap"));
			this->erase(begin()+n2, end());
		}
	}
}

template <typename T>
SA_Reference<T>::SA_Reference()
	:	m_SeqPtr() 
{
	assert(is_null());
}

template <typename T>
SA_Reference<T>::SA_Reference(SequenceArrayType* container, seq_iterator seqPtr)
	:	m_SeqPtr(seqPtr)
	,	m_Container(container)
{
	assert_iter(m_SeqPtr != seq_iterator());
	assert(m_Container);
};

//=======================================
// sequence_array
//=======================================

// sequence_array constructors

template <typename T>
sequence_array<T>::sequence_array(const sequence_array<T>& src, data_size_type expectedGrowth MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	Reset(0, 0 MG_DEBUG_ALLOCATOR_SRC_PARAM);
	assign(src, expectedGrowth MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename T>
void sequence_array<T>::assign(const sequence_array<T>& src, data_size_type expectedGrowth MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	MGD_CHECKDATA(IsLocked());
	MGD_CHECKDATA(src.IsLocked());

	assert(src.calcActualDataSize() == src.actual_data_size());

	reset(src.size(), src.actual_data_size() + expectedGrowth MG_DEBUG_ALLOCATOR_SRC_PARAM);

	typename base_type::const_seq_iterator
		i = src.m_Indices.begin(),
		e = src.m_Indices.end();
	typename base_type::seq_iterator
		ri = m_Indices.begin();
	const_data_iterator
		srcDataBegin = src.m_Values.begin();

	for (; i != e; ++ri, ++i)
		if(IsDefined(*i))
			allocateSequenceRange(ri, srcDataBegin + i->first, srcDataBegin + i->second MG_DEBUG_ALLOCATOR_SRC_PARAM);
		else
			Assign(*ri, Undefined() );

	assert(actual_data_size  () == src.actual_data_size());
	assert(calcActualDataSize() == actual_data_size());
	assert(!IsDirty());
}

template <typename T>
void sequence_array<T>::swap(sequence_array<T>& rhs) noexcept
{
	m_Values.swap(rhs.m_Values);
	m_Indices.swap(rhs.m_Indices);
	std::swap(m_ActualDataSize, rhs.m_ActualDataSize);
}

// sequence_array collection modification

template <typename T>
void sequence_array<T>::erase(const iterator& first, const iterator& last)
{
	assert(first == last || (first.m_Container == this && last.m_Container == this));

	for (iterator i = first; i != last; ++i)
		abandon(i.m_SeqPtr->first, i.m_SeqPtr->second);

	m_Indices.erase(first.m_SeqPtr, last.m_SeqPtr);
}

template <typename T>
void sequence_array<T>::clear()
{
	erase(begin(), end());
	assert(empty()); 
	m_Values.clear();
}

// sequence_array access control

template <typename T>
void sequence_array<T>::Lock(dms_rw_mode rwMode) const  // thread safe operation
{ 
	assert(rwMode >= dms_rw_mode::read_only); // no undefined nor check_only

	SeqLock<const seq_vector_t> lock(m_Indices, dms_must_keep(rwMode) ? rwMode : dms_rw_mode::write_only_mustzero); // may throw without any rolbackhere
	m_Values.Lock(dms_must_keep(rwMode) ? rwMode : dms_rw_mode::write_only_all);                            // may throw with rollback managed by lock
	if (dms_must_keep(rwMode))
	{
		if (!m_ActualDataSize)
		{
			m_ActualDataSize = calcActualDataSize(); // no throw operation ?
			MG_CHECK(m_ActualDataSize <= m_Values.size());
		}
	}
	else
	{
		m_ActualDataSize = 0;
//		m_Values.clear();
	}
	lock.release();

	MG_DEBUGCODE( checkActualDataSize(); )
	MG_DEBUGCODE( checkConsecutiveness(); )

//	assert(!IsDirty()); // last unlock should have packed sequences
}

template <typename T>
void sequence_array<T>::ResetAllocator(abstr_sequence_provider<T>* pr) 
{ 
	ResetAllocators(pr ? pr->CloneForSeqs() : nullptr, pr);
}

template <typename T>
void sequence_array<T>::ResetAllocators(abstr_sequence_provider<IndexRange<SizeT>>* prIndex, abstr_sequence_provider<T>* prSeqs)
{
	MGD_CHECKDATA(!m_Indices.IsLocked());
	MGD_CHECKDATA(!m_Values.IsLocked());

	m_Indices.ResetAllocator(prIndex);
	m_Values.ResetAllocator(prSeqs);
	m_ActualDataSize = 0;

	MGD_CHECKDATA(!m_Indices.IsLocked());
	MGD_CHECKDATA(!m_Values.IsLocked());
}

template <typename T>
void sequence_array<T>::Reset(size_type nrSeqs, typename data_vector_t::size_type expectedDataSize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	MGD_CHECKDATA(!IsLocked());

	if (!m_Indices.IsAssigned())
	{
		assert(!m_Values.IsAssigned());

		ResetAllocator(heap_sequence_provider<T>::CreateProvider());

		assert(m_Indices.IsAssigned());
		assert(m_Values.IsAssigned());

		if (!nrSeqs)
			return;
	}

	SeqLock<sequence_array> selfLock(*this, dms_rw_mode::write_only_mustzero);

	reset(nrSeqs, expectedDataSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename T>
void sequence_array<T>::reset(size_type nrSeqs, typename data_vector_t::size_type expectedDataSize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	MGD_CHECKDATA(IsLocked());
	MGD_CHECKDATA(m_Indices.IsAssigned());

	m_Values.clear();
	data_reserve(expectedDataSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
	  
//	assert(m_Indices.size() == 0); // when else would this be called
	m_Indices.clear();
	assert(m_Indices.size() == 0); // now we're certain
	m_Indices.resizeSO(nrSeqs, true MG_DEBUG_ALLOCATOR_SRC_PARAM);
	m_ActualDataSize = 0;

	MG_DEBUGCODE( checkActualDataSize() );
}

template <typename T>
void sequence_array<T>::Resize(typename data_vector_t::size_type expectedDataSize, size_type expectedSeqsSize, size_type nrSeqs MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert(nrSeqs <= expectedSeqsSize); // PRECONDITION

	if (!m_Indices.IsAssigned())
		ResetAllocator(heap_sequence_provider<T>::CreateProvider());

	SeqLock<sequence_array> selfLock(*this, dms_rw_mode::read_write);

	data_reserve(expectedDataSize MG_DEBUG_ALLOCATOR_SRC_PARAM);

	assert(m_Indices.size() <= nrSeqs);  // we don't allow cutting here

	reserve(expectedSeqsSize MG_DEBUG_ALLOCATOR_SRC_PARAM);

	// m_Seq.resize without shrinking
	m_Indices.resizeSO(nrSeqs, true MG_DEBUG_ALLOCATOR_SRC_PARAM);

	MG_DEBUGCODE( checkActualDataSize() );
}


template <typename T>
bool sequence_array<T>::allocate_data(data_size_type expectedGrowth MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	MGD_CHECKDATA(IsLocked());

	auto currCapacity = actual_data_size();
	locked_sequence<T> oldData(false);
	auto dataWasMoved =  allocate_data(oldData, expectedGrowth MG_DEBUG_ALLOCATOR_SRC_PARAM);

	assert(currCapacity + expectedGrowth <= m_Values.capacity());
	return dataWasMoved;
}

template <typename T>
bool sequence_array<T>::allocate_data(data_vector_t& oldData, typename data_vector_t::size_type expectedGrowth MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert(oldData.empty());
	assert(oldData.IsAssigned());
	assert(oldData.IsHeapAllocated());
//	assert(m_Values.IsAssigned());
	MGD_CHECKDATA(oldData.IsLocked());
	MGD_CHECKDATA(m_Values.IsLocked());

	MG_DEBUGCODE( checkActualDataSize(); )

	assert(oldData.empty()     );
	assert(oldData.IsAssigned());
	assert(oldData.IsHeapAllocated());

	// NYI: doe alleen m_Values en laat m_Indices ongemoeid
	if (!IsDirty())
	{
		assert(actual_data_size() == m_Values.size());
		if (expectedGrowth + m_Values.size() <= m_Values.capacity())
			return false;
		if (!m_Values.IsHeapAllocated())
		{
			data_reserve(actual_data_size() + expectedGrowth MG_DEBUG_ALLOCATOR_SRC_PARAM);
			assert(m_Values.size() + expectedGrowth <= m_Values.capacity());
			return false;
		}
	}

	if (m_Values.IsHeapAllocated() || !m_Values.IsAssigned())
		m_Values.swap(oldData);
	else
	{
		assert(IsDirty());
		oldData.append(m_Values.begin(), m_Values.end() MG_DEBUG_ALLOCATOR_SRC_PARAM);
		m_Values.clear();
	}

	assert(m_Values.empty()    );
	assert(m_Values.size() == 0);
	assert(m_Values.IsAssigned());

	data_reserve(actual_data_size() + expectedGrowth MG_DEBUG_ALLOCATOR_SRC_PARAM);

	typename base_type::seq_vector_t::iterator
		i = m_Indices.begin(),
		e = m_Indices.end();

	typename base_type::data_size_type                dataEnd  = 0;
	typename base_type::data_vector_t::const_iterator oldBegin = oldData.begin();
	while (i!=e) 
	{
		data_size_type size = i->size();
		assert(size || !IsDefined(*i) || ((!i->first) && (!i->second)));
		if (size)
		{
			data_size_type dataCurr = dataEnd;
			m_Values.append(oldBegin + i->first, oldBegin + i->second MG_DEBUG_ALLOCATOR_SRC_PARAM);
			dataEnd += size;
			assert(dataEnd == m_Values.size());
			*i = seq_t(dataCurr, dataEnd);
		}
		++i;
	}

	MG_DEBUGCODE( checkActualDataSize(); )
	MG_DEBUGCODE( checkConsecutiveness(); )

//	assert(!IsDirty());
	assert(m_Values.size() +expectedGrowth == m_Values.capacity());
	return true;
}

template <typename T>
void sequence_array<T>::cut(size_type nrSeqs)
{
	bool keepMoreThanHalf = (nrSeqs > size() / 2);
	if (keepMoreThanHalf)
	{
		m_ActualDataSize -= CalcActualDataSize(begin() + nrSeqs, end());
		MG_DEBUGCODE( checkActualDataSize(); )
	}
	m_Indices.cut(nrSeqs);
	if (!keepMoreThanHalf)
		m_ActualDataSize = calcActualDataSize();
}

template <typename T>
template <typename Initializer> void sequence_array<T>::allocateSequence(typename base_type::seq_iterator seqPtr, data_size_type newSize, Initializer&& initFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	MGD_CHECKDATA(IsLocked());
	assert_iter(seqPtr!=seq_iterator());
	assert(seq_size_type(seqPtr - m_Indices.begin()) < m_Indices.size());
	assert(m_ActualDataSize >= seqPtr->size());

	data_size_type                          oldSize = seqPtr->size();
	typename data_vector_t::difference_type delta   =  newSize - oldSize;

	if (delta > 0)
	{
		bool atEnd = (seqPtr->second == m_Values.size());
		if ((atEnd ? delta : newSize) <= m_Values.capacity() - m_Values.size())
		{
			if (oldSize && !atEnd)
			{
				appendValues(m_Values.begin()+seqPtr->first, m_Values.begin()+seqPtr->second MG_DEBUG_ALLOCATOR_SRC_PARAM); // copy oldSize elemeents from original 
				abandon     (seqPtr->first, seqPtr->second); // and abandon that area
			}
			appendInitializer(delta, initFunc MG_DEBUG_ALLOCATOR_SRC_PARAM);
			*seqPtr = seq_t(m_Values.size()-newSize, m_Values.size());
		}
		else
		{
			locked_sequence<T> oldData(false);

			data_size_type
				oldFirst  = seqPtr->first,
				oldSecond = seqPtr->second;

			abandon(oldFirst, oldSecond);  // removes oldSize bytes from m_ActualData
			*seqPtr = seq_t();

			bool dataWasMoved = allocate_data(oldData, Max<data_size_type>(m_ActualDataSize+2*oldSize, newSize) MG_DEBUG_ALLOCATOR_SRC_PARAM);
			assert(!IsDirty());
			if (oldSize)
			{
				if (dataWasMoved)
					appendValues(oldData.begin() + oldFirst, oldData.begin() + oldSecond MG_DEBUG_ALLOCATOR_SRC_PARAM);
				else
				{
					assert(oldFirst == m_Values.size());
					appendInitializer(oldSize, [](auto i, auto e) {} MG_DEBUG_ALLOCATOR_SRC_PARAM); // skip old range that was abandoned but is still there.
					assert(oldSecond == m_Values.size());
				}
			}
			appendInitializer(delta, initFunc MG_DEBUG_ALLOCATOR_SRC_PARAM);

			*seqPtr = seq_t(m_Values.size()-newSize, m_Values.size());

			MG_DEBUGCODE( checkActualDataSize() );
		}
	}
	else
		cut_seq(seqPtr, newSize);

	assert(newSize == seqPtr->size());
}

template <typename T>
void sequence_array<T>::allocateSequenceRange(typename base_type::seq_iterator seqPtr, const_data_iterator first, const_data_iterator last MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	MGD_CHECKDATA(IsLocked());

	assert_iter(seqPtr!=seq_iterator());
	assert(seq_size_type(seqPtr - m_Indices.begin()) < m_Indices.size());

	data_size_type
		newSize = last - first,
		oldSize = seqPtr->size();
	assert(m_ActualDataSize >= oldSize);

	typename data_vector_t::difference_type delta =  newSize - oldSize;

	if (delta > 0)
	{
		assert(newSize);
		bool atEnd = (seqPtr->second == m_Values.size());
		if ((atEnd ? newSize - oldSize : newSize) <= m_Values.capacity() - m_Values.size())
		{
			if (!atEnd)
			{
				appendValues(first, last MG_DEBUG_ALLOCATOR_SRC_PARAM);                      // insert new range
				abandon(seqPtr->first, seqPtr->second);        // and abandon old sequence
			}
			else
			{
				const_data_iterator copyArea = first+oldSize;
				std::copy(first, copyArea, m_Values.begin() + seqPtr->first);   // overwrite first part
				appendValues(copyArea, last MG_DEBUG_ALLOCATOR_SRC_PARAM);                   // insert second part
			}
			*seqPtr = seq_t(m_Values.size()-newSize, m_Values.size());
		}
		else
		{
			abandon(seqPtr->first, seqPtr->second);
			*seqPtr = seq_t();

			auto newCapacity = Max<data_size_type>(m_ActualDataSize + 2 * oldSize, newSize);
			bool dataWasMoved = allocate_data(newCapacity MG_DEBUG_ALLOCATOR_SRC_PARAM);
			appendValues(first, last MG_DEBUG_ALLOCATOR_SRC_PARAM);

			*seqPtr = seq_t(m_Values.size()-newSize, m_Values.size());

			assert(m_ActualDataSize == calcActualDataSize());

		}
	}
	else // delta <= 0
	{
		fast_copy(first, last, m_Values.begin() + seqPtr->first);
		cut_seq(seqPtr, newSize);
	}
	assert(newSize == seqPtr->size());
}

template <typename T>
void sequence_array<T>::cut_seq(typename base_type::seq_iterator seqPtr, typename data_vector_t::size_type newSize)
{
	MGD_CHECKDATA(IsLocked());

	assert_iter(seqPtr != seq_iterator());
	assert(SizeT(seqPtr - m_Indices.begin()) < m_Indices.size());

	assert(m_ActualDataSize >= seqPtr->size());

	size_type oldSize = seqPtr->size();
	assert(newSize <= oldSize);
	
	if (newSize < oldSize)
	{
		if (seqPtr->second == m_Values.size())
		{
			size_type delta = oldSize - newSize;
			m_Values.cut(m_Values.size() - delta);
			m_ActualDataSize -= delta;
		}
		else
			abandon(seqPtr->first + newSize, seqPtr->second);
	}

	if (newSize)
		seqPtr->second = seqPtr->first + newSize;
	else
		*seqPtr = seq_t(); // set all empty sequences to start of m_Values to avoid overlap after expanding a predecessing non-empty sequence

	assert(newSize == seqPtr->size());
}

template <typename T>
void sequence_array<T>::abandon(data_size_type first, data_size_type last)
{
	data_size_type n = last - first;
	assert( n <= m_ActualDataSize);
	m_ActualDataSize -= n;
	
	destroy_range(m_Values.begin()+first, m_Values.begin() + last);
}

template <typename T>
template <typename Initializer>
void sequence_array<T>::appendInitializer(size_type n, Initializer&& initFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert(m_Values.capacity() - m_Values.size() >= n);

	m_Values.appendInitializer(n, initFunc MG_DEBUG_ALLOCATOR_SRC_PARAM);
	m_ActualDataSize += n;
}

//	template <typename InputIterator>
//	void appendValues(InputIterator first, InputIterator last)
template <typename T>
void sequence_array<T>::appendValues(const_data_iterator first, const_data_iterator last MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	assert((m_Values.capacity() - m_Values.size()) >= data_size_type(last - first));

	m_Values.append(first, last MG_DEBUG_ALLOCATOR_SRC_PARAM);
	m_ActualDataSize += (last - first);
}

template <typename T>
void sequence_array<T>::StreamOut(BinaryOutStream& ar) const
{
	if (CanWrite())
		const_cast<sequence_array<T>*>(this)->allocate_data(0 MG_DEBUG_ALLOCATOR_SRC_SA); // we only want to write allocated elements, so reallocate if dirty

	assert(! IsDirty() );

	MG_DEBUGCODE( checkActualDataSize(); );

	ar << m_Indices.size();
	for (auto i=m_Indices.begin(), e=m_Indices.end(); i!=e; ++i)
		ar << i->first << i->second;

	ar << m_Values;
}

template <typename T>
void sequence_array<T>::StreamIn (BinaryInpStream& ar, bool mayResize)
{
	SizeT s;
	ar >> s;

	if (s != m_Indices.size())
	{
		if (mayResize)
			m_Indices.resizeSO(s, false MG_DEBUG_ALLOCATOR_SRC_SA);
		else
			throwErrorF("StreamIn", "provided input stream notifies %d index elements but sequence_array has %d index elements",
				s, m_Indices.size()
			);
	}
	assert(m_Indices.size() == s);
	assert(m_Indices.begin() + s == m_Indices.end());
	for (SizeT i = 0; i != s; ++i)
		ar >> m_Indices[i].first >> m_Indices[i].second;
	if (ar.Buffer().AtEnd())
		throwErrorF("StreamIn", "provided input stream ended before sequence-array was read.\n"
			"Did the input stream represent sequences or strings? Consider configuring a fixed size value type"
		);

	ar >> m_Values;
	m_ActualDataSize = m_Values.size();

	MG_DEBUGCODE( checkActualDataSize(); );
}

#if defined(MG_DEBUG)

template <typename T>
void sequence_array<T>::checkActualDataSize() const
{
	MGD_CHECKDATA(IsLocked());
	if (!m_Values.IsAssigned())
		return;

	SizeT as = calcActualDataSize(); // O(#elements)
	assert(as == m_ActualDataSize);
}

template <typename T>
void sequence_array<T>::checkConsecutiveness() const
{
	MGD_CHECKDATA(IsLocked());
	for (const auto& indexPair : m_Indices)
	{
		assert((indexPair.first  <= indexPair.second) || !IsDefined(indexPair));
		assert((indexPair.second <= m_ActualDataSize) || !IsDefined(indexPair));
	}
}

#endif

//=======================================
// SequenceArray<T> instantiation
//=======================================

#include "geo/Geometry.h"
#include "geo/Point.h"
#include "set/Token.h"

template struct sequence_array<char>;
template struct sequence_array<DPoint>;
template struct sequence_array<FPoint>;
//template struct sequence_array<TokenID>;
template void sequence_array<TokenID>::allocateSequenceRange(typename base_type::seq_iterator seqPtr, const_data_iterator first, const_data_iterator last MG_DEBUG_ALLOCATOR_SRC_ARG);
template void sequence_array<TokenID>::Reset(size_type nrSeqs, data_vector_t::size_type expectedDataSize MG_DEBUG_ALLOCATOR_SRC_ARG);
template void sequence_array<TokenID>::Lock(enum dms_rw_mode) const;

template struct SA_Reference<char>;
template struct SA_Reference<DPoint>;
template struct SA_Reference<FPoint>;
//template struct SA_Reference<TokenID>;
template SA_Reference<TokenID>::SA_Reference(SequenceArrayType* container, seq_iterator seqPtr);

template struct SA_ConstReference<char>;
template struct SA_ConstReference<DPoint>;
template struct SA_ConstReference<FPoint>;
//template struct SA_ConstReference<TokenID>;
template SA_ConstReference<TokenID>::SA_ConstReference(const SequenceArrayType* sa, const_seq_iterator cSeqPtr);

template struct managed_alloc_data<UInt64>;
template struct managed_alloc_data<UInt32>;
template struct managed_alloc_data<UInt16>;
template struct managed_alloc_data<UInt8>;

template struct managed_alloc_data<Int64>;
template struct managed_alloc_data<Int32>;
template struct managed_alloc_data<Int16>;
template struct managed_alloc_data<Int8>;

template struct managed_alloc_data<Float32>;
template struct managed_alloc_data<Float64>;

template struct managed_alloc_data<IPoint>;
template struct managed_alloc_data<UPoint>;
template struct managed_alloc_data<SPoint>;
template struct managed_alloc_data<WPoint>;
template struct managed_alloc_data<FPoint>;
template struct managed_alloc_data<DPoint>;

template struct my_vector<UInt64>;
template struct my_vector<UInt32>;
template struct my_vector<UInt16>;
template struct my_vector<UInt8>;

template struct my_vector<Int64>;
template struct my_vector<Int32>;
template struct my_vector<Int16>;
template struct my_vector<Int8>;

template struct my_vector<Float32>;
template struct my_vector<Float64>;

template struct my_vector<IPoint>;
template struct my_vector<UPoint>;
template struct my_vector<SPoint>;
template struct my_vector<WPoint>;
template struct my_vector<FPoint>;
template struct my_vector<DPoint>;

template class heap_sequence_provider<UInt64>;
template class heap_sequence_provider<UInt32>;
template class heap_sequence_provider<UInt16>;
template class heap_sequence_provider<UInt8>;

template class heap_sequence_provider<Int64>;
template class heap_sequence_provider<Int32>;
template class heap_sequence_provider<Int16>;
template class heap_sequence_provider<Int8>;

//template class heap_sequence_provider<Float32>;
//template class heap_sequence_provider<Float64>;

template class heap_sequence_provider<IPoint>;
template class heap_sequence_provider<UPoint>;
template class heap_sequence_provider<SPoint>;
template class heap_sequence_provider<WPoint>;
template class heap_sequence_provider<FPoint>;
template class heap_sequence_provider<DPoint>;

template struct sequence_array<UInt64>;
template struct sequence_array<UInt32>;
template struct sequence_array<UInt16>;
template struct sequence_array<UInt8>;

template struct sequence_array<Int64>;
template struct sequence_array<Int32>;
template struct sequence_array<Int16>;
template struct sequence_array<Int8>;

template struct sequence_array<Float64>;
template struct sequence_array<Float32>;

template struct sequence_array<SPoint>;
template struct sequence_array<IPoint>;
template struct sequence_array<WPoint>;
template struct sequence_array<UPoint>;

template struct SA_Reference<UInt64>;
template struct SA_Reference<UInt32>;
template struct SA_Reference<UInt16>;
template struct SA_Reference<UInt8>;

template struct SA_Reference<Int64>;
template struct SA_Reference<Int32>;
template struct SA_Reference<Int16>;
template struct SA_Reference<Int8>;

template struct SA_Reference<Float64>;
template struct SA_Reference<Float32>;

template struct SA_Reference<SPoint>;
template struct SA_Reference<IPoint>;
template struct SA_Reference<WPoint>;
template struct SA_Reference<UPoint>;

template struct SA_ConstReference<UInt64>;
template struct SA_ConstReference<UInt32>;
template struct SA_ConstReference<UInt16>;
template struct SA_ConstReference<UInt8>;

template struct SA_ConstReference<Int64>;
template struct SA_ConstReference<Int32>;
template struct SA_ConstReference<Int16>;
template struct SA_ConstReference<Int8>;

template struct SA_ConstReference<Float64>;
template struct SA_ConstReference<Float32>;

template struct SA_ConstReference<SPoint>;
template struct SA_ConstReference<IPoint>;
template struct SA_ConstReference<WPoint>;
template struct SA_ConstReference<UPoint>;
