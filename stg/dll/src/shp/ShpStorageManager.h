// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

// *****************************************************************************
// 
// DMS object to store and retrieve 'Shp-grids'
//
// *****************************************************************************

#if !defined(__STG_SHP_STORAGEMANAGER_H)
#define __STG_SHP_STORAGEMANAGER_H

// for AbstactStorageManager interface
#include "StgBase.h"

// storagemanager for 'Shp-grids'
class ShpStorageManager : public NonmappableStorageManager
{

public:
//	implement AbstrStorageManager interface
	bool ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

	bool ReadUnitRange(const StorageMetaInfo& smi) const override;

	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

	DECL_RTTI(STGDLL_CALL, StorageClass)
};


#endif // __STG_SHP_STORAGEMANAGER_H
