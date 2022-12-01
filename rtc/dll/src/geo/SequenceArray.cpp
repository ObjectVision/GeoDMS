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

#include "RtcPCH.h"
#pragma hdrstop

#include "geo/SequenceArray.h"

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
	dms_assert( !is_null() );
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
void SA_Reference<T>::assign(const_iterator first, const_iterator last)
{
	dms_assert( !is_null() );

	m_Container->allocateSequenceRange(m_SeqPtr, first, last);
	dms_assert(size() == last - first);
}

template <typename T>
void SA_Reference<T>::assign(Undefined) 
{
	dms_assert( !is_null() );

	m_Container->m_ActualDataSize -= size();
	*m_SeqPtr = Undefined();
}

template <typename T>
void SA_Reference<T>::erase(iterator first, iterator last)
{
	dms_assert( !is_null() );

	m_Container->cut_seq(m_SeqPtr, std::copy(last, end(), first) - begin());
}

template <typename T>
void SA_Reference<T>::clear()
{
	dms_assert( !is_null() );

	m_Container->cut_seq(m_SeqPtr, 0);
}

template <typename T>
void SA_Reference<T>::resize(size_type seqSize, const value_type& initValue)
{
	dms_assert( !is_null() );

	if (!::IsDefined(seqSize))
		assign(Undefined());
	else
		if (seqSize != size() || (seqSize == 0 && !IsDefined()))
			m_Container->allocateSequence(m_SeqPtr, seqSize, [initValue](auto i, auto e) { fast_fill(i, e, initValue);  });
}

template <typename T>
RTC_CALL void SA_Reference<T>::resize_uninitialized(size_type seqSize)
{
	dms_assert(!is_null());

	if (!::IsDefined(seqSize))
		assign(Undefined());
	else
		if (seqSize != size() || (seqSize == 0 && !IsDefined()))
			m_Container->allocateSequence(m_SeqPtr, seqSize, [](auto i, auto e) {} );
}

template <typename T>
void SA_Reference<T>::push_back(const value_type& value)
{
	dms_assert( !is_null() );
	dms_assert(size() == 0 || end()== m_Container->m_Values.end()); // not an actual requirement for the following, but to avoid fragmentation it's good to obey this
	m_Container->allocateSequence(m_SeqPtr, size()+1, [value](auto i, auto e) { dms_assert(i + 1 == e);  *i = value; } );
}

template <typename T>
void SA_Reference<T>::push_back()
{
	dms_assert(!is_null());
	dms_assert(size() == 0 || end() == m_Container->m_Values.end()); // not an actual requirement for the following, but to avoid fragmentation it's good to obey this
	m_Container->allocateSequence(m_SeqPtr, size() + 1, [](auto i, auto e) {} );
}

template <typename T>
void SA_Reference<T>::operator =(SA_ConstReference<T> rhs) 
{ 
	if (rhs.IsDefined()) 
		assign(rhs.begin(), rhs.end()); 
	else 
		assign(Undefined()); 
}

template <typename T>
void SA_Reference<T>::operator =(SA_Reference rhs) 
{ 
	dms_assert(!is_null() ); 
	if (rhs.IsDefined())
		assign(rhs.begin(), rhs.end()); 
	else
		assign(Undefined());
}

template <typename T>
void SA_Reference<T>::operator =(const typename sequence_traits<T>::container_type& rhs) 
{ 
	if (::IsDefined(rhs))
		assign(begin_ptr(rhs), end_ptr(rhs));
	else
		assign(Undefined());
}


template <typename T>
void SA_Reference<T>::swap(SA_Reference<T>& rhs)
{
	dms_assert(!    is_null() );
	dms_assert(!rhs.is_null() );
	if (m_Container == rhs.m_Container)
		omni::swap(*m_SeqPtr, *rhs.m_SeqPtr);
	else
	{
		SizeT n1 = size();
		SizeT n2 = rhs.size();
		if (n1 < n2)
		{
			swap_range(begin(), end(), rhs.begin());
			append(rhs.begin()+n1, rhs.end());
			rhs.erase(rhs.begin()+n1, rhs.end());
		}
		else
		{
			swap_range(begin(), begin()+n2, rhs.begin());
			rhs.append(begin()+n2, end());
			erase(begin()+n2, end());
		}
	}
}

