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
#include "set/QuickContainers.h"
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
			if (supplier->IsPassorOrChecked())
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
