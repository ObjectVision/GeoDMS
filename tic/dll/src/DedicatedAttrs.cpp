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
#include "TicPCH.h"
#pragma hdrstop

#include "DedicatedAttrs.h"

#include "act/ActorVisitor.h"
#include "dbg/DmsCatch.h"

#include "AbstrDataItem.h"
#include "Unit.h"
#include "UsingCache.h"
#include "TreeItemContextHandle.h"
#include "TreeItemSet.h"

//----------------------------------------------------------------------
// C style Interface functions for construction

//----------------------------------------------------------------------


bool IsNewItem(const TreeItem* self, TreeItemSetType& doneItems)
{
	TreeItemSetType::iterator itemPtr = doneItems.lower_bound(self);
	if (itemPtr != doneItems.end() && *itemPtr == self)
		return false;
	doneItems.insert(itemPtr, self);
	return true;
}

template <typename Func>
void VisitAllVisibleSubItems(const TreeItem* context, Func f, TreeItemSetType& doneItems = TreeItemSetType())
{
	if (!IsNewItem(context, doneItems))
		return;
	f(context);

	for (const TreeItem* subItem = context->GetFirstVisibleSubItem(); subItem; subItem = subItem->GetNextVisibleItem())
		if (IsNewItem(subItem, doneItems))
			f(subItem);
		
	if (context->CurrHasUsingCache())
	{
		auto uc = context->GetUsingCache();
		UInt32 i=uc->GetNrUsings();
		if(!i)
		{
			dms_assert(!context->GetTreeParent());
			return;
		}
		while (true)
		{
			if (!--i)
			{
				dms_assert(!context->GetTreeParent() || context->GetTreeParent() == uc->GetUsing(0));
				break;
			}
			VisitAllVisibleSubItems<Func>(uc->GetUsing(i), f, doneItems);
		}
	}
	const TreeItem* parent = context->GetTreeParent();
	if (parent)
		VisitAllVisibleSubItems<Func>(parent, f, doneItems);
}

// ==================================== dedicated query functions
#include "mci/ValueClass.h"

#include "SessionData.h"
#include "PropFuncs.h"
#include "TreeItemSet.h"
#include "UnitClass.h"
#include "UsingCache.h"

template <typename DerivedProcVisitor>
void VisitConstVisibleSubTree(const TreeItem* container, DerivedProcVisitor& v)
{
	if (container)
		container->VisitConstVisibleSubTree(v);
}

template <typename DerivedProcVisitor>
void VisitConstVisibleSubTrees(const TreeItem* context, DerivedProcVisitor&& v)
{
	VisitConstVisibleSubTree(SessionData::Curr()->GetClassificationContainer(context), v);
	VisitConstVisibleSubTree(SessionData::Curr()->GetActiveDesktop(),                  v);
}

TIC_CALL UInt32 DMS_CONV DMS_DataItem_VisitClassBreakCandidates(const AbstrDataItem* context, TSupplCallbackFunc callback, ClientHandle clientHandle)
{
	UInt32 count = 0;

	DMS_CALL_BEGIN

		const AbstrUnit* valuesUnit = context->GetAbstrValuesUnit();

		auto f = 
			[clientHandle, callback, valuesUnit, &count](const TreeItem* candidate)->void
			{
				dms_assert(candidate);

				if (candidate->IsCacheItem())
					return;
				if (!IsDataItem(candidate))
					return;
				auto adi = AsDataItem(candidate);
				auto candidateDomainUnit = adi->GetAbstrDomainUnit();
				const AbstrUnit* candidateValuesUnit = adi->GetAbstrValuesUnit();
				auto domainValueType = candidateDomainUnit->GetValueType();
				if (domainValueType->GetNrDims() != 1) // no 2d raster domain
					return;
				if (domainValueType->GetBitSize() > 8 && candidateValuesUnit->GetValueType()->IsIntegral()) // no big palettes unless candidate is base grid
					return;

				candidate->UpdateMetaInfo();
				valuesUnit->UpdateMetaInfo();
				if (valuesUnit->UnifyValues(candidateValuesUnit, "ClassBreak values", "Candidate Values", UM_AllowDefaultLeft)) // metric and value-type compatible
				if (!candidateValuesUnit->GetValueType()->IsIntegral() // base grid ?
					|| IsClassBreakAttr(candidate) // class breaks ?
					|| valuesUnit->UnifyDomain(candidateValuesUnit, "ClassBreak values", "Candidate Domain", UnifyMode())) // rlookup possible ?
					if (callback(clientHandle, candidate))
							++count;
			};
	
		TreeItemSetType doneItems;

		if (auto candidate = GetCdfAttr(context))
			VisitAllVisibleSubItems(candidate, f, doneItems);

		if (auto candidate = GetCdfAttr(valuesUnit))
			VisitAllVisibleSubItems(candidate, f, doneItems);

		VisitAllVisibleSubItems(context,    f, doneItems);
		VisitAllVisibleSubItems(valuesUnit, f, doneItems);

		auto newF =
			[&doneItems, &f](const Actor* aCandidate)->void
		{
			try {
				auto candidate = debug_cast<const TreeItem*>(aCandidate);
				if (candidate && IsNewItem(candidate, doneItems))
					f(candidate);
			}
			catch (const concurrency::task_canceled&)
			{
				throw;
			}
			catch (...) {}
		};

		VisitConstVisibleSubTrees( context, MakeDerivedProcVisitor(std::move(newF)) );

	DMS_CALL_END

	return count;
}

