// Copyright (C) 1998-2026 Object Vision b.v. 
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

struct newly_obj {};
struct existing_obj {};
struct no_zombies {};

template <class Ptr>
struct SharedPtrWrap : Ptr
{
	using typename Ptr::pointer;

	constexpr SharedPtrWrap() noexcept : Ptr()
	{
		assert(!this->m_Ptr);
	}

	SharedPtrWrap(const SharedPtrWrap<Ptr>& rhs) noexcept
		: Ptr(rhs)
	{ 
		IncCount();
	}

	SharedPtrWrap(SharedPtrWrap<Ptr>&& rhs) noexcept // we assume no external access to rhs as that would conflict with its rvalue-ness
		: Ptr(rhs.release_ptr())
	{
		assert(!rhs.m_Ptr);
	}

	template<class RhsPtr>
	SharedPtrWrap(SharedPtrWrap<RhsPtr>&& rhs) noexcept // we assume no external access to rhs as that would conflict with its rvalue-ness
		: Ptr(rhs.release_ptr()) 
	{ 
		assert(!rhs.m_Ptr);
	}

	~SharedPtrWrap() noexcept { DecCount(); }

	SharedPtrWrap<Ptr>& operator = (SharedPtrWrap<Ptr>&& rhs) noexcept
	{
		this->swap(rhs);
		return *this;
	}

	SharedPtrWrap<Ptr>& operator = (const SharedPtrWrap<Ptr>& rhs) noexcept
	{ 
		auto tmp = SharedPtrWrap(rhs);
		this->swap(tmp);
		return *this;
	}

//	void assign(pointer ptr) noexcept // TODO G8: REMOVE, replace by reset
//	{ 
//		*this = SharedPtrWrap(ptr);
//	}

	void reset(pointer ptr) noexcept
	{
		*this = SharedPtrWrap(ptr, newly_obj{});
	}

	void reset() noexcept
	{
		reset(nullptr);
	}

	auto delayed_reset() noexcept -> zombie_destroyer
	{
		if (!this->m_Ptr)
			return {};
		auto ptr = this->m_Ptr;
		this->m_Ptr = nullptr;
		return ptr->DelayedRelease();
	}

	void swap(SharedPtrWrap<Ptr>& rhs) noexcept
	{ 
		auto tmp = this->m_Ptr;
		this->m_Ptr = rhs.m_Ptr;
		rhs.m_Ptr = tmp;
	}
	friend void swap(SharedPtrWrap<Ptr>& a, SharedPtrWrap<Ptr>& b) noexcept { a.swap(b); }

protected:
	constexpr SharedPtrWrap(pointer ptr, newly_obj) noexcept : Ptr(ptr) { if (ptr) ptr->AdoptRef(); }
	SharedPtrWrap(pointer ptr, existing_obj) noexcept : Ptr(ptr) { if (ptr) ptr->IncRef(); }
	SharedPtrWrap(pointer ptr, no_zombies) noexcept : Ptr(ptr && ptr->DuplRef() ? ptr : nullptr) {}

	pointer release_ptr() noexcept { auto ptr = this->m_Ptr; this->m_Ptr = nullptr; return ptr; }

	constexpr void IncCount () const noexcept { if (this->m_Ptr) this->m_Ptr->IncRef(); }
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

	SharedPtr(SharedPtr<T>&& rhs) noexcept
		: base_type(std::move(rhs))
	{ 
		assert(rhs.m_Ptr == nullptr); 
	}

	SharedPtr(const SharedPtr<T>& rhs) noexcept
		: base_type(rhs)
	{}

	template <typename U>
	SharedPtr(U* rhs) noexcept
		:	base_type(rhs, newly_obj{})
	{}

	template <typename U>
	SharedPtr(U* rhs, existing_obj eo) noexcept
		: base_type(rhs, eo)
	{
	}

	template <typename U>
	SharedPtr(U* rhs, no_zombies nz) noexcept
		: base_type(rhs, nz)
	{}

	template <typename SrcPtr>
	SharedPtr(SharedPtr<SrcPtr>&& rhs) noexcept
		: base_type(std::move(rhs))
	{
		assert(!rhs.get_ptr());
	}

	SharedPtr<T>& operator =(const SharedPtr<T>& rhs) noexcept
	{
		auto tmp = SharedPtr<T>(rhs);
		this->swap(tmp);
		return *this;
	}

	SharedPtr<T>& operator =(SharedPtr<T>&& rhs) noexcept
	{
		this->swap(rhs);
		return *this;
	}

	template <typename U>
	SharedPtr& operator =(U* rhs) noexcept
	{ 
		if (this->m_Ptr != rhs)   // adjust if pointer types differ
		{
			// construct wrapper that calls IncCount that already calls IncRef on the rhs (if non-null)
			auto tmp = SharedPtr<T>(rhs);
			this->swap(tmp);     // tmp now holds old pointer, for which Release should be called by DecCount from tmp's dtor
		}
		return *this;
	}
};

template<typename T>
auto MakeShared(T* ptr) -> SharedPtr<T>
{ 
	return ptr; 
}


//  -----------------------------------------------------------------------

#endif // __PTR_SHAREDPTR_H
