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
#include "LockLevels.h"
#include "act/garbage_can.h"
#include "ptr/SharedTreePtr.h" // make_shared_tree + construction tags for the TreeItem-family (std::shared_ptr) overloads
#include "utl/swap.h"

// discriminates the TreeItem-family CPtr case: std::shared_ptr must recover the EXISTING control
// block via make_shared_tree (a raw-pointer std::shared_ptr ctor would create a rogue second one).
template <typename P> struct is_std_shared_ptr : std::false_type {};
template <typename T> struct is_std_shared_ptr<std::shared_ptr<T>> : std::true_type {};
template <typename P> constexpr bool is_std_shared_ptr_v = is_std_shared_ptr<P>::value;
template <typename P> struct is_std_weak_ptr : std::false_type {};
template <typename T> struct is_std_weak_ptr<std::weak_ptr<T>> : std::true_type {};
template <typename P> constexpr bool is_std_weak_ptr_v = is_std_weak_ptr<P>::value;


//----------------------------------------------------------------------
// class  : OptionalInterest
//----------------------------------------------------------------------

// The per-item declaration sits AFTER the null check: an empty interest pointer is created and destroyed
// under sg_CountSection all the time (GetInterestPtrOrNull returns one), and takes nothing.
template <typename T> void OptionalInterestInc(T* ptr)          { if (ptr) { DMS_ENTERS_ITEM(ord_level_type::ItemRegister, dms_exclusive_v); ptr->IncInterestCount(); } }
template <typename T> garbage_can OptionalInterestDec(T* ptr) noexcept { if (!ptr) return {}; DMS_ENTERS_ITEM(ord_level_type::ItemRegister, dms_exclusive_v); return ptr->DecInterestCount(); }

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
		inc_interest();
	}

	template <typename T>
	InterestPtr(T* ptr)
		: m_Item()
	{
		if (ptr)
		{
			// CPtr may be a raw pointer, an intrusive SharedPtr, or a std::shared_ptr (TreeItem family).
			// The latter must recover the EXISTING control block (make_shared_tree/shared_from_this);
			// std::shared_ptr's own raw-pointer ctor would create a rogue second control block.
			if constexpr (is_std_shared_ptr_v<CPtr>)
				m_Item = make_shared_tree(ptr, existing_obj{});
			else if constexpr (is_std_weak_ptr_v<CPtr>)
				m_Item = make_weak_tree(ptr);
			else
				m_Item = CPtr(ptr);
		}
		inc_interest();
	}

	template <typename T, typename OwnershipTag>
	InterestPtr(T* ptr, OwnershipTag tag)
		: m_Item()
	{
		if constexpr (is_std_shared_ptr_v<CPtr>)
			m_Item = make_shared_tree(ptr, tag);
		else if constexpr (is_std_weak_ptr_v<CPtr>)
			m_Item = make_weak_tree(ptr); // weak borrow; the ownership tag is only meaningful for owning targets
		else
			m_Item = CPtr(ptr, tag);
		inc_interest();
	}
	template <typename T>
	InterestPtr(SharedPtr<T>&& item)
		: m_Item(std::move(item))
	{
		inc_interest();
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
		inc_interest();
	}

	// std::shared_ptr (TreeItem family) overloads, mirroring the SharedPtr ones above.
	template <typename T>
	InterestPtr(const std::shared_ptr<T>& item)
		: m_Item(item)
	{
		inc_interest();
	}
	template <typename T>
	InterestPtr(std::shared_ptr<T>&& item)
		: m_Item(std::move(item))
	{
		inc_interest();
	}
	// std::shared_ptr-backed counterpart of the SharedPtr already_incremented_tag ctor: adopt an owning
	// std::shared_ptr whose interest the caller has ALREADY incremented (e.g. under sg_CountSection), so do
	// not increment again here.
	template <typename T>
	InterestPtr(std::shared_ptr<T>&& item, already_incremented_tag)
		: m_Item(std::move(item))
	{
		assert(get_ptr() != nullptr && get_ptr()->GetInterestCount() > 0);
	}

	// std::weak_ptr-backed overload: store the weak handle and bump interest on the (momentarily locked)
	// target. Used for the supplier-interest list (InterestPtr<std::weak_ptr<const Actor>>): non-owning,
	// liveness-checked. get_ptr() locks; OptionalInterestDec at teardown is a no-op if the target expired.
	template <typename T>
	InterestPtr(const std::weak_ptr<T>& item)
		: m_Item(item)
	{
		inc_interest();
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
		inc_interest();
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
		dec_interest();
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
	// For a weak CPtr the target must stay owned ACROSS the count mutation: get_ptr()'s momentary
	// lock dies before the Inc/Dec call, leaving an unowned raw. Expired targets are skipped.
	void inc_interest() const
	{
		if constexpr (is_std_weak_ptr_v<CPtr>)
		{
			if (auto p = m_Item.lock())
				OptionalInterestInc<IVal>(p.get());
		}
		else
			OptionalInterestInc<IVal>(get_ptr());
	}
	void dec_interest() const noexcept
	{
		if constexpr (is_std_weak_ptr_v<CPtr>)
		{
			if (auto p = m_Item.lock())
				OptionalInterestDec<IVal>(p.get());
		}
		else
			OptionalInterestDec<IVal>(get_ptr());
	}

	CPtr m_Item;
	template<typename SrcPtr> friend struct InterestPtr;
};

//----------------------------------------------------------------------
// class  : Specific InterestPtrs
//----------------------------------------------------------------------



#endif // __RTC_PTR_INTERESTHOLDER_H
