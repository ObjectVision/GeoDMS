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

/***************************************************************************

sequence_array<T> is (nearly) compatible with std::vector<std::vector<T> >
but has a faster memory management strategy.

Rationale:

I often worked with vectors of sequences, two examples:

1. typedef std::pair<double, double> dpoint;
   typedef std::vector<dpoint> dpolygon;   // point sequence
   typedef std::vector<dpolygon> dpolygon_array; // point sequence array

2. typedef std::basic_string<char> String; // char sequence
   typedef std::vector<String> StringArray; // char sequence array

When working with vector-based sequence arrays (as above),
the memory management strategy is not efficient
due to the many allocations/deallocations of the individual sequences.
Especially the destruction of such vector based sequence arrays
often takes unneccesary and annoying amount of time.

The memory allocation of sequences by sequence_array<T> 
offers two advantages above using a vector of vectors:
1. A sequence_array specific memory pool for the sequences from which 
   sequences are allocated to avoid individual heap (de)allocations
	for each sequence
2. Not providing synchronization on such memory allocation
   (note that common implementations of vector also don't provide
	thread safety anyway

sequence_array<T> uses an internal
    data_vector_t m_Values;
for sequential storage of sequences of T (the memory pool)
and an additional
    std::vector<std::pair<data_vector_t::iterator, data_vector_t::iterator> > m_Indices;
for identifying allocated sequences.

data_vector_t is defined as std::vector<T> for elementary types T and simple structures thereof,
and can be defined as sequece_array<U> when T is a collection of U in order to support
arrays of arrays of sequcences of U (three dimensional) and higher dimensional equivalents.

sequence_array offers:
	typedef iterator, 
	typedef const_iterator, 
	typedef reference,
	typedef const_reference
with interfaces nearly compatibe with
	std::vector<T>*, 
	const std::vector<T>*, 
	vector<T>&, resp. 
	const vector<T>&.
(the sequences however don't have a reserve() member function,
only a resize() resulting in reallocation in m_Values if they grow
AND [are not the last sequence in m_Values
or insufficent reserved space after m_Values.end() is available] )

The storage m_Values grows with the familiar doubling strategy
as sequences are inserted or re-allocated due to calls to their resize() mf.
m_Indices is adjusted when m_Values has grown by re-allocation.

Destruction of a sequence_array only requires two calls to delete[].
(assuming that T::~T() doesn't do additional things).

sequence_array minimizes the pollution of the heap with varying sized
arrays.

Users benefit most when the total size and number of the sequences is known in advance, 
and provided to a sequence_array by the member-functions reserve() and data_reserve().

At this point, my implementation is not ready for inclusion in boost for the following reasons:
- it is dependent on other headers in my pre-boost facilities library
- it is not yet in boost style
- lack of required peer review in order to get sufficient quality and general purpose usability.
(however, it does work fine after having solved some problems in the DMS, our product).
- open design issues

I specifically would like to hear from you on the following design issues:
- I am considering to change the definition of the above mentioned m_Indices to:
    std::vector<std::pair<size_t, size_t> > m_Indices;
where m_Indices[i].first and second are offsets of the start and end of sequence i relative to m_Values.begin().
This avoids adjustment when m_Values grows, for a small const access-time penalty.
- At this point, the destruction of abandoned elements in m_Values
 (= elements not being part anymore of a reallocated sequence),
 is deferrred to the destruction or re-allocation of m_Values.
- the last sequence in m_Values now gets special treatment when it is resized and sufficient reserved
space is available after m_Values.end(). This special treatment saves an order of complexity
when the last sequence often grows, but adds a constant cost to the re-allocation administration.
- generalization to sequece_array arrays (3 levels deep) or higher dimensions.

*/
#pragma once

#if !defined(__GEO_SEQUENCE_ARRAY_H)
#define __GEO_SEQUENCE_ARRAY_H

#include "dbg/Check.h"
#include "geo/IndexRange.h"
#include "geo/SequenceTraits.h"
//#include "geo/SeqVector.h"
#include "ptr/WeakPtr.h"
#include "ptr/OwningPtrSizedArray.h"
#include "set/Token.h"

//=======================================
// forward decl
//=======================================

template <typename T> struct sequence_array_index;

//=======================================
// container generator
//=======================================
#include "mem/SequenceObj.h"

struct sequence_generator {
	template <typename T>
	struct make { 
		typedef typename sequence_traits<T>::polymorph_vec_t type; 
	};
};

//=======================================
// SequenceArray_Base: defines some common types
//=======================================

template <typename T>
struct SequenceArray_Base
{
	typedef sequence_array<T>  SequenceArrayType; 
	typedef sequence_generator Generator;

	// data_vector_t is usally std::vector<T>.
	// It is sequence_array<U> if T == std::vector<U> in order to support n dimensional structures where n>2
	// sequence_traits<T> is defined elsewhere. 
	// You can define it as std::vector<T> when T is an elementary type or composed thereof.

	typedef typename Generator::make<T>::type           data_vector_t; 
	typedef typename data_vector_t::size_type           data_size_type; 

	typedef typename data_vector_t::iterator            data_iterator;        // not compatible with T*
	typedef typename data_vector_t::const_iterator      const_data_iterator;  // not compatible with const T*
	typedef typename data_vector_t::reference           data_reference;       // compatible with T&
	typedef typename data_vector_t::const_reference     const_data_reference; // compatible with const T&

	typedef          IndexRange<data_size_type>         seq_t;

	typedef typename Generator::make<seq_t >::type      seq_vector_t;
	typedef typename seq_vector_t::iterator             seq_iterator;
	typedef typename seq_vector_t::const_iterator       const_seq_iterator;

