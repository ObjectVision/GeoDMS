// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_RTC_GEO_RINGITERATOR_H)
#define DMS_RTC_GEO_RINGITERATOR_H

#pragma warning( disable : 4018) // warning C4018: '<=' : signed/unsigned mismatch
#pragma warning( disable : 4146) // warning C4146: unary minus operator applied to unsigned type, result still unsigned

#include "geo/PointIndexBuffer.h"
#include "geo/PointOrder.h"
#include "geo/SequenceTraits.h"

// *****************************************************************************
//	SA_ConstRing
// *****************************************************************************

// template <typename P> struct SA_ConstRing; //fwrd decl ?


template <typename P>
struct SA_ConstRing : IterRange<const P*>
{
	typedef P                            point_type;
	typedef typename scalar_of<P>::type  coordinate_type;
	typedef typename IterRange<const P*>::iterator iterator_type;

	SA_ConstRing(const P* first, const P* last)
		:	IterRange<const P*>(first, last)
	{}
};

// *****************************************************************************
//	SA_ConstRingIterator
// *****************************************************************************


template <typename P>
struct SA_ConstRingIterator
{
//	typedef std::forward_iterator_tag iterator_category;
	typedef std::random_access_iterator_tag iterator_category;
	typedef SA_ConstRing<P>                 value_type;
	typedef DiffT                           difference_type;
	typedef SA_ConstRing<P>*                pointer;
	typedef SA_ConstRing<P>&                reference;

	SA_ConstRingIterator() {}

	SA_ConstRingIterator(SA_ConstRingIterator&& src) 
		:	m_SequenceBase(src.m_SequenceBase)
		,	m_IndexBuffer (std::move(src.m_IndexBuffer))
		,	m_RingIndex   (src.m_RingIndex)
	{
	}

	SA_ConstRingIterator(const SA_ConstRingIterator& src) 
		:	m_SequenceBase(src.m_SequenceBase)
		,	m_IndexBuffer (src.m_IndexBuffer)
		,	m_RingIndex   (src.m_RingIndex)
	{
	}

	template <typename PointRange>
	SA_ConstRingIterator(const PointRange& scr, SizeT index)
		:	m_SequenceBase(begin_ptr(scr))
		,	m_RingIndex(index)
	{
		if (index != -1)
		{
			fillPointIndexBuffer(m_IndexBuffer, scr.begin(), scr.end());
			if (m_IndexBuffer.empty())
				m_RingIndex = -1;
			else
				assert(m_RingIndex < m_IndexBuffer.size());
		}
	}

	SA_ConstRingIterator& operator =(const SA_ConstRingIterator& rhs) = default;
	bool operator ==(const SA_ConstRingIterator& rhs) const { assert(m_SequenceBase == rhs.m_SequenceBase); return m_RingIndex == rhs.m_RingIndex; }
	bool operator !=(const SA_ConstRingIterator& rhs) const { assert(m_SequenceBase == rhs.m_SequenceBase); return m_RingIndex != rhs.m_RingIndex; }

	void operator ++()
	{ 
		assert(m_RingIndex != -1);
		if (++m_RingIndex == m_IndexBuffer.size())
			m_RingIndex = -1;
	}
	void operator --()
	{
		assert(m_RingIndex != 0);
		if (m_RingIndex == -1)
			m_RingIndex = m_IndexBuffer.size() - 1;
		else
			--m_RingIndex;
	}
	SA_ConstRing<P> operator *() const
	{
		assert(m_RingIndex != -1);
		assert(m_RingIndex < m_IndexBuffer.size());
		return SA_ConstRing<P>(
			m_SequenceBase + m_IndexBuffer[m_RingIndex].first 
		,	m_SequenceBase + m_IndexBuffer[m_RingIndex].second
		);
	}
	difference_type operator -(const SA_ConstRingIterator& oth) const
	{
		SizeT index    = m_RingIndex;
		SizeT othIndex = oth.m_RingIndex;
		if (index == othIndex) // same, including both at end
			return 0;
		if (othIndex == -1)
			othIndex = m_IndexBuffer.size(); // oth is at end but not this
		if (index == -1)
			index = oth.m_IndexBuffer.size(); // this is at end but not oth
		return index - othIndex;
	}

	const P*            m_SequenceBase = nullptr;
	pointIndexBuffer_t  m_IndexBuffer;
	SizeT               m_RingIndex = -1;
};

#endif //!defined(DMS_RTC_GEO_RINGITERATOR_H)
