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

/* REMOVE
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

	template <typename DRT>
	auto delayed_reset() noexcept -> std::unique_ptr<DRT>
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
*/

template <class T>
struct SharedPtr
{
	using pointer = T*;

	constexpr SharedPtr() noexcept = default;
	~SharedPtr() noexcept
	{
		DecCount();
	}

	SharedPtr(SharedPtr<T>&& rhs) noexcept
		: m_Ptr(rhs.release_ptr())
	{
		assert(!m_Ptr || m_Ptr->IsOwned());
	}

	SharedPtr(const SharedPtr<T>& rhs) noexcept
		: m_Ptr(rhs.m_Ptr)
	{
		assert(!rhs || rhs->IsOwned());
		IncCount();
		assert(!m_Ptr || m_Ptr->IsOwned());
	}
	SharedPtr(const WeakPtr<T>& rhs) noexcept
		: SharedPtr(rhs.get(), existing_obj{})
	{}

	// use this constructor for newly created objects
	template <typename U>
	constexpr SharedPtr(U* rhs, newly_obj) noexcept
		: m_Ptr(rhs)
	{
		assert(!rhs || !rhs->IsOwned());
		IncCount();
	}

	// use this constructor for borrowed existing objects, for which existing Shared Ownership can be be assumed
	template <typename U>
	SharedPtr(U* rhs, existing_obj) noexcept
		: m_Ptr(rhs)
	{
		assert(!rhs || rhs->IsOwned());
		IncCount();
	}

	// use this constructor for borrowed existing objects, for which existing Shared Ownership cannot be be assumed, such as Cache back pointers or side pointers to objects for which abandonment might have been initited but not yet completed.
	// this always requires user-side synchronization 
	template <typename U>
	SharedPtr(U* rhs, no_zombies nz) noexcept
		: m_Ptr(rhs && rhs->DuplRef() ? rhs : nullptr)
	{
		assert(!m_Ptr || m_Ptr->IsOwned());
	}

	template <typename U>
	SharedPtr(SharedPtr<U>&& rhs) noexcept
		: m_Ptr(rhs.release_ptr())
	{
		assert(!rhs || rhs->IsOwned());
	}

	template <typename U>
	SharedPtr(const SharedPtr<U>& rhs) noexcept
		: m_Ptr(rhs.get())
	{
		assert(!rhs || rhs->IsOwned());
		IncCount();
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

	void operator =(std::nullptr_t) noexcept
	{
		reset();
	}

	operator pointer() const noexcept { return this->m_Ptr; } // TODO : REMOVE and be more explicit
	pointer get() const noexcept { return this->m_Ptr; }
	pointer get_ptr() const noexcept { return this->m_Ptr; }
	bool has_ptr() const noexcept { return this->m_Ptr != nullptr; }
	bool is_null() const noexcept { return this->m_Ptr == nullptr; }
	operator bool() const noexcept { return has_ptr(); }

	pointer get_nonnull() const 
	{ 
		assert(this->m_Ptr != nullptr); 
		return this->m_Ptr; 
	}
	pointer operator ->() const noexcept { return get_nonnull(); }
	
	auto delayed_reset() noexcept -> std::unique_ptr<T>
	{
		if (!this->m_Ptr)
			return {};
		auto ptr = this->m_Ptr;
		this->m_Ptr = nullptr;
		return ptr->DelayedRelease();
	}

	void swap(SharedPtr<T>& rhs) noexcept
	{
		auto tmp = this->m_Ptr;
		this->m_Ptr = rhs.m_Ptr;
		rhs.m_Ptr = tmp;
	}
	friend void swap(SharedPtr<T>& a, SharedPtr<T>& b) noexcept { a.swap(b); }

	void reset() noexcept
	{
		DecCount();
		*this = nullptr;
	}
	void reset(T* ptr) noexcept
	{
		auto tmp = SharedPtr<T>(ptr, newly_obj{});
		this->swap(tmp);
	}

	pointer release_ptr() noexcept { auto ptr = this->m_Ptr; this->m_Ptr = nullptr; return ptr; }
private:

	constexpr void IncCount() const noexcept { if (this->m_Ptr) this->m_Ptr->IncRef(); }
	constexpr void DecCount() const noexcept
	{
		if (!this->m_Ptr)
			return;
		this->m_Ptr->Release();
	}

	T* m_Ptr = nullptr;
};

template<typename T, typename OwningTag>
auto MakeShared(T* ptr, OwningTag tag) -> SharedPtr<T>
{ 
	return SharedPtr<T>(ptr, tag);
}

template<typename T>
auto MakeSharedForNewlyCreatedObject(T* ptr) -> SharedPtr<T>
{
	return MakeShared<T>(ptr, newly_obj{});
}

template<typename T>
auto MakeSharedFromBorrowedObjectPtr(T* ptr) -> SharedPtr<T>
{
	return MakeShared<T>(ptr, existing_obj{});
}

template<typename T>
auto MakeSharedFromWeakPtrInsideSync(T* ptr) -> SharedPtr<T>
{
	return MakeShared<T>(ptr, no_zombies{});
}


//  -----------------------------------------------------------------------

#endif // __PTR_SHAREDPTR_H
