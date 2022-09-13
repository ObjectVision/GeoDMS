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

#if !defined(__RTC_PTR_OWNINGPTRRESERVEDARRAY_H)
#define __RTC_PTR_OWNINGPTRRESERVEDARRAY_H

// container      cpy-ctor    mov-ctor def-ctor oth-ctor    raw_move notes
//                T(const T&) T(T&&)   T()      T(Args...)
// std::vector<T>             req      req      opt
// std::array<T>                       req
// OwningPtrArray                      req                           depends on hidden delete [] implementation
// OwningPtrSizedArray                 req                           could use std::allocator for aligned memory allocation as delete [] is not needed.
// OwningPtrReservedArray									         when reserve is used; not required when only initial_reserve is used.


#include "dbg/debug.h"
#include "ptr/OwningPtrArray.h"
#include "set/RangeFuncs.h"

template <class T>
struct OwningPtrReservedArray : private ref_base<T, movable>
{
	using typename ref_base<T, movable>::pointer;
	using typename ref_base<T, movable>::const_pointer;
	using typename ref_base<T, movable>::reference;
	using typename ref_base<T, movable>::const_reference;
	using typename ref_base<T, movable>::has_ptr;

	typedef pointer iterator;
	typedef const_pointer const_iterator;

	OwningPtrReservedArray() {}
	OwningPtrReservedArray(SizeT capacity) { initial_reserve(capacity); }
	OwningPtrReservedArray(OwningPtrReservedArray<T>&& oth) { swap(oth); }

	~OwningPtrReservedArray()
	{
		destroy_range(begin(), end());
		std::allocator<T> myAlloc;
		myAlloc.deallocate(this->get_ptr(), m_Capacity);
	}

	SizeT size() const { return m_Size; }
	SizeT capacity() const { return m_Capacity; }

	void swap(OwningPtrReservedArray<T>& oth)
	{
		ref_base<T, movable>::swap(oth);
		std::swap(m_Size, oth.m_Size);
		std::swap(m_Capacity, oth.m_Capacity);
	}
	void clear() { OwningPtrReservedArray empty; swap(empty); }
	void operator = (OwningPtrReservedArray<T>&& rhs) { clear(); swap(rhs); }

	pointer        begin() { return this->get_ptr(); }
	const_pointer  begin()   const { return this->get_ptr(); }
	iterator       end() { return begin() + m_Size; }
	const_iterator end() const { return begin() + m_Size; }

	reference       operator [](SizeT i) { dms_assert(i < size()); return begin()[i]; }
	const_reference operator [](SizeT i) const { dms_assert(i < size()); return begin()[i]; }

	reference       back() { dms_assert(size()); return begin()[m_Size - 1]; }
	const_reference back() const { dms_assert(size()); return begin()[m_Size - 1]; }


	void initial_reserve(SizeT capacity)
	{
		dms_assert(m_Size == 0);
		dms_assert(m_Capacity == 0);
		std::allocator<T> myAlloc;
		this->m_Ptr = myAlloc.allocate(capacity);
		m_Capacity = capacity;
	}
	void reserve(SizeT capacity)
	{
		if (capacity > m_Capacity)
		{
			OwningPtrReservedArray<T> oth; oth.initial_reserve(capacity);
			raw_move(begin(), end(), oth.begin());
			oth.m_Size = m_Size;
			m_Size = 0;
			swap(oth);
		}
	}
	template<typename ...Args>
	void emplace_back(Args&& ...args)
	{
		dms_assert(m_Size < m_Capacity);
		::new(end()) T(std::forward<Args>(args)...);
		++m_Size;
	}

private:
	OwningPtrReservedArray(const OwningPtrReservedArray& src); // don't call this
	SizeT m_Size = 0;
	SizeT m_Capacity = 0;
};

#endif // __RTC_PTR_OWNINGPTRRESERVEDARRAY_H
