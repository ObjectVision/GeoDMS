// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#pragma once

#if !defined(__RTC_ACT_SUPPLINTEREST_H)
#define __RTC_ACT_SUPPLINTEREST_H

#include <map>

#include "act/Actor.h"           // Actor, weak_from_actor()
#include "ptr/InterestHolders.h"

//  -----------------------------------------------------------------------
// The supplier-interest list holds a NON-owning, liveness-checked interest on each std-managed supplier
// (TreeItems, via weak_from_actor()). DataController suppliers (intrusive SharedActor) are deliberately NOT
// retained: their interest is already held by the ownership chain -- a TreeItem's StartInterest holds its
// mc_DC (=GetDC()), and a FuncDC's StartInterest holds its arg DCs (=GetArgDC()); and FuncDC::VisitSuppliers
// visits each arg DC's RESULT TreeItem too, so holding that result's interest re-holds the DC transitively.
// They yield an empty weak from weak_from_actor() and are skipped by push_front.
//  -----------------------------------------------------------------------

using SupplierInterestPtr = InterestPtr<std::weak_ptr<const Actor> >;

//  -----------------------------------------------------------------------
// SupplInterestListElem
//  -----------------------------------------------------------------------

template <typename Ptr>
Ptr release_ptr(Ptr& p)
{
	Ptr tmp = p;
	p = nullptr; // service precondition of destructor of SupplInterestListElem
	return tmp;
}

struct SupplInterestListElem;

struct SupplInterestListPtr: private std::unique_ptr<SupplInterestListElem>
{
	using base_type = std::unique_ptr<SupplInterestListElem>;
	SupplInterestListPtr() noexcept
	{}

	SupplInterestListPtr(SupplInterestListPtr&& rhs) noexcept
		: base_type(std::move(rhs))
	{}

	RTC_CALL ~SupplInterestListPtr() noexcept;

	void init(SupplInterestListElem* ptr)
	{
		base_type::reset(ptr);
	}

	void operator =(SupplInterestListElem* ptr)
	{
		assert(ptr != get());
		base_type src = std::move(*this);
		assert(get() == nullptr);
		base_type::reset(ptr);
	}

	using base_type::release;
	using base_type::operator bool;

	operator SupplInterestListElem* () const noexcept { return get(); }
};

struct SupplInterestListElem
{
	SupplInterestListElem(SupplierInterestPtr&& value, SupplInterestListPtr&& next)
		:	m_NextPtr(next.release())
		,	m_Value(std::move(value))
	{
		assert(m_Value); // invariant at insertion: push_front only inserts a live (non-expired) supplier
	}
	~SupplInterestListElem()
	{
		// NB: m_Value is a non-owning weak interest; the supplier MAY have expired by now (the InterestPtr
		// dtor decrements only if still live), so the "always non-null" invariant no longer holds here.
	}

	SupplInterestListElem* m_NextPtr;
	SupplierInterestPtr    m_Value;
};

//typedef SupplInterestListElem::first_ptr_type SupplInterestListPtr;

// Retain a non-owning, liveness-checked interest on a std-managed supplier (TreeItem). DataControllers
// yield an empty weak (not std-managed) and are skipped -- their interest rides on the ownership chain
// (their result TreeItems, also visited). InterestPtr<std::weak_ptr> bumps/decrements the interest count.
inline void push_front(SupplInterestListPtr& self, const Actor* actor)
{
	auto w = actor ? actor->weak_from_actor() : std::weak_ptr<const Actor>{};
	if (w.expired())
		return;
	self = new SupplInterestListElem(SupplierInterestPtr(w), std::move(self));
};

typedef std::map<const Actor*, SupplInterestListPtr > SupplTreeInterestType;

#include "ptr/StaticPtr.h"

extern RTC_CALL static_ptr<SupplTreeInterestType> s_SupplTreeInterest;


#endif //!defined(__RTC_ACT_SUPPLINTEREST_H)