template <typename T>
SA_Reference<T>::SA_Reference()
	:	m_SeqPtr() 
{
	dms_assert(is_null());
}

template <typename T>
SA_Reference<T>::SA_Reference(SequenceArrayType* container, seq_iterator seqPtr)
	:	m_SeqPtr(seqPtr)
	,	m_Container(container)
{
	assert_iter(m_SeqPtr != seq_iterator());
	dms_assert(m_Container);
};

//=======================================
// sequence_array
//=======================================

// sequence_array constructors

/* REMOVE ???*
template <typename T>
sequence_vector<T>::sequence_vector(const_iterator i, const_iterator e)
{
	this->Lock(true);
	auto srcDataSize = calcActualDataSize(i, e);

	this->Reset(e-i, srcDataSize);

	auto ri = this->m_Indices.begin();

	while (i != e)
	{
		if (i->IsDefined())
			allocateSequenceRange(ri, i->begin(), i->end());
		++ri;
		++i;
	}
	dms_assert(this->calcActualDataSize() == this->actual_data_size());
	dms_assert(srcDataSize == this->actual_data_size());
	dms_assert(!this->IsDirty());
}
*/

template <typename T>
sequence_array<T>::sequence_array(const sequence_array<T>& src, data_size_type expectedGrowth)
{
	Reset(0, 0 MG_DEBUG_ALLOCATOR_SRC_SA);
	assign(src, expectedGrowth);
}

template <typename T>
void sequence_array<T>::assign(const sequence_array<T>& src, data_size_type expectedGrowth)
{
	MGD_CHECKDATA(IsLocked());
	MGD_CHECKDATA(src.IsLocked());

	dms_assert(src.calcActualDataSize() == src.actual_data_size());

	reset(src.size(), src.actual_data_size() + expectedGrowth MG_DEBUG_ALLOCATOR_SRC_SA);

	typename base_type::const_seq_iterator
		i = src.m_Indices.begin(),
		e = src.m_Indices.end();
	typename base_type::seq_iterator
		ri = m_Indices.begin();
	const_data_iterator
		srcDataBegin = src.m_Values.begin();

	for (; i != e; ++ri, ++i)
		if(IsDefined(*i))
			allocateSequenceRange(ri, srcDataBegin + i->first, srcDataBegin + i->second );
		else
			Assign(*ri, Undefined() );

	dms_assert(actual_data_size  () == src.actual_data_size());
	dms_assert(calcActualDataSize() == actual_data_size());
	dms_assert(!IsDirty());
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
	dms_assert(first == last || (first.m_Container == this && last.m_Container == this));

	for (iterator i = first; i != last; ++i)
		abandon(i.m_SeqPtr->first, i.m_SeqPtr->second);

	m_Indices.erase(first.m_SeqPtr, last.m_SeqPtr);
}

template <typename T>
void sequence_array<T>::clear()
{
	erase(begin(), end());
	dms_assert(empty()); 
	m_Values.clear();
}

// sequence_array access control

