// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#include "StoragePCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)


// *****************************************************************************
//
// XdbStorageManager: reads fixed-width text records; XyzStorageManager is its only registered
// instance (see UpdateColInfo below for the .xyz layout). Writing is not supported.
//
// *****************************************************************************

#include "XdbStorageManager.h"
#include "xdb/XdbImp.h"

#include "stg/StorageClass.h"

#include "utl/StrFormat.h"
#include "ser/BaseStreamBuff.h"  
#include "dbg/debug.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "Param.h"

#include "TreeItemContextHandle.h"
#include "UnitClass.h"

#define MG_DEBUG_XDB false

FileResult XdbStorageManager::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	AbstrDataItem* adi = smi->CurrWD();
	dms_assert(!t);

	dms_assert( DoesExist(smi->StorageHolder()) );
	dms_assert(adi);

	XdbImp imp;
	UpdateColInfo(imp);

	auto result = imp.OpenForRead(GetNameStr(), m_DatExtension);
	if (!result)
		return result;

	assert(adi->GetDataObjLockCount() < 0);
	AbstrDataObject* ado = borrowedReadResultHolder;
	const ValueClass* vc = ado->GetValuesType();
	ValueClassID valueTypeID = vc->GetValueClassID();
	auto nr_cells = imp.NrOfRows();
	MG_CHECK(nr_cells == ado->GetNrFeaturesNow());

	auto colName = adi->GetRelativeName(smi->StorageHolder());
	auto colIndex = imp.ColIndex(colName.c_str());
	if (colIndex == UInt32(-1))
		adi->throwItemErrorF("no column '{}' in {}", colName, GetNameStr());

	// ReadColumn stores 4 or 8 bytes per row according to the COLUMN type into the ITEM's buffer;
	// an attribute of a narrower type (a uint8 on an xyz column) was a heap overflow.
	const ValueClass* colClass = ValueClass::FindByValueClassID(imp.ColType(colIndex));
	bool compatible = colClass && colClass->IsNumeric() && vc->IsNumeric()
		&& colClass->GetSize() == vc->GetSize() && colClass->IsIntegral() == vc->IsIntegral();
	if (!compatible)
		adi->throwItemErrorF("column '{}' of {} has type {}, which cannot be read into an attribute of type {}", colName, GetNameStr(), colClass ? colClass->GetNameID() : TokenID::GetEmptyID(), vc->GetNameID());

	return FileResult::require(
		imp.ReadColumn(
			reinterpret_cast<void *>(ado->GetDataWriteBegin(no_tile, dms_rw_mode::write_only_mustzero).get_ptr()),
			nr_cells,
			colIndex
		)
		, "failed to xdb.ReadColumn"
	);
}

FileResult XdbStorageManager::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	// The .xdb column-append path was retired long ago; until 2026-09 this opened the file for
	// writing and threw the same message from XdbImp::Open.
	throwErrorF("Xdb", "writing to {} is not supported", GetNameStr());
}

// Constructor for this implementation of the abstact storagemanager interface
XdbStorageManager::XdbStorageManager(CharPtr datExtension)
	:	m_DatExtension(datExtension)
{
}


bool XdbStorageManager::ReadUnitRange(const StorageMetaInfo& smi) const
{
	XdbImp imp;
	UpdateColInfo(imp);

	if (!imp.OpenForRead(GetNameStr(), m_DatExtension))
		return false;

	smi.CurrWU()->SetCount(imp.NrOfRows());
	return true;
}

void XdbStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	NonmappableStorageManager::DoUpdateTree(storageHolder, curr, sm);

	if (sm == SyncMode::None)
		return;

	dms_assert(storageHolder);
	if (storageHolder != curr)
		return;

	StorageReadHandle storageHandle(const_cast<XdbStorageManager*>(this), storageHolder, curr, StorageAction::updatetree);
	XdbImp imp;
	UpdateColInfo(imp);

	// Pick up the content of the file

	const AbstrUnit* u_row = StorageHolder_GetTableDomain(storageHolder);


	Int32 colcnt = imp.NrOfCols();
	for (Int32 i =0; i<colcnt; i++)
	{
		CharPtr colName = imp.ColName(i);
		TreeItem* di = curr->GetSubTreeItemByID(GetTokenID_mt(colName));
		AbstrDataItem* adi = NULL;
		if (di)
		{
			adi = checked_cast<AbstrDataItem*>(di);
			dms_assert(adi);
			if (adi->GetAbstrDomainUnit() != u_row)
				throwItemErrorF("Column {} is configured with an inconsistent domain unit", colName);
			if (!OverlappingTypes(
					adi->GetDynamicObjClass()->GetValuesType(), 
					ValueClass::FindByValueClassID(imp.ColType(i))))
				throwItemErrorF("Column {} is configured with a unit type that is incompatible with the xdb type {}", colName, int(imp.ColType(i)));
		}
		else if (sm != SyncMode::None)
		{
			const ValueClass* vc  = ValueClass::FindByValueClassID(imp.ColType(i));
			dms_assert(vc);
			const AbstrUnit * u_col = UnitClass::Find(vc)->CreateDefault();
			// Data item
			adi = CreateDataItem(curr, GetTokenID_mt(colName), u_row, u_col).get(); // owned by curr (parent)
		}
	}
}



void XdbStorageManager::UpdateColInfo(XdbImp& imp) const
{}

// Only XyzStorageManager is registered as a storage type; a plain "xdb" storage type no longer exists.

class XyzStorageManager : public XdbStorageManager
{
public:
	XyzStorageManager() : XdbStorageManager("xyz") 
	{}

	// An .xyz file is read as fixed-width records: three 12-byte fields (X, Y, Z as Float32 text)
	// plus a 1-byte line break, 33 bytes per line. The line length is not validated against the file.
	void UpdateColInfo(XdbImp& imp) const override
	{
		imp.nrows = -1;
		imp.nrheaderlines = 0;
		imp.headersize = 0;
		imp.m_RecSize = 32;
		imp.m_LineBreakSize = 1;
		imp.ColDescriptions.resize(3);
		imp.ColDescriptions[0].m_Name =  "X"; imp.ColDescriptions[0].m_Offset =  0; imp.ColDescriptions[0].m_Type = ValueClassID::VT_Float32;
		imp.ColDescriptions[1].m_Name =  "Y"; imp.ColDescriptions[1].m_Offset = 12; imp.ColDescriptions[1].m_Type = ValueClassID::VT_Float32;
		imp.ColDescriptions[2].m_Name =  "Z"; imp.ColDescriptions[2].m_Offset = 24; imp.ColDescriptions[2].m_Type = ValueClassID::VT_Float32;
	};

	DECL_RTTI(,StorageClass)
};

IMPL_DYNC_STORAGECLASS(XyzStorageManager, "xyz")
