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

/*****************************************************************************/
// Implementations of - DbfStorageOutStreamBuff
//                    - DbfStorageManager
/*****************************************************************************/

#include "DbfStorageManager.h"
#include "dbf/dbfImpl.h"

#include "dbg/debug.h"
#include "mci/ValueClassID.h"  
#include "ser/BaseStreamBuff.h"  
#include "utl/mySPrintF.h"

#include "AbstrDataItem.h"						
#include "DataItemClass.h"
#include "DataStoreManagerCaller.h"
#include "Param.h"  // 'Hidden DMS-interface functions'

#include "TreeItemContextHandle.h"
#include "UnitClass.h"

#include "stg/StorageClass.h"

#include "NameSet.h"

/*****************************************************************************/
//										DbfStorageManager
/*****************************************************************************/

struct DbfImplRead : DbfImpl
{
	DbfImplRead(WeakStr filename, const TreeItem* storageHolder)
	{
		auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
		if (! OpenForRead(filename, sfwa.get()) )
			throwErrorF("DBF", "Cannot open %s for read (%d: %s)", filename.c_str(), errno, strerror(errno) );
	}
};

bool DbfStorageManager::ReduceResources()
{
	m_NameSet.reset();
	return true;
} // CloseDbf


TNameSet* DbfStorageManager::BuildNameSet(const TreeItem* storageHolder)  const
{
	if (m_NameSet.is_null() || storageHolder != m_NameSetStorageHolder)
	{
		SharedPtr<const AbstrUnit> tableDomain = StorageHolder_GetTableDomain(storageHolder);

		OwningPtr<TNameSet> nameset = new TNameSet(DBF_COLNAME_SIZE);

		nameset->InsertIfColumn(storageHolder, tableDomain);
		for (const TreeItem* subItem = storageHolder->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			nameset->InsertIfColumn(subItem, tableDomain);

		m_TableDomain          = tableDomain;
		m_NameSet              = nameset.release();
		m_NameSetStorageHolder = storageHolder;
	}
	dms_assert(m_NameSet); // POSTCONDITION
	dms_assert(m_TableDomain);
	return m_NameSet;
}

bool DbfStorageManager::ReadUnitRange(const StorageMetaInfo& smi) const
{
	DbfImplRead dbf(GetNameStr(), smi.StorageHolder());
	smi.CurrWU()->SetCount(dbf.RecordCount());
	return true;
}

bool DbfStorageManager::WriteUnitRange(StorageMetaInfoPtr&& smi)
{
	return true;
}

void DbfStorageManager::TestDomain(const AbstrDataItem* adi) const
{
	dms_assert(adi);
	if (!TableDomain_IsAttr(m_TableDomain, adi))
		adi->throwItemErrorF(
			"DataItem %s has a domain that is incompatible with the table domain %s", 
			adi->GetName().c_str(), 
			m_TableDomain->GetFullName().c_str()
		);
}

struct DbfMetaInfo : StorageMetaInfo
{
	DbfMetaInfo(const DbfStorageManager* dbf, const TreeItem* storageHolder, TreeItem* adi)
		: StorageMetaInfo(storageHolder, adi)
		, m_NameSet(dbf->BuildNameSet(storageHolder))
	{}
	SharedPtr<TNameSet> m_NameSet;
};

StorageMetaInfoPtr DbfStorageManager::GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction) const
{
	return std::make_unique<DbfMetaInfo>(this, storageHolder, adi);
}

void DbfStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	AbstrStorageManager::DoUpdateTree(storageHolder, curr, sm);

	if (sm == SM_None)
		return;
	dms_assert(storageHolder);
	if (storageHolder != curr)
		return;
	if (!DoesExist(storageHolder))
		return;

	SharedPtr<TNameSet> nameset = BuildNameSet(storageHolder);
	dms_assert(nameset);

	StorageReadHandle storageHandle(this, storageHolder, curr, StorageAction::updatetree);
	dms_assert( IsOpen() );

	DbfImplRead dbf(GetNameStr(), storageHolder);

	Int32 colcnt = dbf.ColumnCount();

	for (Int32 i = 0; i < colcnt; i++)
	{
		CharPtr mappedName = dbf.ColumnIndexToName(i);
		CharPtr name = nameset->GetItemName(mappedName);
		
		if (name)
		{
			TreeItem* ti = curr->GetSubTreeItemByID(GetTokenID_mt(name));
			if (!ti) ti = curr;
			dms_assert(ti); // is in nameset, so must be there
			AbstrDataItem* adi = checked_cast<AbstrDataItem*>(ti);
			dms_assert(adi);
			if (ti->IsDisabledStorage())
				ti->throwItemErrorF(
					"DisableStorage not expected because of Dbf Column %s", 
					mappedName
				);
			
			TestDomain(adi);

			const ValueClass* vc = adi->GetDynamicObjClass()->GetValuesType();
			if (! 
					(	vc->IsNumeric()                    && ValueClass::FindByValueClassID(dbf.ColumnType(i))->IsNumeric()
					||	vc->GetValueClassID() == VT_SharedStr && dbf.ColumnType(i) == VT_SharedStr
					||	vc->GetValueClassID() == VT_Bool   && dbf.ColumnType(i) == VT_Bool
					)
				)
				ti->throwItemErrorF(
					"Type of ValuesUnit is incompatible with the Dbf column compatible type %s"
				,	ValueClass::FindByValueClassID(dbf.ColumnType(i))->GetName()
				);
		} else
		{
			CDebugContextHandle addItemDebugContext("adding dataitem:", mappedName, false);
			TreeItem* ti = curr->GetSubTreeItemByID(GetTokenID_mt(mappedName));
			if (ti)
				ti->throwItemErrorF(
					"A valid column expected because of Dbf Column %s"
				,	mappedName
				);
			
			SharedStr itemName = nameset->InsertFieldName(mappedName);

			CreateDataItem(
				curr
			,	GetTokenID_mt(itemName.c_str())
			,	m_TableDomain
			,	UnitClass::Find(ValueClass::FindByValueClassID(dbf.ColumnType(i)))->CreateDefault()
			);
			;
		}
	}
}

void DbfStorageManager::DoWriteTree(const TreeItem* storageHolder)	
{
	dms_assert(m_IsOpenedForWrite);
	dms_assert(storageHolder);

	BuildNameSet(storageHolder);
}

// REMOVE return dbf.ReadData(&(debug_cast<DataArray<type>*>(ado)->GetDataWrite()), fieldName.begin(), vcID);

bool DbfStorageManager::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	TreeItemContextHandle och2(smi->StorageHolder(), "StorageParent");
	dms_assert(!t);

	DbfImplRead dbf(GetNameStr(), smi->StorageHolder());

	SharedPtr<TNameSet> nameset = debug_cast<const DbfMetaInfo*>(smi.get())->m_NameSet;
	TestDomain(smi->CurrRD());

	AbstrDataObject* ado = borrowedReadResultHolder;

	smi->CurrWD()->GetAbstrDomainUnit()->ValidateCount(dbf.RecordCount());

	const ValueClass* vc = ado->GetValuesType();
	ValueClassID	vcID = vc->GetValueClassID();		

	dms_assert(nameset);
	SharedStr fieldName = nameset->ItemNameToFieldName(smi->CurrRI()->GetName().c_str());

	switch (vcID)
	{
#define INSTANTIATE(T) case VT_##T: return DbfImplStub<T>(&dbf, debug_cast<DataArray<T>*>(borrowedReadResultHolder)->GetDataWrite(), fieldName.begin(), vcID).m_Result;
		INSTANTIATE_NUM_ORG
		INSTANTIATE_OTHER
#undef INSTANTIATE

		default:
			smi->CurrRI()->throwItemErrorF(
				"DbfStorageManager::ReadDataItem not implemented for DataItem with ValuesUnitType: %s"
			,	vc->GetName().c_str()
			);
	} // switch

	return false;
} // ReadData


bool DbfStorageManager::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	DBG_START("DbfStorageManager", "WriteDataItem", false);

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return false;

	auto smi = smiHolder.get();
	StorageWriteHandle storageHandle(std::move(smiHolder));

	const TreeItem* storageHolder = smi->StorageHolder();
	const AbstrDataItem*   adi     = smi->CurrRD();
	const AbstrDataObject* ado     = adi->GetRefObj();
	const ValueClass*      vc      = ado->GetValuesType();
	ValueClassID           vcID    = vc->GetValueClassID();		
	SharedPtr<TNameSet>    nameset = debug_cast<DbfMetaInfo*>(smi)->m_NameSet;
	dms_assert(nameset);
	SharedStr fieldName = nameset->ItemNameToFieldName(adi->GetName().c_str());
	CharPtr fieldNameStr = fieldName.c_str();

	DbfImpl dbf;

	switch (vcID)
	{
#define INSTANTIATE(T) case VT_##T: return DbfImplStub<T>(&dbf, GetNameStr(), sfwa.get(), debug_cast<const DataArray<T>*>(ado)->GetDataRead(), fieldNameStr, vcID).m_Result;
		INSTANTIATE_NUM_ORG
		INSTANTIATE_OTHER
#undef INSTANTIATE
		default:
			throwItemErrorF("WriteDataItem not implemented for dbf data with ValuesUnitType: %s", vc->GetName());
	}
	return false;
}

IMPL_DYNC_STORAGECLASS(DbfStorageManager, "dbf")

/*****************************************************************************/
//										END OF FILE
/*****************************************************************************/