template <typename T>
void sequence_array<T>::Open (seq_size_type nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	dms_assert(rwMode != dms_rw_mode::unspecified);

	m_Indices.Open(nrElem, rwMode == dms_rw_mode::write_only_all ? dms_rw_mode::write_only_mustzero : rwMode, isTmp, sfwa MG_DEBUG_ALLOCATOR_SRC_PARAM);
	try {
		m_Values.Open(UNDEFINED_VALUE(SizeT), rwMode, isTmp, sfwa MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}
	catch (...)
	{
		m_Indices.Close(); // rollback m_Indices.Open
		throw;
	}
	m_ActualDataSize = 0; // init when first lock is set
}

template <typename T>
void sequence_array<T>::Lock(dms_rw_mode rwMode)   // thread safe operation
{ 
	dms_assert(rwMode >= dms_rw_mode::read_only); // no undefined nor check_only

	SeqLock<seq_vector_t> lock(m_Indices, dms_must_keep(rwMode) ? rwMode : dms_rw_mode::write_only_mustzero); // may throw without any rolbackhere
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
		m_Values.clear();
	}
	lock.release();

	MG_DEBUGCODE( checkActualDataSize(); )
	MG_DEBUGCODE( checkConsecutiveness(); )

//	dms_assert(!IsDirty()); // last unlock should have packed sequences
}

template <typename T>
void sequence_array<T>::Reset (abstr_sequence_provider<T>* pr) 
{ 
	MGD_CHECKDATA(!m_Indices.IsLocked());
	MGD_CHECKDATA(!m_Values.IsLocked());

	m_Indices.Reset(pr ? pr->CloneForSeqs() : 0); 
	m_Values.Reset(pr); 
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
		dms_assert(!m_Values.IsAssigned());

		Reset(heap_sequence_provider<T>::CreateProvider());

		dms_assert(m_Indices.IsAssigned());
		dms_assert(m_Values.IsAssigned());

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
	  
//	dms_assert(m_Indices.size() == 0); // when else would this be called
	m_Indices.clear();
	dms_assert(m_Indices.size() == 0); // now we're certain
	m_Indices.resizeSO(nrSeqs, true MG_DEBUG_ALLOCATOR_SRC_PARAM);
	m_ActualDataSize = 0;

	MG_DEBUGCODE( checkActualDataSize() );
}

template <typename T>
void sequence_array<T>::Resize(typename data_vector_t::size_type expectedDataSize, size_type expectedSeqsSize, size_type nrSeqs MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	dms_assert(nrSeqs <= expectedSeqsSize); // PRECONDITION

	if (!m_Indices.IsAssigned())
		Reset(heap_sequence_provider<T>::CreateProvider());

	SeqLock<sequence_array> selfLock(*this, dms_rw_mode::read_write);

	data_reserve(expectedDataSize MG_DEBUG_ALLOCATOR_SRC_PARAM);

	dms_assert(m_Indices.size() <= nrSeqs);  // we don't allow cutting here

	reserve(expectedSeqsSize MG_DEBUG_ALLOCATOR_SRC_PARAM);

	// m_Seq.resize without shrinking
	m_Indices.resizeSO(nrSeqs, true MG_DEBUG_ALLOCATOR_SRC_PARAM);

	MG_DEBUGCODE( checkActualDataSize() );
}


