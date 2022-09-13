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

//	Template OwningPtr holds onto a pointer obtained via new and deletes that
//	object when it itself is destroyed (such as when leaving block scope). It
//	can be used to make calls to new() exception safe and as data-members. Copy
//	& assingment are non-const; the pointer is cleared after transfer
//	vector<owning_ptr< T > > is therefore not allowed.

#if !defined(__RTC_PTR_OWNINGPTRSIZEDARRAY_H)
#define __RTC_PTR_OWNINGPTRSIZEDARRAY_H

#include "dbg/debug.h"
#include "ptr/OwningPtrArray.h"
#include "set/RangeFuncs.h"

template <class T>
struct OwningPtrSizedArray : private ref_base<T, movable>
{
	using base_type = ref_base<T, movable>;
	using typename base_type::pointer;
	using typename base_type::const_pointer;
	using typename base_type::reference;
	using typename base_type::const_reference;
	using iterator       = pointer;
	using const_iterator = const_pointer ;

	OwningPtrSizedArray()
	{}

	OwningPtrSizedArray(SizeT sz MG_DEBUG_ALLOCATOR_SRC_ARG)
		:	ref_base<T, movable>(array_traits<T>::Create(sz MG_DEBUG_ALLOCATOR_SRC_PARAM))
		,	m_Size(sz)
	{}
	OwningPtrSizedArray(SizeT sz, Undefined MG_DEBUG_ALLOCATOR_SRC_ARG)
		: OwningPtrSizedArray(sz MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		fast_undefine(begin(), begin() + sz);
	}

	OwningPtrSizedArray(const_pointer first, const_pointer last MG_DEBUG_ALLOCATOR_SRC_ARG)
		: OwningPtrSizedArray(last - first MG_DEBUG_ALLOCATOR_SRC_PARAM)
		, m_Size(last - first)
	{
		fast_copy(first, last, begin());
	}

	OwningPtrSizedArray(OwningPtrSizedArray&& oth)
	{
		swap(oth);
	}
	~OwningPtrSizedArray()
	{
		reset();
	}
	void reset(pointer ptr = pointer()) noexcept
	{
		dms_assert(this->m_Ptr != ptr || !ptr);
		if (this->m_Ptr)
			array_traits<T>::Destroy(this->m_Ptr, m_Size);
		else
			assert(!m_Size);
		this->m_Ptr = ptr;
	}

	SizeT size() const { return m_Size; }
	bool empty() const { return !m_Size; }
	operator bool() const { return m_Size; }
//	bool IsDefined() const { return this->get_ptr() != nullptr; }

	void clear() { OwningPtrSizedArray empty; swap(empty); }

	void swap (OwningPtrSizedArray<T>& oth) { base_type::swap(oth); std::swap(m_Size, oth.m_Size); }

	void operator = (OwningPtrSizedArray<T>&& rhs) { clear(); swap(rhs); }
	reference       operator [](SizeT i)       { dms_assert(i < size()); return begin()[i]; }
	const_reference operator [](SizeT i) const { dms_assert(i < size()); return begin()[i]; }

	reference       back()       { dms_assert(size()); return begin()[ m_Size -1 ]; }
	const_reference back() const { dms_assert(size()); return begin()[ m_Size -1 ]; }

	iterator       begin()       { return this->get(); }
	const_iterator begin() const { return this->get(); }
	iterator       end  ()       { return begin() + m_Size; }
	const_iterator end  () const { return begin() + m_Size; }

	void reserve(SizeT sz MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		if (sz > m_Size)
		{
			OwningPtrSizedArray local(sz MG_DEBUG_ALLOCATOR_SRC_PARAM);
			swap(local);
		}
	}
	void resizeSO(SizeT sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		auto oldSize = m_Size;
		grow(sz MG_DEBUG_ALLOCATOR_SRC_PARAM);
		m_Size = sz;
		if (oldSize < m_Size)
			fast_zero(begin() + oldSize, end());
	}
	void reallocSO(SizeT sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		reserve(sz MG_DEBUG_ALLOCATOR_SRC_PARAM);
		m_Size = sz;
		if (mustClear)
			fast_zero(begin(), end());
	}
	void grow(SizeT sz MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		if (sz > m_Size)
		{
			OwningPtrSizedArray local(sz MG_DEBUG_ALLOCATOR_SRC_PARAM);
			fast_copy(begin(), end(), local.begin());
			swap(local);
		}
	}

private:
	OwningPtrSizedArray(const OwningPtrSizedArray& src); // don't call this
	SizeT m_Size = 0;
};


template <typename T>
inline bool IsDefined(const OwningPtrSizedArray<T>& v) { return v.IsDefined(); }

template <typename T>
auto GetSeq(OwningPtrSizedArray<T>& so) -> IterRange<typename OwningPtrSizedArray<T>::pointer>
{
	return { so.begin(), so.end() };
}

template <typename T>
auto GetConstSeq(const OwningPtrSizedArray<T>& so) -> IterRange<typename OwningPtrSizedArray<T>::const_pointer>
{
	return { so.begin(), so.end() };
}


#endif // __RTC_PTR_OWNINGPTRSIZEDARRAY_H
