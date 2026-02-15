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

#include "ptr/InterestHolders.h"

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
	SupplInterestListElem(SharedActorInterestPtr&& value, SupplInterestListPtr&& next)
		:	m_NextPtr(next.release())
		,	m_Value(std::move(value))
	{
		assert(m_Value); // class invariant
	}
	~SupplInterestListElem()
	{
		// provide assumptions to the optimizer of the member destructors
		assert(m_Value);              // class invariant
	}

	SupplInterestListElem* m_NextPtr;
	SharedActorInterestPtr m_Value;
};

//typedef SupplInterestListElem::first_ptr_type SupplInterestListPtr;

inline void push_front(SupplInterestListPtr& self, const SharedActor* actor)
{
	self = new SupplInterestListElem(SharedActorInterestPtr(actor, existing_obj{}), std::move(self));
};

typedef std::map<const Actor*, SupplInterestListPtr > SupplTreeInterestType;

#include "ptr/StaticPtr.h"

extern RTC_CALL static_ptr<SupplTreeInterestType> s_SupplTreeInterest;


#endif //!defined(__RTC_ACT_SUPPLINTEREST_H)