template <typename T>
bool sequence_array<T>::allocate_data(data_size_type expectedGrowth MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	locked_sequence<T> oldData(false);
	return allocate_data(oldData, expectedGrowth MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename T>
bool sequence_array<T>::allocate_data(data_vector_t& oldData, typename data_vector_t::size_type expectedGrowth MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	dms_assert(oldData.empty());
	dms_assert(oldData.IsAssigned());
	dms_assert(oldData.IsHeapAllocated());
	dms_assert(m_Values.IsAssigned());
	MGD_CHECKDATA(oldData.IsLocked());
	MGD_CHECKDATA(m_Values.IsLocked());

	MG_DEBUGCODE( checkActualDataSize(); )

	dms_assert(oldData.empty()     );
	dms_assert(oldData.IsAssigned());
	dms_assert(oldData.IsHeapAllocated());

	// NYI: doe alleen m_Values en laat m_Indices ongemoeid
	if (!IsDirty())
	{
		if (expectedGrowth + m_Values.size() <= m_Values.capacity())
			return false;
		if (!m_Values.IsHeapAllocated())
		{
			data_reserve(actual_data_size() + expectedGrowth MG_DEBUG_ALLOCATOR_SRC_PARAM);
			return false;
		}
	}

	if (m_Values.IsHeapAllocated())
		m_Values.swap(oldData);
	else
	{
		dms_assert(IsDirty());
		oldData.appendRange(m_Values.begin(), m_Values.end());
		m_Values.clear();
	}

	dms_assert(m_Values.empty()    );
	dms_assert(m_Values.size() == 0);
	dms_assert(m_Values.IsAssigned());

	data_reserve(actual_data_size() + expectedGrowth MG_DEBUG_ALLOCATOR_SRC_PARAM);

	typename base_type::seq_vector_t::iterator
		i = m_Indices.begin(),
		e = m_Indices.end();

	typename base_type::data_size_type                dataEnd  = 0;
	typename base_type::data_vector_t::const_iterator oldBegin = oldData.begin();
	while (i!=e) 
	{
		data_size_type size = i->size();
		dms_assert(size || !IsDefined(*i) || ((!i->first) && (!i->second)));
		if (size)
		{
			data_size_type dataCurr = dataEnd;
			m_Values.appendRange(oldBegin + i->first, oldBegin + i->second);
			dataEnd += size;
			dms_assert(dataEnd == m_Values.size());
			*i = seq_t(dataCurr, dataEnd);
		}
		++i;
	}

	MG_DEBUGCODE( checkActualDataSize(); )
	MG_DEBUGCODE( checkConsecutiveness(); )

//	dms_assert(!IsDirty());
	dms_assert(m_Values.size() +expectedGrowth == m_Values.capacity());
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
template <typename Initializer> void sequence_array<T>::allocateSequence(typename base_type::seq_iterator seqPtr, data_size_type newSize, Initializer&& initFunc )
{
	MGD_CHECKDATA(IsLocked());
	assert_iter(seqPtr!=seq_iterator());
	dms_assert(seq_size_type(seqPtr - m_Indices.begin()) < m_Indices.size());
	dms_assert(m_ActualDataSize >= seqPtr->size());

	data_size_type                          oldSize = seqPtr->size();
	typename data_vector_t::difference_type delta   =  newSize - oldSize;

	if (delta > 0)
	{
		bool atEnd = (seqPtr->second == m_Values.size());
		if ((atEnd ? delta : newSize) <= m_Values.capacity() - m_Values.size())
		{
			if (oldSize && !atEnd)
			{
				appendRange(m_Values.begin()+seqPtr->first, m_Values.begin()+seqPtr->second); // copy oldSize elemeents from original 
				abandon    (seqPtr->first, seqPtr->second); // and abandon that area
			}
			appendInitializer(delta, initFunc);
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

			bool dataWasMoved = allocate_data(oldData, Max<data_size_type>(m_ActualDataSize+2*oldSize, newSize) MG_DEBUG_ALLOCATOR_SRC_SA);
			dms_assert(!IsDirty());
			if (oldSize)
			{
				if (dataWasMoved)
					appendRange(oldData.begin() + oldFirst, oldData.begin() + oldSecond);
				else
				{
					dms_assert(oldFirst == m_Values.size());
					appendInitializer(oldSize, [](auto i, auto e) {}); // skip old range that was abandoned but is still there.
					dms_assert(oldSecond == m_Values.size());
				}
			}
			appendInitializer(delta, initFunc);

			*seqPtr = seq_t(m_Values.size()-newSize, m_Values.size());

			MG_DEBUGCODE( checkActualDataSize() );
		}
	}
	else
		cut_seq(seqPtr, newSize);

	dms_assert(newSize == seqPtr->size());
}

template <typename T>
void sequence_array<T>::allocateSequenceRange(typename base_type::seq_iterator seqPtr, const_data_iterator first, const_data_iterator last)
{
	MGD_CHECKDATA(IsLocked());

	assert_iter(seqPtr!=seq_iterator());
	dms_assert(seq_size_type(seqPtr - m_Indices.begin()) < m_Indices.size());

	data_size_type
		newSize = last - first,
		oldSize = seqPtr->size();
	dms_assert(m_ActualDataSize >= oldSize);

	typename data_vector_t::difference_type delta =  newSize - oldSize;

	if (delta > 0)
	{
		dms_assert(newSize);
		bool atEnd = (seqPtr->second == m_Values.size());
		if ((atEnd ? newSize - oldSize : newSize) <= m_Values.capacity() - m_Values.size())
		{
			if (!atEnd)
			{
				appendRange(first, last);                      // insert new range
				abandon(seqPtr->first, seqPtr->second);        // and abandon old sequence
			}
			else
			{
				const_data_iterator copyArea = first+oldSize;
				std::copy(first, copyArea, m_Values.begin() + seqPtr->first);   // overwrite first part
				appendRange(copyArea, last);                   // insert second part
			}
			*seqPtr = seq_t(m_Values.size()-newSize, m_Values.size());
		}
		else
		{
			locked_sequence<T> oldData(false);

			abandon(seqPtr->first, seqPtr->second);
			*seqPtr = seq_t();

			bool dataWasMoved = allocate_data(oldData, Max<data_size_type>(m_ActualDataSize+2*oldSize, newSize) MG_DEBUG_ALLOCATOR_SRC_SA);
			appendRange(first, last);

			*seqPtr = seq_t(m_Values.size()-newSize, m_Values.size());

			dms_assert(m_ActualDataSize == calcActualDataSize());

//			MG_DEBUGCODE( checkActualDataSize() );
		}
	}
	else // delta <= 0
	{
		fast_copy(first, last, m_Values.begin() + seqPtr->first);
		cut_seq(seqPtr, newSize);
	}
	dms_assert(newSize == seqPtr->size());
}

template <typename T>
void sequence_array<T>::cut_seq(typename base_type::seq_iterator seqPtr, typename data_vector_t::size_type newSize)
{
	MGD_CHECKDATA(IsLocked());

	assert_iter(seqPtr != seq_iterator());
	dms_assert(SizeT(seqPtr - m_Indices.begin()) < m_Indices.size());

	dms_assert(m_ActualDataSize >= seqPtr->size());

	size_type oldSize = seqPtr->size();
	dms_assert(newSize <= oldSize);
	
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

	dms_assert(newSize == seqPtr->size());
}

template <typename T>
void sequence_array<T>::abandon(data_size_type first, data_size_type last)
{
	data_size_type n = last - first;
	dms_assert( n <= m_ActualDataSize);
	m_ActualDataSize -= n;
	
	destroy_range(m_Values.begin()+first, m_Values.begin() + last);
}

template <typename T>
template <typename Initializer>
void sequence_array<T>::appendInitializer(size_type n, Initializer&& initFunc)
{
	dms_assert(m_Values.capacity() - m_Values.size() >= n);

	m_Values.appendInitializer(n, initFunc);
	m_ActualDataSize += n;
}

//	template <typename InputIterator>
//	void appendRange(InputIterator first, InputIterator last)
template <typename T>
void sequence_array<T>::appendRange(const_data_iterator first, const_data_iterator last)
{
	dms_assert((m_Values.capacity() - m_Values.size()) >= data_size_type(last - first));

	m_Values.appendRange(first, last);
	m_ActualDataSize += (last - first);
}

template <typename T>
void sequence_array<T>::StreamOut(BinaryOutStream& ar) const
{
	if (CanWrite())
		const_cast<sequence_array<T>*>(this)->allocate_data(0 MG_DEBUG_ALLOCATOR_SRC_SA); // we only want to write allocated elements, so reallocate if dirty

	dms_assert(! IsDirty() );

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
	dms_assert(m_Indices.size() == s);
	dms_assert(m_Indices.begin() + s == m_Indices.end());
	for (SizeT i = 0; i != s; ++i)
		ar >> m_Indices[i].first >> m_Indices[i].second;

	ar >> m_Values;
	m_ActualDataSize = m_Values.size();

	MG_DEBUGCODE( checkActualDataSize(); );
}

#if defined(MG_DEBUG)

template <typename T>
void sequence_array<T>::checkActualDataSize() const
{
	MGD_CHECKDATA(IsLocked());
	SizeT as = calcActualDataSize(); // O(#elements)
	dms_assert(as == m_ActualDataSize);
}

template <typename T>
void sequence_array<T>::checkConsecutiveness() const
{
	MGD_CHECKDATA(IsLocked());
	for (const auto& indexPair : m_Indices)
	{
		dms_assert((indexPair.first  <= indexPair.second) || !IsDefined(indexPair));
		dms_assert((indexPair.second <= m_ActualDataSize) || !IsDefined(indexPair));
	}
}

#endif

//=======================================
// SequenceArray<T> instantiation
//=======================================

#include "geo/Geometry.h"
#include "geo/Point.h"
#include "set/Token.h"

template sequence_array<char>;
template sequence_array<int>;
template sequence_array<DPoint>;
template sequence_array<FPoint>;
//template void sequence_array<TokenID>::Reset(size_type nrSeqs, data_vector_t::size_type expectedDataSize);
template void sequence_array<TokenID>::allocateSequenceRange(typename base_type::seq_iterator seqPtr, const_data_iterator first, const_data_iterator last );
template void sequence_array<TokenID>::Reset(size_type nrSeqs, data_vector_t::size_type expectedDataSize MG_DEBUG_ALLOCATOR_SRC_ARG);

template SA_Reference<char>;
template SA_Reference<int>;
template SA_Reference<DPoint>;
template SA_Reference<FPoint>;
template SA_Reference<TokenID>::SA_Reference(SequenceArrayType* container, seq_iterator seqPtr);
template SA_ConstReference<TokenID>::SA_ConstReference(const SequenceArrayType* sa, const_seq_iterator cSeqPtr);

template SA_ConstReference<FPoint>;
template SA_ConstReference<char>;

#if defined (DMS_TM_HAS_INT_SEQ)

template sequence_array<UInt64>;
template sequence_array<UInt32>;
template sequence_array<UInt16>;
template sequence_array<UInt8>;

template sequence_array<Int64>;
template sequence_array<Int32>;
template sequence_array<Int16>;
template sequence_array<Int8>;

template sequence_array<Float64>;
template sequence_array<Float32>;

template sequence_array<SPoint>;
template sequence_array<IPoint>;
template sequence_array<WPoint>;
template sequence_array<UPoint>;

template SA_Reference<UInt64>;
template SA_Reference<UInt32>;
template SA_Reference<UInt16>;
template SA_Reference<UInt8>;

template SA_Reference<Int64>;
template SA_Reference<Int32>;
template SA_Reference<Int16>;
template SA_Reference<Int8>;

template SA_Reference<Float64>;
template SA_Reference<Float32>;

template SA_Reference<SPoint>;
template SA_Reference<IPoint>;
template SA_Reference<WPoint>;
template SA_Reference<UPoint>;

template SA_ConstReference<UInt64>;
template SA_ConstReference<UInt32>;
template SA_ConstReference<UInt16>;
template SA_ConstReference<UInt8>;

template SA_ConstReference<Int64>;
template SA_ConstReference<Int32>;
template SA_ConstReference<Int16>;
template SA_ConstReference<Int8>;

template SA_ConstReference<Float64>;
template SA_ConstReference<Float32>;

template SA_ConstReference<SPoint>;
template SA_ConstReference<IPoint>;
template SA_ConstReference<WPoint>;
template SA_ConstReference<UPoint>;

template SA_ConstReference<FPoint>;
template SA_ConstReference<DPoint>;

#endif
