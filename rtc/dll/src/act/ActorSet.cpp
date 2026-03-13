// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "act/ActorSet.h"

#if defined(MG_DEBUG_UPDATESOURCE)

#include "act/Actor.h"
#include "act/ActorVisitor.h"
#include "act/UpdateMark.h"
#include "dbg/SeverityType.h"
#include "set/VectorFunc.h"

#include <algorithm>

// *****************************************************************************
// Section:     ActorSet helper funcs for SupplInclusionTester
// *****************************************************************************

class ActorSetType : public std::set<const Actor*>  {};

static void storeAllSuppliers(const Actor* self, SupplierVisitFlags svf, ActorSetType& itemSet)
{
	VisitSupplProcImpl(self, svf, 
		[&] (const Actor* supplier)
		{
			if (supplier->IsPassor())
				return;
			ActorSetType::iterator i = itemSet.lower_bound(supplier);
			if (i == itemSet.end() || (*i) != supplier)
			{
				dms_assert(supplier->m_LastGetStateTS >= UpdateMarker::LastTS() );
				itemSet.insert(i, supplier);
				storeAllSuppliers(supplier, svf, itemSet);
			}
		}
	);
}

static void GetCompleteSupplierSet(const Actor* self, SupplierVisitFlags svf, ActorVectorType& buffer)
{
	ActorSetType supplSet;
	dms_assert(self->m_LastGetStateTS >= UpdateMarker::LastTS() );
	supplSet.insert(self);
	storeAllSuppliers(self, svf, supplSet);
	set2vector(supplSet, buffer);
}

// *****************************************************************************
// Section:     MG_DEBUG_UPDATESOURCE
// *****************************************************************************

static SupplInclusionTester* s_Active = 0;

SupplInclusionTester::SupplInclusionTester(const Actor* actor): m_Prev(s_Active)
{
	GetCompleteSupplierSet(actor, *this);
	if (s_Active)
	{
		// report suppliers that were not included in s_Active
		ActorVectorType::const_iterator
			f1 = s_Active->begin(), f2 = begin(),
			l1 = s_Active->end  (), l2 = end();
		while (true)
		{
			if (f2 == l2)
				break;
			if (f1==l1 || *f2 < *f1)
			{
				reportD(ST_MajorTrace, "Intransitive supplier: ", (*f2)->GetSourceName().c_str());
				dms_assert(0);
				++f2;
			}
			else if (*f1< *f2)
				++f1;
			else
			{	// advance both
				++f1;
				++f2;
			}
		}
		dms_assert(std::includes(s_Active->begin(), s_Active->end(), begin(), end()) );
	}
	s_Active = this;
}

SupplInclusionTester::~SupplInclusionTester()
{
	s_Active = m_Prev;
}

bool SupplInclusionTester::ActiveDoesContain(const Actor* actor)
{
	return !s_Active 
		|| std::binary_search(s_Active->begin(), s_Active->end(), actor);
}

#endif // defined(MG_DEBUG_UPDATESOURCE)
