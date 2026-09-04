// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// CreateTreeItemColumnInfo: create/check the tree items for a storage's
// table columns. Split from stg DllMain.cpp (2026-08).

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

// ------------------------------------------------------------------------
// Implementation of CreateTreeItemColumnInfo
// ------------------------------------------------------------------------

bool TreeItemIsColumn(TreeItem *ti)
{
	return IsDataItem(ti) && ! AsDataItem(ti)->HasVoidDomainGuarantee();
}

bool CompatibleTypes(const ValueClass* dbCls, const ValueClass* configCls)
{
	return dbCls && configCls &&
		(	(dbCls->IsNumericOrBool() && configCls->IsNumericOrBool())
		||	dbCls == configCls);
}

bool CreateTreeItemColumnInfo(TreeItem* tiTable, CharPtr colName, const AbstrUnit *domainUnit, const ValueClass *dbValuesClass, ValueComposition vc)
{
	if (dbValuesClass == nullptr)
		return false;

	TreeItem* tiColumn = TreeItem_CheckCls(tiTable->GetItem(colName), AbstrDataItem::GetStaticClass());

	if (tiColumn)
	{
		if (!TreeItemIsColumn(tiColumn))
			return false;
		const ValueClass *vCls = AsDataItem(tiColumn)->GetAbstrValuesUnit()->GetValueType();
		bool res = CompatibleTypes(dbValuesClass, vCls);
		if (!res)
		{
			auto msg = mySSPrintF("StorageManager: inconsistent value types; table: {}, column: {}, configured type: {}, database type: {}",
				tiTable->GetFullName(),
				colName,
				vCls->GetNameID(),
				dbValuesClass->GetNameID()
			);
			tiColumn->Fail(msg, FailType::Data);
		}
		return res;
	}
	const AbstrUnit* valuesUnit = UnitClass::Find(dbValuesClass)->CreateDefault();
	assert(valuesUnit);
	return CreateDataItem(tiTable, GetTokenID_mt(colName),	domainUnit, valuesUnit, vc) != nullptr;
}

