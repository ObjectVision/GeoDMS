//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

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

struct SupplInterestListPtr: private OwningPtr<SupplInterestListElem>
{
	SupplInterestListPtr() noexcept
	{}

	SupplInterestListPtr(SupplInterestListPtr&& rhs) noexcept
		:	OwningPtr(std::move(rhs))
	{}

	RTC_CALL ~SupplInterestListPtr() noexcept;

	void init(SupplInterestListElem* ptr)
	{
		OwningPtr::init(ptr);
	}

	void operator =(SupplInterestListElem* ptr)
	{
		assert(ptr != get_ptr());
		OwningPtr<SupplInterestListElem> src = release();
		OwningPtr::init(ptr);
	}

	using OwningPtr::release;
	operator SupplInterestListElem* () const noexcept { return get_ptr(); }
};

struct SupplInterestListElem
{
	SupplInterestListElem(SharedActorInterestPtr&& value, SupplInterestListPtr&& next)
		:	m_NextPtr(next.release())
		,	m_Value(std::move(value))
	{
		dms_assert(m_Value); // class invariant
	}
	~SupplInterestListElem()
	{
		// provide assumptions to the optimizer of the member destructors
		dms_assert(m_Value);              // class invariant
		dms_assert(m_NextPtr == nullptr); // PRECONDITION for destruction to prevent excessive stack usage.
	}

	SupplInterestListElem* m_NextPtr;
	SharedActorInterestPtr m_Value;
};

//typedef SupplInterestListElem::first_ptr_type SupplInterestListPtr;

inline void push_front(SupplInterestListPtr& self, const Actor* actor)
{
	self = new SupplInterestListElem(SharedActorInterestPtr(actor), std::move(self));
};

typedef std::map<const Actor*, SupplInterestListPtr > SupplTreeInterestType;

#include "ptr/StaticPtr.h"

extern RTC_CALL static_ptr<SupplTreeInterestType> s_SupplTreeInterest;


#endif //!defined(__RTC_ACT_SUPPLINTEREST_H)
