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
// The supplier-interest list retains interest on each (non-passor) supplier for as long as the consumer
// holds supplier interest. It holds suppliers in one of two ways, by ownership kind:
//  - std-managed suppliers (TreeItems, weak_from_actor() returns a live weak): a NON-owning, liveness-checked
//    weak interest. Non-owning is essential -- an owning ref here would recreate the config-root retain cycle
//    (supplier -> ancestor -> ... -> root) that this migration removed.
//  - intrusive suppliers (DataControllers; weak_from_actor() is empty because they are not std-managed): an
//    OWNING intrusive interest (SharedActorInterestPtr), exactly as the pre-std-ptr design did for ALL
//    suppliers. This is REQUIRED: a FuncDC's arg DC must have interest while its data is calculated
//    (assert in FuncDC::GetArgs), and the transitive path "hold the arg's RESULT TreeItem ->
//    result.StartInterest holds result.mc_DC" only re-holds the DC when the result is a cache item; for an
//    arg that resolves to a CONFIG item the result has no mc_DC, so the DC would otherwise get no interest.
//    DataControllers do not own the config tree, so this owning ref creates no root retain cycle.
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

	~SupplInterestListPtr() noexcept;

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
	SupplInterestListElem(SupplierInterestPtr&& value, SharedActorInterestPtr&& dcValue, SupplInterestListPtr&& next)
		:	m_NextPtr(next.release())
		,	m_Value(std::move(value))
		,	m_DcValue(std::move(dcValue))
	{
		assert(m_Value || m_DcValue); // invariant at insertion: a live supplier (weak std OR owning intrusive DC)
	}
	~SupplInterestListElem()
	{
		// NB: m_Value is a non-owning weak interest; the std supplier MAY have expired by now (the InterestPtr
		// dtor decrements only if still live). m_DcValue is an owning intrusive interest on a DataController
		// supplier, so that DC is still alive here and its interest is safely decremented.
	}

	SupplInterestListElem* m_NextPtr;
	SupplierInterestPtr    m_Value;    // weak interest on a std-managed (TreeItem) supplier; empty for DC suppliers
	SharedActorInterestPtr m_DcValue;  // owning intrusive interest on a DataController supplier; empty for std suppliers
};

//typedef SupplInterestListElem::first_ptr_type SupplInterestListPtr;

// Retain interest on a supplier for the consumer's supplier-interest lifetime. std-managed suppliers
// (TreeItems) are held by a non-owning, liveness-checked weak interest; intrusive suppliers (DataControllers,
// which yield an empty weak) are held by an OWNING intrusive interest (see the header comment above for why
// the DC must be retained here and why owning it is safe).
inline void push_front(SupplInterestListPtr& self, const Actor* actor)
{
	if (!actor)
		return;
	auto w = actor->weak_from_actor();
	if (!w.expired())
		self = new SupplInterestListElem(SupplierInterestPtr(w), SharedActorInterestPtr(), std::move(self));
	else if (const auto* sa = dynamic_cast<const SharedActor*>(actor))
		self = new SupplInterestListElem(SupplierInterestPtr(), SharedActorInterestPtr(sa), std::move(self));
	// else: a non-std, non-SharedActor Actor (not expected as a supplier) -> nothing to retain.
};

typedef std::map<const Actor*, SupplInterestListPtr > SupplTreeInterestType;

#include "ptr/StaticPtr.h"

extern static_ptr<SupplTreeInterestType> s_SupplTreeInterest;


#endif //!defined(__RTC_ACT_SUPPLINTEREST_H)
