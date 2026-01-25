// Copyright (C) 1998-2023 Object Vision b.v. 
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

template <class T>
struct OwningPtr
{
	using pointer = T*;

	static_assert(sizeof(T) != 0, "Type must be complete");

	OwningPtr(pointer ptr = nullptr) noexcept
		: m_Ptr(ptr)
	{
		assert(!m_Ptr || m_Ptr->GetRefCount() >= 0);
		if (m_Ptr)
			m_Ptr->AdoptRef();
	}

	OwningPtr(OwningPtr&& org) noexcept
		: m_Ptr(org.release())
	{
		assert(!org);
		assert(!m_Ptr || m_Ptr->GetRefCount() >= 0);
	}

	~OwningPtr () noexcept { reset(); }

	void operator = (OwningPtr&& rhs) noexcept { assign(rhs.release()); }

	void    init   (pointer ptr)       noexcept { assert(this->is_null()); this->m_Ptr = ptr; }
	pointer release()                  noexcept { pointer tmp_ptr = this->m_Ptr; this->m_Ptr = nullptr; return tmp_ptr; }
	void    reset  ()                  noexcept { assign(nullptr); }
	void    assign (pointer ptr)       noexcept 
	{ 
		assert(this->m_Ptr != ptr || !ptr); 
		assert(!ptr || ptr->GetRefCount() == 0);
		std::swap(this->m_Ptr, ptr);

		if (ptr && !ptr->DecRef())
		{
			ptr->Release();
		}
	}
	pointer   operator ->() const { return this->get_nonnull(); }
	auto& operator * () const { return *(this->get_nonnull()); }
	explicit operator bool() const { return has_ptr(); }

	bool operator <  (pointer right) const { return m_Ptr < right; }
	bool operator == (pointer right) const { return m_Ptr == right; }
	bool operator != (pointer right) const { return m_Ptr != right; }

	bool has_ptr() const { return m_Ptr != nullptr; }
	bool is_null() const { return m_Ptr == nullptr; }

	pointer   get_ptr() const { return m_Ptr; }
	pointer   get() const { return m_Ptr; }
	pointer   get_nonnull() const { MG_CHECK(m_Ptr != nullptr); return m_Ptr; }

	void swap(OwningPtr& oth) { std::swap(this->m_Ptr, oth.m_Ptr); }
	friend void swap(OwningPtr& a, OwningPtr& b) noexcept { a.swap(b); }

	// illegal copy ctors
	OwningPtr(const OwningPtr<T>& oth) = delete;

private:
	T* m_Ptr = nullptr;
};

template<typename T, typename U = T, typename ...Args>
OwningPtr<T> MakeOwned(Args&& ...args) 
{
	return new U(std::forward<Args>(args)...);
}

#endif // __RTC_PTR_OWNINGPTR_H
