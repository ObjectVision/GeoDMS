// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

//  -----------------------------------------------------------------------
//  Name        : dms\tic\src\InterestHolders.h
//  -----------------------------------------------------------------------

#if !defined(__RTC_PTR_INTERESTHOLDER_H)
#define __RTC_PTR_INTERESTHOLDER_H

#include "RtcBase.h"
#include "act/garbage_can.h"
#include "utl/swap.h"

template <class T> struct shared_tree_ptr; // ptr/SharedTreePtr.h; only the template name is needed for the ctor signatures below


//----------------------------------------------------------------------
// class  : OptionalInterest
//----------------------------------------------------------------------

template <typename T> void OptionalInterestInc(T* ptr)          { if (ptr) ptr->IncInterestCount(); }
template <typename T> garbage_can OptionalInterestDec(T* ptr) noexcept { if (!ptr) return {};  return ptr->DecInterestCount(); }

//----------------------------------------------------------------------
// class  : InterestPtr
//----------------------------------------------------------------------

struct already_incremented_tag {};

template <typename CPtr>
struct InterestPtr
{
	using IPtr = typename pointer_traits<CPtr>::ptr_type;
	using IRef = typename pointer_traits<CPtr>::ref_type;
	using IVal = typename pointer_traits<CPtr>::value_type;

	InterestPtr(std::nullptr_t = nullptr) noexcept 
		:	m_Item() // may be raw pointer that must be initialized
	{}

	InterestPtr(InterestPtr&& src) noexcept
		:	m_Item(std::move(src.m_Item)) 
	{
		src.m_Item = CPtr();
	}

	InterestPtr(const InterestPtr& src) noexcept
		: m_Item(src.m_Item)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	template <typename T>
	InterestPtr(T* ptr)
		: m_Item()
	{
		if (ptr)
		{
			// CPtr may be a raw pointer, an intrusive SharedPtr, or a std::shared_ptr-backed
			// shared_tree_ptr. Only the latter needs a construction tag to recover its control block.
			if constexpr (requires { CPtr(ptr, existing_obj{}); })
				m_Item = CPtr(ptr, existing_obj{});
			else
				m_Item = CPtr(ptr);
		}
		OptionalInterestInc<IVal>(get_ptr());
	}

	template <typename T, typename OwnershipTag>
	InterestPtr(T* ptr, OwnershipTag tag)
		: m_Item(ptr, tag)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}
	template <typename T>
	InterestPtr(SharedPtr<T>&& item)
		: m_Item(std::move(item))
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	template <typename T>
	InterestPtr(SharedPtr<T>&& item, already_incremented_tag)
		: m_Item(std::move(item))
	{
		assert(get_ptr() != nullptr && get_ptr()->GetInterestCount() > 0);
	}

	template <typename T>
	InterestPtr(const SharedPtr<T>& item)
		: m_Item(item)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	// std::shared_ptr-backed wrapper (shared_tree_ptr) overloads, mirroring the SharedPtr ones above.
	template <typename T>
	InterestPtr(const shared_tree_ptr<T>& item)
		: m_Item(item)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}
	template <typename T>
	InterestPtr(shared_tree_ptr<T>&& item)
		: m_Item(std::move(item))
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	// std::weak_ptr-backed overload: store the weak handle and bump interest on the (momentarily locked)
	// target. Used for the supplier-interest list (InterestPtr<std::weak_ptr<const Actor>>): non-owning,
	// liveness-checked. get_ptr() locks; OptionalInterestDec at teardown is a no-op if the target expired.
	template <typename T>
	InterestPtr(const std::weak_ptr<T>& item)
		: m_Item(item)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	template <typename SrcPtr> requires std::is_constructible_v<CPtr, SrcPtr&&>
	InterestPtr(InterestPtr<SrcPtr>&& rhs) noexcept
		: m_Item(std::move(rhs.m_Item))
	{
		rhs.m_Item = SrcPtr();
	}
	template <typename SrcPtr> requires std::is_constructible_v<CPtr, const SrcPtr&>
	InterestPtr(const InterestPtr<SrcPtr>& rhs) noexcept
		: m_Item(rhs.m_Item)
	{
		OptionalInterestInc<IVal>(get_ptr());
	}

	InterestPtr& operator =(InterestPtr&& rhs) noexcept
	{
		omni::swap(m_Item, rhs.m_Item);
		return *this;
	}

	void operator =(const InterestPtr& rhs) noexcept
	{
		InterestPtr rhsCopy(rhs);
		omni::swap(m_Item, rhsCopy.m_Item);        // this line shouldn't throw
		 // fullfill the promise to Decrement old m_Item by destructing temporary rhs
	}

	template <typename SrcPtr>
	void operator =(SrcPtr&& rhs)
	{
		InterestPtr rhsCopy(std::forward<SrcPtr>(rhs));
		omni::swap(m_Item, rhsCopy.m_Item);        // this line shouldn't throw
		 // fullfill the promise to Decrement old m_Item by destructing temporary rhs
	}
	void release() noexcept { m_Item = CPtr(); } // remove responsibility for decrement

	~InterestPtr() noexcept
	{
		OptionalInterestDec<IVal>(get_ptr());
	}

	explicit operator bool() const { return get_ptr() != nullptr; }
	bool operator !() const { return get_ptr() == nullptr;  }

	operator IPtr() const requires(std::is_class_v<CPtr>) { return get_ptr(); }
	operator CPtr() const { return m_Item; }

	IPtr operator ->() const { assert(m_Item != nullptr); return  get_ptr(); }
	IRef operator * () const { assert(m_Item != nullptr); return *get_ptr(); }

	bool operator <  (const InterestPtr& right) const { return get_ptr() <  right.get_ptr(); }
	bool operator == (const InterestPtr& right) const { return get_ptr() == right.get_ptr(); }
	bool operator == (      IPtr      rightPtr) const { return get_ptr() == rightPtr; }

	bool has_ptr() const { return get_ptr() != nullptr; }
	bool is_null() const { return get_ptr() == nullptr; }

	IPtr get_ptr() const { return pointer_traits<CPtr>::get_ptr(m_Item); }
	void swap(InterestPtr& rhs) { omni::swap(m_Item, rhs.m_Item); }

private:
	CPtr m_Item;
	template<typename SrcPtr> friend struct InterestPtr;
};

//----------------------------------------------------------------------
// class  : Specific InterestPtrs
//----------------------------------------------------------------------



#endif // __RTC_PTR_INTERESTHOLDER_H
