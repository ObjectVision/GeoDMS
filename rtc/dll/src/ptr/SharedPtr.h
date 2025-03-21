// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__PTR_SHAREDPTR_H)
#define __PTR_SHAREDPTR_H

#include "RtcBase.h"
#include "dbg/Check.h"
#include "dbg/DebugCast.h"
#include "ptr/PtrBase.h"

struct no_zombies {};

template <class Ptr>
struct SharedPtrWrap : Ptr
{
	using typename Ptr::pointer;

	constexpr SharedPtrWrap() noexcept : Ptr()
	{
		assert(!this->m_Ptr);
	}

	SharedPtrWrap(const SharedPtrWrap& rhs) noexcept
		: Ptr(rhs)
	{ 
		IncCount();
	}

	SharedPtrWrap(SharedPtrWrap&& rhs) noexcept // we assume no external access to rhs as that would conflict with its rvalue-ness
		: Ptr() 
	{ 
		assert(!this->m_Ptr);
		swap(rhs);
	}

	template <class RhsPtr>
	SharedPtrWrap(SharedPtrWrap<RhsPtr>&& rhs) noexcept // we assume no external access to rhs as that would conflict with its rvalue-ness
		: Ptr(rhs.release())
	{}

	~SharedPtrWrap() noexcept { DecCount(); }

	void operator = (SharedPtrWrap&& rhs) noexcept
	{
		swap(rhs);
	}

	void operator = (const SharedPtrWrap& rhs) noexcept
	{ 
		*this = SharedPtrWrap(rhs);
	}

	void assign(pointer ptr) noexcept // TODO G8: REMOVE, replace by reset
	{ 
		*this = SharedPtrWrap(ptr);
	}

	void reset(pointer ptr) noexcept
	{
		*this = SharedPtrWrap(ptr);
	}

	void reset() noexcept
	{
		assign(nullptr);
	}

	auto delayed_reset() noexcept -> zombie_destroyer
	{
		if (!Ptr::m_Ptr)
			return {};
		auto ptr = Ptr::m_Ptr;
		Ptr::m_Ptr = nullptr;
		return ptr->DelayedRelease();
	}

	void swap(SharedPtrWrap& rhs) { omni::swap(Ptr::m_Ptr, rhs.m_Ptr); }
	friend void swap(SharedPtrWrap& a, SharedPtrWrap& b) { a.swap(b); }

protected:
	constexpr SharedPtrWrap(pointer ptr) : Ptr(ptr) { IncCount(); }
	SharedPtrWrap(pointer ptr, no_zombies) : Ptr(ptr && ptr->DuplRef() ? ptr : nullptr) {}

	Ptr release() noexcept { auto ptr = this->m_Ptr; this->m_Ptr = nullptr; return Ptr(ptr); }

	constexpr void IncCount () const noexcept { if (Ptr::m_Ptr) Ptr::m_Ptr->IncRef(); }
	constexpr void DecCount () const noexcept 
	{ 
		if (!this->m_Ptr)
			return;
		this->m_Ptr->Release();
	}
	template <typename T> friend struct SharedPtrWrap;
};

template <class T>
struct SharedPtr : SharedPtrWrap<ptr_base<T, copyable> >
{
	using base_type = SharedPtrWrap<ptr_base<T, copyable> >;
	using typename base_type::pointer;

	SharedPtr(std::nullptr_t = nullptr) noexcept {}

	SharedPtr(SharedPtr&& rhs) noexcept
		: base_type(std::move(rhs))
	{ 
		assert(rhs.m_Ptr == nullptr); 
	}
	SharedPtr(const SharedPtr& rhs) = default;

	template <typename U>
	SharedPtr(U* rhs) noexcept
		:	base_type(rhs)
	{}

	template <typename U>
	SharedPtr(U* rhs, no_zombies nz) noexcept
		: base_type(rhs, nz)
	{}

	template <typename SrcPtr>
	SharedPtr(SharedPtr<SrcPtr>&& rhs) noexcept
		: base_type(std::move(rhs))
	{}

	SharedPtr& operator =(const SharedPtr& rhs) = default;
	SharedPtr& operator =(SharedPtr&& rhs) = default;

	template <typename SrcPtr>
	void operator =(SrcPtr&& rhs) noexcept { *this = SharedPtr(std::forward<SrcPtr>(rhs)); }

};

template<typename T>
auto MakeShared(T* ptr) -> SharedPtr<T>
{ 
	return ptr; 
}


//  -----------------------------------------------------------------------

#endif // __PTR_SHAREDPTR_H
