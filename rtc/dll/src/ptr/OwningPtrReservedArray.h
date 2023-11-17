// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

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
#include "set/rangefuncs.h"

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

	OwningPtrReservedArray() noexcept {}
	OwningPtrReservedArray(SizeT capacity) { initial_reserve(capacity); }
	OwningPtrReservedArray(OwningPtrReservedArray<T>&& oth) noexcept { swap(oth); }

	~OwningPtrReservedArray()
	{
		destroy_range(begin(), end());
		std::allocator<T> myAlloc;
		myAlloc.deallocate(this->get_ptr(), m_Capacity);
	}

	SizeT size() const { return m_Size; }
	SizeT capacity() const { return m_Capacity; }

	void swap(OwningPtrReservedArray<T>& oth) noexcept
	{
		ref_base<T, movable>::swap(oth);
		std::swap(m_Size, oth.m_Size);
		std::swap(m_Capacity, oth.m_Capacity);
	}
	void clear() { OwningPtrReservedArray empty; swap(empty); }
	void operator = (OwningPtrReservedArray<T>&& rhs) noexcept { clear(); swap(rhs); }

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
