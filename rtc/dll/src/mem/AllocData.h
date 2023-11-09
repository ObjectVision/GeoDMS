// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if !defined(__RTC_MEM_ALLOCDATA_H)
#define __RTC_MEM_ALLOCDATA_H

#include "geo/SequenceTraits.h"
#include "geo/iterrange.h"

//----------------------------------------------------------------------
// interfaces to allocated (file mapped?) arrays
//----------------------------------------------------------------------

template <typename V>
struct alloc_data : IterRange<typename sequence_traits<V>::pointer>
{
	using base_type = IterRange<typename sequence_traits<V>::pointer>;
	using typename base_type::size_type;
	using typename base_type::iterator;

	alloc_data() {} // value-initialisation

	alloc_data(iterator first, iterator last, size_type cap)
		: base_type(first, last)
		, m_Capacity(cap) {}

	alloc_data(alloc_data&& rhs) noexcept
		: alloc_data()
	{
		this->operator =(std::move(rhs));
	}
	alloc_data& operator = (alloc_data&& rhs) noexcept
	{
		static_cast<base_type*>(this)->operator=( std::move<base_type&>(rhs) );
		m_Capacity = rhs.m_Capacity;
		static_cast<base_type&>(rhs) = {};
		rhs.m_Capacity = 0;
		return *this;
	}
	size_type capacity() const { return m_Capacity; }

	void swap(alloc_data<V>& oth) noexcept
	{
		base_type::swap(oth);
		omni::swap(m_Capacity, oth.m_Capacity);
	}

	size_type m_Capacity = 0;
};

#endif // __RTC_MEM_ALLOCDATA_H
