// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "dbg/DebugCast.h"
#include "dbg/DmsCatch.h"
#include "vt/Round.h"
#include "geom/Transform.h"
#include "vt/Conversions.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"
#include "utl/StrFormat.h"
#include "utl/Encodes.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "Metric.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "TreeItemClass.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#if defined(_MSC_VER)
#include <minmax.h>
#include <share.h>
#endif

#include "NameSet.h"
#include "ViewPortInfoEx.h"
#include "GridStorageManager.h"
extern RTC_CALL bool s_IsDetectingIncInterest; // defined in rtc act/Actor.cpp; extern is required: without it this is a tentative definition that silently creates a private copy if the symbol ever stops being dllimport

// DMS_Stg_Load, stream-type helpers and the table-domain validation used by
// the table-oriented storage managers. The geo-ref file I/O, TNameSet,
// CreateTreeItemColumnInfo and ViewPortInfoEx implementations were split into
// GeoRef.cpp / NameSet.cpp / TreeItemColumnInfo.cpp / ViewPortInfoEx.cpp
// (2026-08).

STGDLL_CALL void DMS_Stg_Load()
{
}


const ValueClass* GetStreamType(const AbstrDataObject* ado)
{
	return ado->GetValuesType();
}

const ValueClass* GetStreamType(const AbstrDataItem* adi)
{
	return adi->GetDynamicObjClass()->GetValuesType();
}

const AbstrDataItem* TreeItem_AsColumnItem(const TreeItem* ti, bool allowVoid, bool readOnly)
{
	const AbstrDataItem* adi = AsDynamicDataItem(ti);
	if (!adi ||  adi->IsDisabledStorage() || (readOnly && adi->HasCalculator()))
		return nullptr;
	if (!allowVoid && adi->GetAbstrDomainUnit()->GetValueType() == ValueWrap<Void>::GetStaticClass())
		return nullptr;
	return adi;
}

bool TableDomain_IsAttr(const AbstrUnit* domain, const AbstrDataItem* adi)
{
	dms_assert(domain);
	return adi && adi ->GetAbstrDomainUnit() == domain;
}

void TableDomain_TestDataItem(const AbstrUnit*& domain, const TreeItem* ti, bool allowVoid, bool readOnly)
{
	const AbstrDataItem* dataItem = TreeItem_AsColumnItem(ti, allowVoid, readOnly);
	if (dataItem)
	{
		if (domain && !TableDomain_IsAttr(domain, dataItem))
			dataItem->Fail("storageManager requires all dataItems to have the same domain", FailType::Data);
		else
			domain = dataItem->GetAbstrDomainUnit();
	}
}

const AbstrUnit* StorageHolder_GetTableDomain(const TreeItem* storageHolder)
{
	bool readOnly = storageReadOnlyPropDefPtr->GetValue(storageHolder);
	const AbstrUnit* domain = nullptr;

	for (	
			const TreeItem* subItem = storageHolder->_GetFirstSubItem();  // prevent recursive call to UpdateMetaInfo
			subItem; 
			subItem = subItem->GetNextItem()
		)
		TableDomain_TestDataItem(domain, subItem, false, readOnly);
	if (domain) goto exit;

	domain = const_unit_dynacast<UInt32>(storageHolder);
	if (domain) goto exit;

	TableDomain_TestDataItem(domain, storageHolder, true, readOnly);
	if (domain) goto exit;

	for (	
			const TreeItem* subItem = storageHolder->_GetFirstSubItem();  // prevent recursive call to UpdateMetaInfo
			subItem; 
			subItem = subItem->GetNextItem()
		)
	{
		TableDomain_TestDataItem(domain, subItem, true, readOnly);
		if (domain) goto exit;
	}
	storageHolder->ThrowFail("Storage expected an unabiguous domain", FailType::MetaInfo);

exit:
	dms_assert(domain); // POSTCONDITION
	return domain;
}