	using sequence_value_type = typename sequence_traits<T>::container_type;
};

//=======================================
// SequenceArray<T>::const_reference
//=======================================
// const_reference refers to a const sequence of T elements.
// it has (most of) the interface of std::vector<T>
// we don't need the pointer to the containing sequence_array as we don't reallocate const sequences

template <typename T>
struct SA_ConstReference : private SequenceArray_Base<T>
{
	using typename SequenceArray_Base<T>::data_vector_t;
	using typename SequenceArray_Base<T>::SequenceArrayType;
	using typename SequenceArray_Base<T>::const_seq_iterator;
	typedef typename SequenceArray_Base<T>::const_data_iterator  const_iterator; // not compatible with const T*
	typedef typename SequenceArray_Base<T>::const_data_reference const_reference;// compatible with const T&

	typedef typename data_vector_t::size_type       size_type; 
	typedef typename data_vector_t::difference_type difference_type;

	using value_type = const T;
	using typename SequenceArray_Base<T>::sequence_value_type;

	// sequence properties
	size_type      size () const { dms_assert(!is_null()); return m_CSeqPtr->size();  }
	bool           empty() const { dms_assert(!is_null()); return m_CSeqPtr->empty(); }
	bool       IsDefined() const { dms_assert(!is_null()); return ::IsDefined(*m_CSeqPtr); }

	// Element ro access
	const_iterator begin() const { dms_assert(!is_null()); dms_assert(m_CSeqPtr->first <= m_Container->m_Values.size() || !IsDefined()); return m_Container->m_Values.begin() + m_CSeqPtr->first;  }
	const_iterator end  () const { dms_assert(!is_null()); dms_assert(m_CSeqPtr->second<= m_Container->m_Values.size() || !IsDefined()); return m_Container->m_Values.begin() + m_CSeqPtr->second; }


	const_reference operator[](size_type i) const { dms_assert(i<size()); return begin()[i]; }
	const_reference front()                 const { dms_assert(0<size()); return begin()[0]; }
	const_reference back()                  const { dms_assert(0<size()); return end()[-1]; }

//	compare contents
	RTC_CALL bool operator ==(SA_ConstReference<T> rhs) const;
	RTC_CALL bool operator < (SA_ConstReference<T> rhs) const;
	RTC_CALL bool operator ==(SA_Reference<T> rhs) const;
	RTC_CALL bool operator < (SA_Reference<T> rhs) const;
	RTC_CALL bool operator ==(const sequence_value_type& rhs) const;
	RTC_CALL bool operator < (const sequence_value_type& rhs) const;

	SA_ConstIterator<T> makePtr() const { return SA_ConstIterator<T>(m_Container, m_CSeqPtr); }

	operator sequence_value_type () const
	{
		return sequence_value_type(begin(), end());
	}

private:
	friend struct SA_Reference<T>;
	friend struct SA_ConstIterator<T>;
	friend struct SA_Iterator<T>;
	friend struct sequence_array<T>;

	// ctor usally called from const_reference sequence_array<T>::operator [](size_type) const;
	RTC_CALL SA_ConstReference(const SequenceArrayType* sa, const_seq_iterator cSeqPtr);

	// Forbidden to call directly; can be constructed as part of a default SA_ConstIterator
	SA_ConstReference() : m_CSeqPtr() { dms_assert(is_null()); }

	bool is_null() const 
	{
		return m_Container.is_null(); 
	}

	void operator =(const SA_ConstReference& rhs);

	WeakPtr<const SequenceArrayType> m_Container;
	const_seq_iterator               m_CSeqPtr;
};

template <typename T, typename A>
bool operator < (const std::vector<T, A>& lhs, SA_ConstReference<T> rhs) noexcept
{
	return IsDefined(lhs)
		? rhs.IsDefined()
		? lex_compare(lhs.begin(), lhs.end(), cbegin_ptr(rhs), cend_ptr(rhs))
		: false
		: rhs.IsDefined();
}

inline bool operator < (const SharedStr& lhs, SA_ConstReference<char> rhs) noexcept
{
	return lhs.IsDefined()
		? rhs.IsDefined()
		? lex_compare(lhs.begin(), lhs.end(), cbegin_ptr(rhs), cend_ptr(rhs))
		: false
		: rhs.IsDefined();
}


//=======================================
// SequenceArray<T>::reference
//=======================================
// we do need the pointer to the containing sequence_array here to support reallocating the sequence
// as a result of resize() or insert();

template <typename T>
struct SA_Reference : private SequenceArray_Base<T>
{
	using typename SequenceArray_Base<T>::data_vector_t;
	using typename SequenceArray_Base<T>::SequenceArrayType;
	using typename SequenceArray_Base<T>::seq_iterator;

	typedef typename SequenceArray_Base<T>::const_data_iterator            const_iterator; // compatible with const T*
	typedef typename SequenceArray_Base<T>::data_iterator                  iterator;       // compatible with T*

	typedef typename SequenceArray_Base<T>::const_data_reference           const_reference;// compatible with const T&
	typedef typename SequenceArray_Base<T>::data_reference                 reference;      // compatible with T&

	typedef typename data_vector_t::size_type       size_type;
	typedef typename data_vector_t::difference_type difference_type;

	using value_type = const T;
	using typename SequenceArray_Base<T>::sequence_value_type;

	operator SA_ConstReference<T>() const { return SA_ConstReference<T>(m_Container, m_SeqPtr); }

