// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

/*****************************************************************************/
// DMS object to store and retrieve ascii tables
/*****************************************************************************/

#if !defined(__STG_DBF_STORAGEMANAGER_H)
#define __STG_DBF_STORAGEMANAGER_H

//#include "StorageUtils.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedPtr.h"
#include "ptr/SharedTreePtr.h"

/*****************************************************************************/
//									CLASSES
/*****************************************************************************/

// hidden implementation
class DbfImpl;
struct TNameSet;

// storagemanager for 'Dbf-grids'
class DbfStorageManager : public NonmappableStorageManager
{
	StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction) const override;
	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	void DoWriteTree  (const TreeItem* storageHolder) override;

	FileResult ReadDataItem (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	FileResult WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

	bool ReadUnitRange(const StorageMetaInfo& smi) const override;
	bool WriteUnitRange(StorageMetaInfoPtr&& smi) override;

private:
	TNameSet*     BuildNameSet(const TreeItem* storageHolder) const;
	void          TestDomain(const AbstrDataItem* adi) const;

//	hidden implementation which doesn't know about DMS structure
	mutable SharedPtr<TNameSet>        m_NameSet;          // nameset cache
	mutable std::weak_ptr<const TreeItem> m_NameSetStorageHolder; // weak cache-validity token: which storageHolder the nameset cache was made for. Weak so a destroyed-then-reallocated holder at the same address cannot alias into a false cache-hit (an expired weak compares unequal -> rebuild).
	mutable std::shared_ptr<const AbstrUnit> m_TableDomain;

	friend struct DbfMetaInfo;
	DECL_RTTI(,StorageClass)
};

#endif // __STG_DBF_STORAGEMANAGER_H
