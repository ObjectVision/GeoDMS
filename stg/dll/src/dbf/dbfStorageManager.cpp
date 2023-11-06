// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
	NonmappableStorageManager::DoUpdateTree(storageHolder, curr, sm);

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
					||	vc->GetValueClassID() == ValueClassID::VT_SharedStr && dbf.ColumnType(i) == ValueClassID::VT_SharedStr
					||	vc->GetValueClassID() == ValueClassID::VT_Bool   && dbf.ColumnType(i) == ValueClassID::VT_Bool
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
#define INSTANTIATE(T) case ValueClassID::VT_##T: return DbfImplStub<T>(&dbf, debug_cast<DataArray<T>*>(borrowedReadResultHolder)->GetDataWrite(), fieldName.begin(), vcID).m_Result;
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
#define INSTANTIATE(T) case ValueClassID::VT_##T: return DbfImplStub<T>(&dbf, GetNameStr(), sfwa.get(), debug_cast<const DataArray<T>*>(ado)->GetDataRead(), fieldNameStr, vcID).m_Result;
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