	// sequence properties
	size_type      size() const { dms_assert(!is_null()); return m_SeqPtr->size(); }
	bool           empty() const { dms_assert(!is_null()); return m_SeqPtr->empty(); }
	bool       IsDefined() const { dms_assert(!is_null()); return ::IsDefined(*m_SeqPtr); }

	// Element rw access
	iterator begin() { dms_assert(!is_null()); dms_assert(m_SeqPtr->first <= m_Container->m_Values.size()); return m_Container->m_Values.begin() + m_SeqPtr->first; }
	iterator end() { dms_assert(!is_null()); dms_assert(m_SeqPtr->first <= m_Container->m_Values.size()); return m_Container->m_Values.begin() + m_SeqPtr->second; }

	reference operator[](size_type i) { dms_assert(i < size()); return begin()[i]; }
	reference front()                 { dms_assert(0 < size()); return begin()[0]; }
	reference back()                  { dms_assert(0 < size()); return end()[-1]; }

	// Element ro access
	const_iterator begin () const { dms_assert(!is_null()); return m_Container->m_Values.begin() + m_SeqPtr->first; }
	const_iterator end   () const { dms_assert(!is_null()); return m_Container->m_Values.begin() + m_SeqPtr->second; }
	const_iterator cbegin() const { return begin(); }
	const_iterator cend  () const { return end(); }

	const_reference operator[](size_type i) const { dms_assert(i < size()); return begin()[i]; }
	const_reference front()                 const { dms_assert(0 < size()); return begin()[0]; }
	const_reference back()                  const { dms_assert(0 < size()); return end()[-1]; }

//	compare contents
	RTC_CALL bool operator ==(SA_ConstReference<T> rhs) const;
	RTC_CALL bool operator < (SA_ConstReference<T> rhs) const;
	RTC_CALL bool operator ==(SA_Reference<T> rhs) const;
	RTC_CALL bool operator < (SA_Reference<T> rhs) const;
	RTC_CALL bool operator ==(const sequence_value_type& rhs) const;
	RTC_CALL bool operator < (const sequence_value_type& rhs) const;

	RTC_CALL void assign(const_iterator first, const_iterator last);
	RTC_CALL void assign(Undefined);

	iterator insert(iterator startPtr, const T& value)
	{
		size_type startPos = startPtr - begin();
		dms_assert(startPos <= size());

		resize_uninitialized(size() + 1);
		startPtr = begin() + startPos; // can be relocated due to resize()
		dms_assert(startPtr < end());

		std::copy_backward(startPtr, end() - 1, startPtr + 1);
		*startPtr = value;
		return startPtr;
	}

	template <typename InIter>
	void insert(iterator startPtr, InIter first, InIter last)
	{
		size_type startPos = startPtr - begin();
		dms_assert(startPos <= size());

		size_type n = std::distance(first, last);

		resize_uninitialized(size() + n);
		startPtr = begin() + startPos;
		dms_assert(startPtr <= end() - n);

		std::copy_backward(startPtr, end() - n, startPtr + n);
		fast_copy(first, last, startPtr);
	}

	template <typename InIter>
	void append(InIter first, InIter last)
	{
		size_type n = last - first; // std::distance(first, last);

		size_type oldSize = size();
		resize_uninitialized(oldSize + n);

		iterator startPtr = begin() + oldSize;
		dms_assert(startPtr + n == end());

		fast_copy(first, last, startPtr);
	}


	RTC_CALL void erase(iterator first, iterator last);
	RTC_CALL void clear();
	RTC_CALL void resize(size_type seqSize, const value_type& zero);
	RTC_CALL void resize_uninitialized(size_type seqSize);
	RTC_CALL void reserve(size_type seqSize)
	{
		SizeT currSize = size();
		if (currSize < seqSize)
		{
			resize_uninitialized(seqSize);
			erase(begin() + currSize, end());
		}
	}
	template <typename ...Args>
	void emplace_back(Args&& ...args)
	{
		push_back();
		back() = value_type(std::forward<Args>(args)...);
	}
	RTC_CALL void push_back(const value_type& value);
	RTC_CALL void push_back();

	RTC_CALL void operator =(SA_ConstReference<T> rhs);
	RTC_CALL void operator =(SA_Reference rhs);
	RTC_CALL void operator =(const typename sequence_traits<T>::container_type& rhs);

	RTC_CALL void swap(SA_Reference<T>& rhs);

//	SA_Iterator<T> operator &() const { return SA_Iterator<T>(m_Container, m_SeqPtr); }
	SA_Iterator<T> makePtr() const { return SA_Iterator<T>(m_Container, m_SeqPtr); }

	operator sequence_value_type () const
	{
		return sequence_value_type(begin(), end());
	}

private:
	friend typename SequenceArrayType;
	friend struct SA_Iterator<T>;
	friend struct sequence_array_index<T>; 

	RTC_CALL SA_Reference();
	RTC_CALL SA_Reference(SequenceArrayType* container, seq_iterator seqPtr);

	bool is_null() const 
	{ 
		assert_iter((m_SeqPtr == seq_iterator()) == (m_Container.is_null()));
		return m_Container.is_null();
	}

	WeakPtr<SequenceArrayType> m_Container;
	seq_iterator               m_SeqPtr;
};

//=======================================
// SequenceArray<T>::const_iterator
//=======================================

