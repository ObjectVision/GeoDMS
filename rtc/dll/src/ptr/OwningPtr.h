// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

//	Template OwningPtr holds onto a pointer obtained via new and deletes that
//	object when it itself is destroyed (such as when leaving block scope). It
//	can be used to make calls to new() exception safe and as data-members. Copy
//	& assingment are non-const; the pointer is cleared after transfer
//	vector<owning_ptr< T > > is therefore not allowed.

#if !defined(__RTC_PTR_OWNINGPTR_H)
#define __RTC_PTR_OWNINGPTR_H

#include "dbg/Check.h"
#include "ptr/PtrBase.h"

#include <boost/checked_delete.hpp>

template <class T>
struct OwningPtr : ptr_base<T, movable>
{
	using base_type = ptr_base<T, movable>;
	using typename base_type::pointer;

	OwningPtr(pointer ptr = nullptr) noexcept
		: base_type(ptr)
	{}
	OwningPtr(OwningPtr&& org) noexcept
		: base_type(org.release())
	{}

	~OwningPtr () noexcept { reset(); }

	void    init   (pointer ptr)       noexcept { dms_assert(this->is_null()); this->m_Ptr = ptr; }
	pointer release()                  noexcept { pointer tmp_ptr = this->m_Ptr; this->m_Ptr = nullptr; return tmp_ptr; }
	void    reset  ()                  noexcept { assign(nullptr); }
	void    assign (pointer ptr)       noexcept { dms_assert(this->m_Ptr != ptr || !ptr); std::swap(this->m_Ptr, ptr); boost::checked_delete(ptr); }
	void    swap   (OwningPtr<T>& oth) noexcept { std::swap(this->m_Ptr, oth.m_Ptr); }

	void operator = (OwningPtr&& rhs) noexcept { assign(rhs.release()); }

	friend void swap(OwningPtr& a, OwningPtr& b) noexcept { a.swap(b); }

	// illegal copy ctors
	OwningPtr(const OwningPtr<T>& oth) = delete;
};

template<typename T, typename U = T, typename ...Args>
OwningPtr<T> MakeOwned(Args&& ...args) 
{
	return new U(std::forward<Args>(args)...);
}

#endif // __RTC_PTR_OWNINGPTR_H
