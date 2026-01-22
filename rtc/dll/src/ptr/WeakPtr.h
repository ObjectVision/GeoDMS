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


template <class Ptr>
struct WeakPtrWrap : Ptr
{
	using typename Ptr::pointer;

	constexpr WeakPtrWrap(pointer ptr = nullptr) noexcept 
		: Ptr(ptr)
	{}

	WeakPtrWrap(const WeakPtrWrap& oth    ): Ptr(oth.m_Ptr) {}

	void reset(pointer ptr = nullptr)       { Ptr::m_Ptr = ptr; }
	void operator =(pointer ptr)            { reset(ptr); }
	void operator =(const WeakPtrWrap& rhs) { reset(rhs.m_Ptr); }

	void swap(WeakPtrWrap& oth) { omni::swap(Ptr::m_Ptr, oth.m_Ptr); }
};

template <class T>
struct WeakPtr : WeakPtrWrap< ptr_base<T, copyable> >
{
	using base_type = WeakPtrWrap< ptr_base<T, copyable> >;
	using typename base_type::pointer;

	WeakPtr(pointer ptr = nullptr): base_type(ptr) {}

	operator bool() const noexcept { return this->m_Ptr != nullptr; }
};

#endif // __RTC_PTR_WEAKPTR_H