template <typename T>
struct SA_ConstIterator: std::iterator<std::random_access_iterator_tag
	,	typename sequence_traits<T>::container_type
	,	dms::diff_type
	,	SA_ConstReference<T>*
	,	SA_ConstReference<T>
	>
	,	private SA_ConstReference<T>
{
	using SequenceArrayType = typename SequenceArray_Base<T>::SequenceArrayType;
	using const_seq_iterator = typename SequenceArray_Base<T>::const_seq_iterator;

	using SA_ConstReference<T>::m_CSeqPtr;
	using SA_ConstReference<T>::m_Container;

	// redefine boost::iterator typedefs to avoid ambiguity with the SA_ConstReference<T> base class.
	typedef typename sequence_traits<T>::container_type value_type;
	typedef SA_ConstReference<T>*                       pointer; 
	typedef SA_ConstReference<T>                        reference;
	typedef dms::diff_type                              difference_type;

	SA_ConstIterator() {}

	// return pointer to class object
	const SA_ConstReference<T>* operator->() const { assert_iter(!is_null()); return this; }

	// dereference
	const SA_ConstReference<T>& operator *() const { assert_iter(!is_null()); return *this; }

	SA_ConstReference<T> operator[](difference_type i) const { assert_iter(!is_null()); return SA_ConstReference<T>(m_Container, m_CSeqPtr + i); }

	SA_ConstIterator operator +(difference_type displacement) const
	{
		assert_iter(!is_null());
		SA_ConstIterator tmp = *this;
		tmp.m_CSeqPtr += displacement;
		return tmp;
	}

	SA_ConstIterator operator -(difference_type displacement) const { return *this + - displacement; }
	difference_type operator -(SA_ConstIterator rhs) const { assert_iter(is_null() == rhs.is_null()); dms_assert(m_Container == rhs.m_Container); return m_CSeqPtr - rhs.m_CSeqPtr; }

	void operator += (difference_type displacement) { assert_iter(!is_null()); m_CSeqPtr += displacement; }
	void operator -= (difference_type displacement) { assert_iter(!is_null()); m_CSeqPtr -= displacement; }
	
	SA_ConstIterator& operator ++()    { assert_iter(!is_null()); ++m_CSeqPtr; return *this; } // prefix; return new value
	SA_ConstIterator& operator --()    { assert_iter(!is_null()); --m_CSeqPtr; return *this; } // prefix; return new value
	SA_ConstIterator  operator ++(int) { assert_iter(!is_null()); SA_ConstIterator tmp = *this; ++m_CSeqPtr; return tmp; } // postfix; return old value
	SA_ConstIterator  operator --(int) { assert_iter(!is_null()); SA_ConstIterator tmp = *this; --m_CSeqPtr; return tmp; } // postfix; return old value

	void operator =(const SA_ConstIterator& rhs) { m_Container = rhs.m_Container; m_CSeqPtr = rhs.m_CSeqPtr; }

	bool operator ==(const SA_ConstIterator& rhs) const
	{ 
		dms_assert(m_Container == rhs.m_Container || m_CSeqPtr != rhs.m_CSeqPtr);
		return m_CSeqPtr == rhs.m_CSeqPtr;
	}
	bool operator !=(const SA_ConstIterator& rhs) const
	{ 
		dms_assert(m_Container == rhs.m_Container || m_CSeqPtr != rhs.m_CSeqPtr);
		return m_CSeqPtr != rhs.m_CSeqPtr;
	}
	bool operator <(const SA_ConstIterator& rhs) const
	{ 
		dms_assert(m_Container == rhs.m_Container || m_CSeqPtr != rhs.m_CSeqPtr);
		return m_CSeqPtr < rhs.m_CSeqPtr;
	}
	bool operator <=(SA_ConstIterator rhs) const { return !(rhs < *this); }

private:
	friend struct SA_Iterator<T>;
	friend struct sequence_array<T>;
	friend struct SA_ConstReference<T>;

	SA_ConstIterator(const SequenceArrayType* container, const_seq_iterator seqPtr)
		: SA_ConstReference<T>(container, seqPtr) 
	{
		assert_iter(!is_null());
	}
};

//=======================================
// SequenceArray<T>::iterator
//=======================================