UInt32 DMS_CONV DMS_DomainUnit_VisitPaletteCandidates (const AbstrUnit* domain, TSupplCallbackFunc callback, ClientHandle clientHandle)
{
	UInt32 count = 0;

	DMS_CALL_BEGIN
	
		if (!domain)
			return count;

		auto f = 
			[clientHandle, callback, domain, &count](const TreeItem* candidate)->void
			{
				dms_assert(candidate);
				if(		!candidate->IsCacheItem()
					&&	IsDataItem(candidate) 
					&&	AsDataItem(candidate)->GetAbstrDomainUnit()->UnifyDomain(domain)
					&&	AsDataItem(candidate)->GetAbstrValuesUnit()->IsKindOf(Unit<UInt32>::GetStaticClass())
					&&	callback(clientHandle, candidate)
				)
					++count;
			};

		TreeItemSetType doneItems;
		auto newF =
			[&doneItems, &f](const Actor* aCandidate)->void
			{
				auto candidate = debug_cast<const TreeItem*>(aCandidate);
				if (candidate && IsNewItem(candidate, doneItems))
				{
					try {
						f(candidate);
					}
					catch (...) {}
				}
			};

		VisitAllVisibleSubItems(domain,    f, doneItems);

		VisitConstVisibleSubTrees( domain, MakeDerivedProcVisitor(std::move(newF)) );

	DMS_CALL_END

	return count;
}

// ========================== Start OBSOLETE code, FIND REMOVE SIMPLIFY DataStructures for getting this data

extern "C" {

TIC_CALL UInt32         DMS_CONV DMS_Unit_GetNrDataItemsIn (const AbstrUnit* self)
{
	DMS_CALL_BEGIN

		throwIllegalAbstract(MG_POS, self, "DMS_Unit_GetNrDataItemsIn OBSOLETE");

	DMS_CALL_END
	return 0;
}

TIC_CALL const AbstrDataItem* DMS_CONV DMS_Unit_GetDataItemIn   (const AbstrUnit* self, UInt32 i)
{
	DMS_CALL_BEGIN

		throwIllegalAbstract(MG_POS, self, "DMS_Unit_GetDataItemIn OBSOLETE");

	DMS_CALL_END
	return 0;
}

TIC_CALL UInt32         DMS_CONV DMS_Unit_GetNrDataItemsOut(const AbstrUnit* self)
{
	DMS_CALL_BEGIN

		throwIllegalAbstract(MG_POS, self, "DMS_Unit_GetNrDataItemsOut OBSOLETE");

	DMS_CALL_END
	return 0;
}

TIC_CALL const AbstrDataItem* DMS_CONV DMS_Unit_GetDataItemOut(const AbstrUnit* self, UInt32 i)
{
	DMS_CALL_BEGIN

		throwIllegalAbstract(MG_POS, self, "DMS_Unit_GetDataItemOut OBSOLETE");

	DMS_CALL_END
	return 0;
}

}