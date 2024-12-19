// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

// *****************************************************************************
// 
// DMS object to store and retrieve ascii tables
//
// *****************************************************************************

#if !defined(__STG_XDB_STORAGEMANAGER_H)
#define __STG_XDB_STORAGEMANAGER_H


#include "StgBase.h"

class XdbStorageManager : public NonmappableStorageManager
{
public:
	XdbStorageManager(CharPtr datExtension);

//	implement AbstrStorageManager interface
	bool ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

	bool ReadUnitRange(const StorageMetaInfo& smi) const override;

	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

protected:
	// Helper functions
	virtual void UpdateColInfo(class XdbImp& imp) const;

	CharPtr m_DatExtension;
};


#endif // __STG_XDB_STORAGEMANAGER_H