template <typename T>
struct SA_Iterator: std::iterator<std::random_access_iterator_tag
	,	typename sequence_traits<T>::container_type
	,	dms::diff_type
	,	SA_Reference<T>*
	,	SA_Reference<T>
	>
	,	private SA_Reference<T>
{
	using SequenceArrayType = typename SequenceArray_Base<T>::SequenceArrayType;
	using seq_iterator = typename SequenceArray_Base<T>::seq_iterator;

	using SA_Reference<T>::m_SeqPtr;
	using SA_Reference<T>::m_Container;
	using SA_Reference<T>::is_null;

	// redefine std::iterator typedefs to avoid ambiguity with the SA_Reference<T> base class.
	typedef typename sequence_traits<T>::container_type value_type;
	typedef SA_Reference<T>*                            pointer; 
	typedef SA_Reference<T>                             reference;
	typedef dms::diff_type                              difference_type;

	SA_Iterator() {}

	explicit operator bool () const { return !is_null(); }

	// return pointer to class object
	SA_Reference<T>* operator->() const { dms_assert(IsInRange(0)); return const_cast<SA_Iterator*>(this); }
//	      SA_Reference<T>* operator->()       { dms_assert(IsInRange(0)); return this; }

	// dereference
//	SA_ConstReference<T> operator *() const { dms_assert(IsInRange(0)); return SA_ConstReference<T>(m_Container, m_SeqPtr); }
	SA_Reference<T>& operator *() const { dms_assert(IsInRange(0)); return *const_cast<SA_Iterator*>(this); }
	
	SA_Reference<T> operator[](difference_type i) const { dms_assert(IsInRange(i)); return SA_Reference<T>(m_Container, m_SeqPtr + i); }

	operator SA_ConstIterator<T>() const { return SA_ConstIterator<T>(m_Container, m_SeqPtr); }

	SA_Iterator operator +(difference_type displacement) const { assert_iter(!is_null()); return SA_Iterator(m_Container, m_SeqPtr + displacement); }
	SA_Iterator operator -(difference_type displacement) const { return *this + - displacement; }
	difference_type operator -(SA_Iterator rhs) const { assert_iter(is_null() == rhs.is_null()); dms_assert(m_Container == rhs.m_Container); return m_SeqPtr - rhs.m_SeqPtr; }

	void operator += (difference_type displacement) { assert_iter(!is_null()); m_SeqPtr += displacement; }
	void operator -= (difference_type displacement) { assert_iter(!is_null()); m_SeqPtr -= displacement; }

	SA_Iterator& operator ++()    { assert_iter(!is_null()); ++m_SeqPtr; return *this; } // prefix; return new value
	SA_Iterator& operator --()    { assert_iter(!is_null()); --m_SeqPtr; return *this; } // prefix; return new value
	SA_Iterator  operator ++(int) { assert_iter(!is_null()); SA_Iterator tmp = *this; ++m_SeqPtr; return tmp; } // postfix; return old value
	SA_Iterator  operator --(int) { assert_iter(!is_null()); SA_Iterator tmp = *this; --m_SeqPtr; return tmp; } // postfix; return old value

	void operator =(const SA_Iterator& rhs) { m_Container = rhs.m_Container; m_SeqPtr    = rhs.m_SeqPtr; }

	bool operator ==(const SA_Iterator& rhs) const
	{ 
		dms_assert(m_Container == rhs.m_Container || m_SeqPtr != rhs.m_SeqPtr);
		return m_SeqPtr == rhs.m_SeqPtr;
	}
	bool operator !=(const SA_Iterator& rhs) const
	{ 
		dms_assert(m_Container == rhs.m_Container || m_SeqPtr != rhs.m_SeqPtr);
		return m_SeqPtr != rhs.m_SeqPtr;
	}
	bool operator <(const SA_Iterator& rhs) const
	{ 
		dms_assert(m_Container == rhs.m_Container || m_SeqPtr != rhs.m_SeqPtr);
		return m_SeqPtr < rhs.m_SeqPtr;
	}
	bool operator <=(const SA_Iterator& rhs) const { return !(rhs < *this); }

private:
	SA_Iterator(SequenceArrayType* container, seq_iterator pairPtr)
		:	SA_Reference<T>(container, pairPtr)
	{
		assert_iter(!is_null());
	}
	
	bool IsInRange(difference_type i) const
	{
		assert_iter(!is_null());
		return  
			(m_SeqPtr + i <  m_Container->m_Indices.end()   )
		&&	(m_SeqPtr + i >= m_Container->m_Indices.begin() );


	}
	friend SequenceArrayType;
	friend struct sequence_array_index<T>; 
	friend struct SA_Reference<T>;
};

inline size_t payload(const SharedStr& v) { return v.ssize(); }
template <typename Seq> size_t payload(const Seq& v) { return v.size(); }

template <typename CIter>
std::size_t CalcActualDataSize(CIter first, CIter last)
{
	std::size_t result = 0;
	for (; first != last; ++first)
		result += payload(*first);
	return result;
}

//=======================================
// sequence_array
//=======================================

template <typename T>
struct sequence_array : private SequenceArray_Base<T>
{
	using base_type = SequenceArray_Base<T>;
	using typename base_type::data_vector_t;
	using typename base_type::seq_vector_t;
//	using typename base_type::SequenceArrayType;
	using typename base_type::seq_t;

private:
	typedef typename data_vector_t::const_iterator   const_data_iterator;  // compatible with const T*
	typedef typename data_vector_t::iterator         data_iterator;        // compatible with T*

	typedef typename seq_vector_t::size_type        seq_size_type; 
	typedef typename seq_vector_t::difference_type  seq_difference_type;

public:
	typedef typename data_vector_t::size_type       data_size_type; 
	typedef typename data_vector_t::difference_type data_difference_type;

	typedef seq_size_type       size_type;
	typedef seq_difference_type difference_type;

	typedef struct SA_ConstReference<T>  const_reference;
	typedef struct SA_Reference     <T>  reference;
	typedef struct SA_ConstReference<T>* const_pointer;
	typedef struct SA_Reference     <T>* pointer;
	typedef struct SA_ConstIterator <T>  const_iterator;
	typedef struct SA_Iterator      <T>  iterator;
	typedef struct SA_ConstReference<T>  value_type;

	//=======================================
	// sequence_array constructors
	//=======================================

	sequence_array(): m_ActualDataSize(0) {}
	sequence_array(size_type n MG_DEBUG_ALLOCATOR_SRC_ARG): m_ActualDataSize(0)  { Reset(n, 0 MG_DEBUG_ALLOCATOR_SRC_PARAM); }
	sequence_array(abstr_sequence_provider<T>* pr)
		:	m_Values(pr)
		,	m_Indices(pr ? pr->CloneForSeqs() : 0)
	{}

	RTC_CALL sequence_array(const sequence_array<T>& src, data_size_type expectedGrowth = 0);
	sequence_array(sequence_array<T>&& rhs) noexcept { swap(rhs); }

	void operator =(const sequence_array<T>& src) { assign(src, 0); }
	RTC_CALL void assign(const sequence_array<T>& src, data_size_type expectedGrowth);
	RTC_CALL void swap(sequence_array<T>& rhs) noexcept;

	//=======================================
	// sequence_array collection modification
	//=======================================

