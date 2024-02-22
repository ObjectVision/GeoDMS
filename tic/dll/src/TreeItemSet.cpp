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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "TreeItemSet.h"

#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "set/VectorFunc.h"

#include "AbstrCalculator.h"
#include "AbstrUnit.h"
#include "SessionData.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"


static const TreeItemVectorType* makeVector(const TreeItemSetType& itemSet)
{
	TreeItemVectorType* iv = new TreeItemVectorType; 
	iv->resize(itemSet.size());
	std::copy(itemSet.begin(), itemSet.end(), iv->begin());
	return iv;
}

// store unique suppl in itemSet
static void storeAllSuppliers(const TreeItem* self, SupplierVisitFlag svf, TreeItemSetType& itemSet)
{
	VisitSupplProcImpl(self, svf,
		[&] (const Actor* supplier)
		{
			if (supplier->IsPassorOrChecked())
				return;
			const TreeItem* suppl = dynamic_cast<const TreeItem*>(supplier);
			if (suppl)
			{
				TreeItemSetType::iterator i = itemSet.lower_bound(suppl);
				if (i == itemSet.end() || (*i) != suppl)
				{
					itemSet.insert(i, suppl);
					storeAllSuppliers(suppl, svf, itemSet);
				}
			}
		}
	);
}

