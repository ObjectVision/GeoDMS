// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

//----------------------------------------------------------------------
// static pointers for construction on demand
//----------------------------------------------------------------------

#ifndef __RTC_PTR_WEAKPTR_H
#define __RTC_PTR_WEAKPTR_H

#include "dbg/Check.h"
#include "ptr/PtrBase.h"
#include "utl/swap.h"

/*
 *	WeakPtr: pointer with object semantics, one can derive from it and access is checked
 */

template <class T>
struct WeakPtr : ptr_base<T, copyable>
{
	using base_type = ptr_base<T, copyable>;
	using typename base_type::pointer;

	constexpr WeakPtr(pointer ptr = nullptr) noexcept
		: base_type(ptr) 
	{}
	WeakPtr(const WeakPtr& oth) : base_type(oth.m_Ptr) {}


	void reset(pointer ptr = nullptr) { this->m_Ptr = ptr; }
	void operator =(pointer ptr) { reset(ptr); }
	void operator =(const WeakPtr& rhs) { reset(rhs.m_Ptr); }

	void swap(WeakPtr& oth) { omni::swap(this->m_Ptr, oth.m_Ptr); }

	operator bool() const noexcept { return this->m_Ptr != nullptr; }
};

#endif // __RTC_PTR_WEAKPTR_H
