// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

// storagemanager for 'ODBC storages'

#if !defined(__STG_ODBC_STORAGEMANAGER_H)
#define __STG_ODBC_STORAGEMANAGER_H

// for AbstactStorageManager interface
#include "StgBase.h"
#include "odbc/ODBCImp.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedTreePtr.h"

#include <map>

// *****************************************************************************
//		DMS object to store and retrieve ODBC data
// *****************************************************************************

class ODBCStorageManager : public NonmappableStorageManager
{
	typedef AbstrStorageManager base_type;
	friend class ODBCStorageOutStreamBuff; 
	friend class ODBCStorageInpStreamBuff; 

public:
	ODBCStorageManager();

	//	implement AbstrStorageManager interface
	bool DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const override;
	bool DoCheckWritability() const override;
	
	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

	FileResult ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool ReadUnitRange(const StorageMetaInfo& smi) const override;

	STGDLL_CALL SharedStr GetDatabaseFilename(const TreeItem* storageHolder);

	TDatabase*         DatabaseInstance    (const TreeItem* storageHolder) const;
	TDatabase*         OpenDatabaseInstance(const TreeItem* storageHolder) const;
	TRecordSet*        GetRecordSet(const TreeItem* storageHolder, TreeItem* tableHolder, SharedStr sqlString) const;

	StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction) const override;

private:
	typedef std::map<SharedStr, TIMESTAMP_STRUCT> TTableTimestampCacheType;
	typedef SharedPtr<TRecordSet>               TRecordSetRef;

//	TIMESTAMP_STRUCT AccessTableLastUpdate(const TreeItem* storageHolder, const TreeItem* tableHolder);

	void ResetAccessSysObjectsCopy() // Public soon? Triggered by Invalidate or something?
	{
		m_HasAccessSysObjectsCopy = false;
		m_TableTimestampCache.clear();
	}

	mutable std::unique_ptr<TDatabase>  m_Database;
	mutable std::weak_ptr<const TreeItem> m_TiDatabase; // weak cache-validity token for m_Database's configured storageHolder (compared, never dereferenced)
	mutable std::map<TreeItem*, TRecordSetRef> m_RecordSets;
	bool                                m_HasAccessSysObjectsCopy;
	TTableTimestampCacheType            m_TableTimestampCache;

	DECL_RTTI(STGDLL_CALL, StorageClass)
};

#endif // __STG_ODBC_STORAGEMANAGER_H

/*****************************************************************************/
//										END OF FILE
/*****************************************************************************/