static void storeAllSubItemSuppliers(const TreeItem* self, SupplierVisitFlag svf, TreeItemSetType& itemSet)
{
	storeAllSuppliers(self, svf, itemSet);
	for (const TreeItem* subItem = self->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		storeAllSubItemSuppliers(subItem, svf, itemSet);
}

#include "AbstrDataItem.h"

enum CSS_FLAGS
{
	CSS_MatchName   =    1,
	CSS_MatchDomain =    2,
	CSS_MatchValues =    4,
	CSS_ExactValues =    8,
	CSS_ExpandMeta  = 0x10,
	CSS_NoCaseParams = 0x20,
};

static bool isSimilarItem(const TreeItem* searchLoc, const TreeItem* pattern, CSS_FLAGS flags)
//	bool mustMatchDomainUnit, bool mustMatchValuesUnit, bool noCaseParameters, bool expandMetaInfo)
{
	MG_SIGNAL_ON_UPDATEMETAINFO

	if (searchLoc->IsFailed())
		return false;

	bool expandMetaInfo      = (flags & CSS_ExpandMeta);
	bool mustMatchDomainUnit = (flags & CSS_MatchDomain   );
	bool mustMatchValuesUnit = (flags & (CSS_MatchValues|CSS_ExactValues));

	const AbstrDataItem* pdi = AsDynamicDataItem(pattern);
	if (pdi)
	{
		const AbstrDataItem* sdi = AsDynamicDataItem(searchLoc);
		if (!sdi)
			return false;

		const AbstrUnit *svu = sdi->GetAbstrValuesUnit();
		const AbstrUnit *pvu = pdi->GetAbstrValuesUnit();

		if (pvu->GetDynamicObjClass() != svu->GetDynamicObjClass() )
			return false;

		const AbstrUnit *sdu = sdi->GetAbstrDomainUnit(); 
		const AbstrUnit *pdu = pdi->GetAbstrDomainUnit();

		if	(pdu->GetDynamicObjClass() != sdu->GetDynamicObjClass())
			return false;


		if	(	mustMatchDomainUnit
			&&	pdu != sdu 
			&&	(!expandMetaInfo || !pdu->UnifyDomain(sdu))
			)
			return false;

		if	(	mustMatchValuesUnit
			&&	pvu != svu
			&&	(!expandMetaInfo || !((flags & CSS_ExactValues) ? pvu->UnifyDomain(svu): pvu->UnifyValues(svu)))
			)
			return false;
	}
	if ((flags & CSS_NoCaseParams) && searchLoc->HasCalculator() && searchLoc->GetCalculator() && searchLoc->GetCalculator()->IsDcPtr())
		return false;

	return true;
}

static bool isSimilar(const TreeItem* searchLoc, const TreeItem* pattern, CSS_FLAGS flags)
{
	if (!isSimilarItem(searchLoc, pattern, flags))
		return false;

	pattern  = pattern->GetFirstSubItem();
	if ((flags & CSS_ExpandMeta) && pattern)
		searchLoc->UpdateMetaInfo();

	for (; pattern; pattern = pattern->GetNextItem())
	{
		const TreeItem* si = const_cast<TreeItem*>(searchLoc)->GetSubTreeItemByID(pattern->GetID());
		if (!si) 
			return false;

		if (!isSimilar(si, pattern, CSS_FLAGS(flags & ~CSS_NoCaseParams))) //mustMatchDomainUnit, mustMatchValuesUnit, false, expandMetaInfo))
			return false;
	}
	return true;
}

static void createSimilarSet(const TreeItem* searchLoc, const TreeItem* pattern, TreeItemVectorType& itemset, CSS_FLAGS flags)
{
	if (searchLoc->IsTemplate())
		return;
	if (!(flags & CSS_MatchName) || (!stricmp( searchLoc->GetName().c_str(), pattern->GetName().c_str())))
		if (isSimilar(searchLoc, pattern, CSS_FLAGS(flags | CSS_NoCaseParams)))
		{
			itemset.push_back(searchLoc);
			return;
		}

	searchLoc = 
		(flags & CSS_ExpandMeta)
		?	searchLoc->GetFirstSubItem ()  // calls UpdateMetaInfo
		:	searchLoc->_GetFirstSubItem(); // don't

	for (; searchLoc; searchLoc = searchLoc->GetNextItem())
		createSimilarSet(searchLoc, pattern, itemset, flags);
}

//----------------------------------------------------------------------
// extern "C" interface funcs
//----------------------------------------------------------------------

#include "TicInterface.h"

TIC_CALL const TreeItemVectorType* DMS_CONV DMS_TreeItem_CreateCompleteSupplierSet(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_CreateCompleteSupplierSet");

		TreeItemSetType is;
		storeAllSuppliers(self, SupplierVisitFlag::Inspect, is);
		return makeVector(is);

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItemVectorType* DMS_CONV DMS_TreeItem_CreateDirectConsumerSet(const TreeItem* self, bool expandMetaInfo)
{
	DMS_CALL_BEGIN

		throwIllegalAbstract(MG_POS, self, "DMS_TreeItem_CreateConsumerSet");

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItemVectorType* DMS_CONV DMS_TreeItem_CreateCompleteConsumerSet(const TreeItem* self, bool expandMetaInfo)
{
	DMS_CALL_BEGIN

		throwIllegalAbstract(MG_POS, self, "DMS_TreeItem_CreateConsumerSet");

	DMS_CALL_END
	return nullptr;
}

TIC_CALL void DMS_CONV DMS_TreeItemSet_Release(const TreeItemVectorType* self)
{
	DMS_CALL_BEGIN

		DBG_START("TreeItemSet", "Release", false);

		MG_PRECONDITION(self);
		delete self;

	DMS_CALL_END
}

TIC_CALL UInt32 DMS_CONV DMS_TreeItemSet_GetNrItems(const TreeItemVectorType* self)
{
	DMS_CALL_BEGIN

		DBG_START("TreeItemSet", "GetNrItems", false);

		MG_PRECONDITION(self);
		return self->size();

	DMS_CALL_END
	return 0;
}

TIC_CALL const TreeItem*    DMS_CONV DMS_TreeItemSet_GetItem   (const TreeItemVectorType* self, UInt32 i)
{
	DMS_CALL_BEGIN

		DBG_START("TreeItemSet", "GetItem", false);

		MG_PRECONDITION(self);
		return *(self->begin()+i);

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const TreeItemVectorType* DMS_CONV DMS_TreeItem_CreateSimilarItemSet(
	const TreeItem* pattern, const TreeItem* searchLoc,
	bool mustMatchName,       bool mustMatchDomainUnit,
	bool mustMatchValuesUnit, bool exactValuesUnit,
	bool expandMetaInfo)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(pattern, TreeItem::GetStaticClass(), "DMS_TreeItem_CreateSimilarItemSet");

		MG_SIGNAL_ON_UPDATEMETAINFO

		if (searchLoc == 0)
			searchLoc = SessionData::Curr()->GetConfigRoot();

		UInt32 flags = 0;
		if (mustMatchName      ) flags += CSS_MatchName;
		if (mustMatchDomainUnit) flags += CSS_MatchDomain;
		if (mustMatchValuesUnit) flags += CSS_MatchValues;
		if (exactValuesUnit    ) flags += CSS_ExactValues;
		if (expandMetaInfo     ) flags += CSS_ExpandMeta;
		
		OwningPtr<TreeItemVectorType> is = new TreeItemVectorType;
		createSimilarSet(searchLoc, pattern, is.get_ref(), static_cast<CSS_FLAGS>(flags));
		return is.release();

	DMS_CALL_END
	return nullptr;
}
