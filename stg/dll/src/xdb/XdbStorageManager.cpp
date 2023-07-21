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
#include "StoragePCH.h"
#pragma hdrstop


// *****************************************************************************
//
// Implementations of - XdbStorageOutStreamBuff
//                    - XdbStorageInpStreamBuff
//                    - XdbStorageManager
//
// *****************************************************************************

#include "XdbStorageManager.h"
#include "xdb/XdbImp.h"

#include "stg/StorageClass.h"

#include "utl/mySPrintF.h"
#include "ser/BaseStreamBuff.h"  
#include "dbg/debug.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataStoreManagerCaller.h"
#include "Param.h"

#include "TreeItemContextHandle.h"
#include "UnitClass.h"

#define MG_DEBUG_XDB false

bool XdbStorageManager::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	AbstrDataItem* adi = smi->CurrWD();
	dms_assert(!t);

	dms_assert( DoesExist(smi->StorageHolder()) );
	dms_assert(adi);

	XdbImp imp;
	UpdateColInfo(imp);

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return false;
	bool result = imp.OpenForRead(GetNameStr(), sfwa.get(), m_DatExtension, m_SaveColInfo);

	MG_CHECK2(result, "Cannot open Xdb for reading");

	dms_assert(adi->GetDataObjLockCount() < 0);
	AbstrDataObject* ado = borrowedReadResultHolder;
	const ValueClass* vc = ado->GetValuesType();
	ValueClassID valueTypeID = vc->GetValueClassID();
	auto nr_cells = imp.NrOfRows();
	MG_CHECK(nr_cells == ado->GetNrFeaturesNow());

	return imp.ReadColumn(
		reinterpret_cast<void *>(ado->GetDataWriteBegin().get_ptr()),
		nr_cells, 
		imp.ColIndex(adi->GetRelativeName(smi->StorageHolder()).c_str())
	);
}

bool XdbStorageManager::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	auto smi = smiHolder.get();
	StorageWriteHandle storageHandle(std::move(smiHolder));

	XdbImp imp;
	UpdateColInfo(imp);

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return false;
	bool result = imp.Open  (GetNameStr(), sfwa.get(), FCM_OpenRwFixed, m_DatExtension, m_SaveColInfo);
	if (!result)
		result  = imp.Create(GetNameStr(), sfwa.get(), m_DatExtension, m_SaveColInfo);
	MG_CHECK2(result, "Cannot open Xdb");

	const AbstrDataItem* adi = smi->CurrRD();

	assert(adi->GetDataRefLockCount());

	const AbstrDataObject* ado = adi->GetRefObj();
	ValueClassID  vclsid = ado->GetValuesType()->GetValueClassID();

	MG_CHECK(ado->GetNrFeaturesNow() == imp.NrOfRows());

	return imp.WriteColumn(
		ado->GetDataReadBegin()
	,	ado->GetNrFeaturesNow()
	,	imp.ColIndex(adi->GetRelativeName(smi->StorageHolder()).c_str())
	);
}


// Constructor for this implementation of the abstact storagemanager interface
XdbStorageManager::XdbStorageManager(CharPtr datExtension, bool saveColInfo)
	:	m_DatExtension(datExtension)
	,	m_SaveColInfo(saveColInfo)
{
}


bool XdbStorageManager::ReadUnitRange(const StorageMetaInfo& smi) const
{
	XdbImp imp;
	UpdateColInfo(imp);

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return false;

	if (!imp.OpenForRead(GetNameStr(), sfwa.get(), m_DatExtension, m_SaveColInfo))
		return false;

	smi.CurrWU()->SetCount(imp.NrOfRows());
	return true;
}

void XdbStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	AbstrStorageManager::DoUpdateTree(storageHolder, curr, sm);

	if (sm == SM_None)
		return;

	dms_assert(storageHolder);
	if (storageHolder != curr)
		return;

	StorageReadHandle storageHandle(this, storageHolder, curr, StorageAction::updatetree);
	XdbImp imp;
	UpdateColInfo(imp);

	auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
	// Pick up the content of the file
	if (m_SaveColInfo && !imp.OpenForRead(GetNameStr(), sfwa.get(), m_DatExtension, true))
		return;

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
				throwItemErrorF("Column %s is configured with an inconsistent domain unit", colName);
			if (!OverlappingTypes(
					adi->GetDynamicObjClass()->GetValuesType(), 
					ValueClass::FindByValueClassID(imp.ColType(i))))
				throwItemErrorF("Column %s is configured with a unit type that is incompatible with the xdb type %d", colName, imp.ColType(i));
		}
		else if (sm !=SM_None)
		{
			const ValueClass* vc  = ValueClass::FindByValueClassID(imp.ColType(i));
			dms_assert(vc);
			const AbstrUnit * u_col = UnitClass::Find(vc)->CreateDefault();
			// Data item
			adi = CreateDataItem(curr, GetTokenID_mt(colName), u_row, u_col);
		}
	}
}

// Inspect the current tree and creates c.q synchronises the ascii table
void SyncItem(XdbStorageManager* self, XdbImp& imp, bool saveColInfo, const TreeItem* subItem)
{
	DBG_START("XdbStorageManager", "SyncTree", false);

	if (!IsDataItem(subItem))
		return;
	const AbstrDataItem* curdi = AsDataItem(subItem);

	// Units
	const AbstrUnit* du = curdi->GetAbstrDomainUnit();
	const AbstrUnit* vu = curdi->GetAbstrValuesUnit();

	assert(du);
	assert(vu);

	// Get domain range
	long range = du->GetCount();
	DBG_TRACE(("range: %d", range));

	// Get value type
	long col_size = 0;

	ValueClassID vid = vu->GetValueType()->GetValueClassID();
	switch (vid)
	{
		case VT_UInt32: col_size = 12;  break;
		case VT_Int32:  col_size = 12;  break;
		case VT_Float32: col_size = 16; break;
		case VT_Float64: col_size = 25; break;
		default: self->throwItemErrorF("xdb: unsupported value-type %s", vu->GetValueType()->GetName());
	}
	DBG_TRACE(("col_type = %d", vid));
	DBG_TRACE(("col_size = %d", col_size));

	// Write dummy content to disk (if the column doesn't exist yet)
	auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
	imp.AppendColumn(curdi->GetName().c_str(), sfwa.get(), col_size, vid, range, saveColInfo);
}

void XdbStorageManager::DoWriteTree(const TreeItem* storageHolder)
{
	if (!m_SaveColInfo)
		return;

	// inspect the tree and append the corresponding columns
	// if the column exists nothing is changed	

	auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
	XdbImp imp;
	imp.OpenForRead(GetNameStr(), sfwa.get(), m_DatExtension, m_SaveColInfo); // to pass the storage name and lookup column name
	SyncItem(this, imp, m_SaveColInfo, storageHolder);
	for (const TreeItem* subItem = storageHolder->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		SyncItem(this, imp, m_SaveColInfo, subItem);
}

void XdbStorageManager::UpdateColInfo(XdbImp& imp) const
{}

IMPL_DYNC_STORAGECLASS(XdbStorageManager, "xdb");

class XyzStorageManager : public XdbStorageManager
{
public:
	XyzStorageManager() : XdbStorageManager("xyz", false) 
	{}

	void UpdateColInfo(XdbImp& imp) const override
	{
		imp.nrows = -1;
		imp.nrheaderlines = 0;
		imp.headersize = 0;
		imp.m_RecSize = 32;
		imp.m_LineBreakSize = 1;
		imp.ColDescriptions.resize(3);
		imp.ColDescriptions[0].m_Name =  "X"; imp.ColDescriptions[0].m_Offset =  0; imp.ColDescriptions[0].m_Type = VT_Float32;
		imp.ColDescriptions[1].m_Name =  "Y"; imp.ColDescriptions[1].m_Offset = 12; imp.ColDescriptions[1].m_Type = VT_Float32;
		imp.ColDescriptions[2].m_Name =  "Z"; imp.ColDescriptions[2].m_Offset = 24; imp.ColDescriptions[2].m_Type = VT_Float32;
	};

	DECL_RTTI(STGDLL_CALL,StorageClass)
};

IMPL_DYNC_STORAGECLASS(XyzStorageManager, "xyz")