	RTC_CALL void erase(const iterator& first, const iterator& last);
	RTC_CALL void clear();

	void reserve(size_type n MG_DEBUG_ALLOCATOR_SRC_ARG)      { m_Indices.reserve(n MG_DEBUG_ALLOCATOR_SRC_PARAM); }
	void reserve_data(size_type n MG_DEBUG_ALLOCATOR_SRC_ARG) { m_Values.reserve(n MG_DEBUG_ALLOCATOR_SRC_PARAM); }
	void push_back(Undefined)      { m_Indices.push_back(seq_t( Undefined() )); }

	template <typename CIter>
	void push_back_seq(CIter first, CIter last)
	{
		m_Indices.push_back( seq_t());
		allocateSequenceRange(m_Indices.end()-1, first, last);
	}
	
	//=======================================
	// sequence_array statistice
	//=======================================

	size_type size() const           { return m_Indices.size(); }
	bool      empty() const          { return m_Indices.empty(); }
	size_type capacity() const       { return m_Indices.capacity(); }

	std::size_t nr_bytes(bool calcStreamSize) const 
	{ 
		std::size_t res = NrBytesOf(m_Values, true);
		if (calcStreamSize)
		{
			res += NrStreamBytesOf(m_Indices.size());
			res += m_Indices.size() * sizeof(UInt32) * 2;

			// hack for large values
			for (auto i = m_Indices.begin(), e= m_Indices.end(); i!=e; ++i)
			{
				if (i->second < MAX_VALUE(UInt32))
				{
					dms_assert(i->first < MAX_VALUE(UInt32));
					continue;
				}
				res += sizeof(SizeT);
				if (i->first >= MAX_VALUE(UInt32))
					res += sizeof(SizeT);
			}

		}
		else
			res += NrBytesOf(m_Indices, false);
		return res;
	}


	size_type Size()     const { return m_Indices.Size(); }
	size_type Capacity() const { return m_Indices.Capacity(); }
	size_type Empty()    const { return m_Indices.Empty(); }

	//=======================================
	// sequence_array data access
	//=======================================

	const_reference operator [](size_type i) const
	{
		dms_assert(i<size());
		return const_reference(this, m_Indices.begin() + i);
	}
	reference operator [](size_type i)
	{ 
		dms_assert(i<size());
		return reference(this, m_Indices.begin() + i); 
	}
	const_reference back() const
	{
		dms_assert(size());
		return const_reference(this, m_Indices.end() - 1);
	}
	reference back()
	{ 
		dms_assert(size());
		return reference(this, m_Indices.end() - 1); 
	}
	const_iterator begin() const { return const_iterator(this, m_Indices.begin()); }
	const_iterator end()   const { return const_iterator(this, m_Indices.end());   }
	iterator       begin()       { return iterator      (this, m_Indices.begin()); }
	iterator       end()         { return iterator      (this, m_Indices.end()  ); }

	//=======================================
	// sequence_array memory allocation control
	//=======================================

	//=======================================
	// sequence_array access control
	//=======================================

	bool IsOpen()          const { return m_Indices.IsOpen         () && m_Values.IsOpen    (); }
#if defined(MG_DEBUG_DATA)
	bool IsLocked()        const { return m_Indices.IsLocked() && m_Values.IsLocked  (); }
#endif
	bool CanWrite()        const { return m_Indices.CanWrite       () && m_Values.CanWrite  (); }
	bool IsAssigned()      const { return m_Indices.IsAssigned     () && m_Values.IsAssigned(); }
	bool IsHeapAllocated() const { return m_Indices.IsHeapAllocated() && m_Values.IsHeapAllocated(); }

	RTC_CALL void Open (seq_size_type nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG);
	RTC_CALL void Lock  (dms_rw_mode rwMode);

	void Close () { m_Indices.Close(); m_Values.Close(); dms_assert(Empty()); m_ActualDataSize = 0; }
	void UnLock() { allocate_data(0 MG_DEBUG_ALLOCATOR_SRC_SA); m_Indices.UnLock(); m_Values.UnLock(); }
	void Drop  () { m_Indices.Drop (); m_Values.Drop (); dms_assert(Empty()); m_ActualDataSize = 0; }
	WeakStr GetFileName() const { return m_Values.GetFileName(); }

	RTC_CALL void Reset (abstr_sequence_provider<T>* pr = nullptr);

	RTC_CALL void Reset(seq_size_type nrSeqs, typename data_vector_t::size_type expectedDataSize MG_DEBUG_ALLOCATOR_SRC_ARG);
	RTC_CALL void reset(seq_size_type nrSeqs, typename data_vector_t::size_type expectedDataSize MG_DEBUG_ALLOCATOR_SRC_ARG);
	RTC_CALL void Resize(data_size_type expectedDataSize, seq_size_type expectedSeqsSize, seq_size_type nrSeqs MG_DEBUG_ALLOCATOR_SRC_ARG);

	void data_reserve(data_size_type expectedDataSize MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		m_Values.reserve(expectedDataSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}
	data_size_type data_size()  const
	{
		return m_Values.size();
	}
	data_size_type actual_data_size()  const
	{
		return m_ActualDataSize;
	}
	data_size_type data_capacity() const
	{
		return m_Values.capacity();
	}

	RTC_CALL bool allocate_data(data_size_type expectedGrowth MG_DEBUG_ALLOCATOR_SRC_ARG);
	RTC_CALL bool allocate_data(data_vector_t& oldData, data_size_type expectedGrowth MG_DEBUG_ALLOCATOR_SRC_ARG);

	RTC_CALL void cut(size_type nrSeqs);

	void resizeSO(size_type nrSeqs, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		if (nrSeqs < m_Indices.size())
			cut(nrSeqs);
		else
			m_Indices.resizeSO(nrSeqs, true MG_DEBUG_ALLOCATOR_SRC_PARAM); // constructed values affect assignment as existing sequences are to be cleared.
	}
	void reallocSO(size_type nrSeqs, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		resizeSO(nrSeqs, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}

	bool IsDirty() const 
	{
		dms_assert(m_ActualDataSize <= m_Values.size());
		return (m_ActualDataSize != m_Values.size());
	}

	RTC_CALL void StreamOut(BinaryOutStream& ar) const;
	RTC_CALL void StreamIn (BinaryInpStream& ar, bool mayResize);

protected:
	template<typename Initializer> void allocateSequence(typename base_type::seq_iterator seqPtr, data_size_type newSize, Initializer&& initFunc);

	RTC_CALL void allocateSequenceRange(typename base_type::seq_iterator seqPtr, const_data_iterator first, const_data_iterator last);

private:
	void cut_seq(typename base_type::seq_iterator seqPtr, typename data_vector_t::size_type newSize);
	void abandon(data_size_type first, data_size_type last);
	template<typename Initializer> void appendInitializer(size_type n, Initializer&& init);

	RTC_CALL void appendRange(const_data_iterator first, const_data_iterator last);

protected:
	auto calcActualDataSize() const->data_size_type;

#if defined(MG_DEBUG)
	RTC_CALL void checkActualDataSize() const;
	RTC_CALL void checkConsecutiveness() const;
#endif
	seq_vector_t   m_Indices;            // for identifying allocated sequences
	data_vector_t  m_Values;             // for sequential storage of sequences of T (the memory pool)
	data_size_type m_ActualDataSize = 0; // no of elements in allocated sequences (not abandoned elements)

	friend struct SA_ConstReference<T>;
	friend struct SA_Reference<T>;
	friend struct SA_Iterator<T>;
};


//=======================================
// sequence_array
//=======================================

template <typename T>
struct sequence_vector : sequence_array<T>
{
	using base_type = sequence_array<T>;
	using typename base_type::size_type;
	using typename base_type::const_iterator;

	sequence_vector(sequence_vector&& rhs) = default;
	sequence_vector(const sequence_vector& rhs) = default;

	sequence_vector() : base_type(size_type(0) MG_DEBUG_ALLOCATOR_SRC_EMPTY)
	{
		this->Lock(dms_rw_mode::write_only_all);
	}
	sequence_vector(size_type n MG_DEBUG_ALLOCATOR_SRC_ARG): base_type(n MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		this->Lock(dms_rw_mode::write_only_all);
	}
	template<typename CIter>
	sequence_vector(CIter i, CIter e);
	~sequence_vector()
	{
		if (this->IsAssigned())
			this->UnLock();
	}
};

//=======================================
// sequence_array
//=======================================

// sequence_array constructors

template <typename T>
template <typename CIter>
sequence_vector<T>::sequence_vector(CIter i, CIter e)
	:	sequence_vector()
{
	auto srcDataSize = CalcActualDataSize(i, e);

	this->reset(std::distance(i, e), srcDataSize MG_DEBUG_ALLOCATOR_SRC_STR("sequence_vector from span"));

	auto ri = this->m_Indices.begin();

	while (i != e)
	{
		this->allocateSequenceRange(ri, begin_ptr(*i), end_ptr(*i));
		++ri;
		++i;
	}
	dms_assert(this->calcActualDataSize() == this->actual_data_size());
	dms_assert(srcDataSize == this->actual_data_size());
	dms_assert(!this->IsDirty());
}

template <typename T>
auto sequence_array<T>::calcActualDataSize() const -> data_size_type
{
	MGD_CHECKDATA(IsLocked());
	return CalcActualDataSize(begin(), end());
}


//=======================================
// sequence for sequence_array elements
//=======================================

#include "ptr/PtrBase.h"

template <typename P>
struct sequence_array_ref
{
	typedef SA_Iterator<P>                              iterator;
	typedef SA_Reference<P>                             value_type;
	typedef SA_Reference<P>                             reference;
	typedef typename sequence_array<P>::size_type       size_type;
	typedef typename sequence_array<P>::data_size_type  data_size_type;
	typedef typename sequence_array<P>::difference_type difference_type;

	sequence_array_ref(sequence_array<P>* ptr = nullptr) :  m_ArrayPtr(ptr) {}

	iterator  begin() { return get_sa().begin(); }
	iterator  end()   { return get_sa().end();   }
	reference operator [](SizeT i) { dms_assert(i < size()); return begin()[i]; }
	size_type size()  const { return  get_sa().size();  }

	sequence_array<P>&       get_sa()       { dms_assert(m_ArrayPtr); return *m_ArrayPtr; }
	const sequence_array<P>& get_sa() const { dms_assert(m_ArrayPtr); return *m_ArrayPtr; }

private:
	WeakPtr<sequence_array<P>> m_ArrayPtr;
};

template <typename P>
struct sequence_array_cref
{
	typedef SA_ConstIterator<P>                         const_iterator;
	typedef typename sequence_traits<P>::container_type value_type;
	typedef SA_ConstReference<P>                        const_reference;
	typedef typename sequence_array<P>::size_type       size_type;
	typedef typename sequence_array<P>::difference_type difference_type;

	sequence_array_cref(const sequence_array<P>* ptr = nullptr) : m_ArrayPtr(ptr), m_Size(ptr ? ptr->size() : 0) {}
	sequence_array_cref(SA_ConstIterator<P> begin, SA_ConstIterator<P> end) : m_ArrayPtr(begin.m_Container), m_Size(end - begin) {}

	const_iterator  begin() const { return get_sa().begin(); }
	const_iterator  end()   const { return get_sa().begin() + m_Size; }
	const_reference operator [](SizeT i) const  { dms_assert(i < size()); return begin()[i]; }

	size_type       size()  const { return m_Size; }

	const sequence_array<P>& get_sa() const { dms_assert(m_ArrayPtr); return *m_ArrayPtr; }

private:
	WeakPtr<const sequence_array<P>> m_ArrayPtr;
	SizeT                            m_Size;
};

template<typename T>
sequence_array_ref<T> GetSeq(sequence_array<T>& sa) 
{ 
	return &sa; 
}

template<typename T>
sequence_array_cref<T> GetConstSeq(const sequence_array<T>& csa)
{
	return &csa; 
}

template<typename T>
sequence_array_ref<T> GetSeq(sequence_array_ref<T> sar)
{
	return sar;
}

template<typename T>
sequence_array_cref<T> GetConstSeq(sequence_array_ref<T> sar)
{
	return *sar.get_sa();
}

template<typename T>
sequence_array_cref<T> GetConstSeq(sequence_array_cref<T> csar)
{
	return csar;
}


//----------------------------------------------------------------------
// Section: support for IsDefined predicate and UndefinedValue(X*) type func
//----------------------------------------------------------------------

template <typename T> inline bool IsDefined(const SA_Reference<T>&      sec) { return sec.IsDefined(); }
template <typename T> inline bool IsDefined(const SA_ConstReference<T>& sec) { return sec.IsDefined(); }

template <typename T> inline void MakeUndefined(SA_Reference<T>& lhs) { lhs.assign(Undefined());; }
template <typename T> inline void MakeUndefinedOrZero(SA_Reference<T>& lhs) { lhs.assign(Undefined());; }

//----------------------------------------------------------------------
// Section: Assign helper func for assignment of sequence to/from sequence, vector or SharedStr
//----------------------------------------------------------------------

template <typename T> inline void Assign(SA_Reference<T> lhs, SA_ConstReference<T> rhs) { lhs = rhs; }
template <typename T> inline void Assign(SA_Reference<T> lhs, SA_Reference     <T> rhs) { lhs = rhs; }
template <typename T> inline void Assign(SA_Reference<T> lhs, Undefined)  { lhs.assign(Undefined()); }

template <typename T> inline void Assign(SA_Reference<T> lhs, const std::vector<T>& rhs) 
{ 
	if (IsDefined(rhs))
		lhs.assign(begin_ptr(rhs), end_ptr(rhs) ); 
	else
		lhs.assign(Undefined());
}

template <typename T> inline void Assign(SA_Reference<T> lhs, TokenID rhs) 
{ 
	if (IsDefined(rhs))
		lhs.assign(rhs.GetStr(), rhs.GetStrEnd() ); 
	else
		lhs.assign(Undefined());
}

template <typename T> inline void Assign(std::vector<T>&  lhs, SA_ConstReference<T> rhs) 
{ 
	lhs.assign(rhs.begin(), rhs.end()); 
}

template <typename T> inline void Assign(std::vector<T>&  lhs, SA_Reference<T> rhs) 
{ 
	lhs.assign(rhs.begin(), rhs.end()); 
}

template <typename T> inline void Assign(sequence_array_ref<T> lhs, const sequence_array<T>& rhs) 
{
	lhs.get_sa() = rhs;
}

template <typename T> inline void Assign(std::vector<T>& lhs, Undefined)
{
	vector_clear(lhs);
}

//====================================
// safe_interator TODO generalize for any container
//====================================

template <typename T>
struct sequence_array_index 
{
	typedef typename sequence_array<T>::iterator  iterator;
	typedef typename sequence_array<T>::reference reference;
	typedef typename sequence_array<T>::const_iterator  const_iterator;
	typedef typename sequence_array<T>::const_reference const_reference;

	iterator        get_ptr   ()       { return m_Container->begin() + m_Index; }
	reference       operator *()       { return m_Container->begin()[ m_Index ]; }
	const_iterator  get_ptr   () const { return m_Container->begin() + m_Index;  }
	const_reference operator *() const { return m_Container->begin()[ m_Index ]; }

	sequence_array_index(sequence_array<T>* container, SizeT index)
		:	m_Container(container)
		,	m_Index    (index)
	{}
	explicit sequence_array_index(iterator it)
		:	m_Container(it.m_Container)
		,	m_Index    (it - it.m_Container->begin() )
	{}

	bool  operator ==(const sequence_array_index& rhs) const { dms_assert(m_Container == rhs.m_Container); return m_Index == rhs.m_Index; }
	bool  operator !=(const sequence_array_index& rhs) const { dms_assert(m_Container == rhs.m_Container); return m_Index != rhs.m_Index; }
	DiffT operator - (const sequence_array_index& rhs) const { dms_assert(m_Container == rhs.m_Container); return m_Index -  rhs.m_Index; }

	void operator ++()
	{
		dms_assert(m_Container);
		++m_Index;
		dms_assert(m_Index <= m_Container->size());
	}

	operator       iterator ()       { return get_ptr(); }
	operator const_iterator () const { return get_ptr(); }
//	SA_Reference<T> operator *() const { return m_Container[m_Index]; }

	WeakPtr< sequence_array<T> > m_Container;
	SizeT                        m_Index;
};


#endif // !defined(__GEO_SEQUENCE_ARRAY_H)
