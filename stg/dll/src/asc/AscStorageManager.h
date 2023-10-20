// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

// *****************************************************************************
// 
// DMS object to store and retrieve 'Ascii-grids'
//
// *****************************************************************************

#if !defined(__STG_ASC_STORAGEMANAGER_H)
#define __STG_ASC_STORAGEMANAGER_H

#include "StgBase.h"
#include "GridStorageManager.h"

// storagemanager for 'Ascii-grids'
class AsciiStorageManager : public AbstrGridStorageManager
{
public:
	STGDLL_CALL AsciiStorageManager();

//	implement AbstrStorageManager interface
	bool ReadDataItem (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smi) override;

	bool ReadUnitRange(const StorageMetaInfo& smi) const override;
	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

	STGDLL_CALL bool AllowRandomTileAccess() const override { return false; }

	DECL_RTTI(STGDLL_CALL, StorageClass)

private:
	void ReadGridCounts(const StgViewPortInfo& vpi, AbstrDataObject* borrowedReadResultHolder, tile_id t);
	bool ReadGridData  (const StgViewPortInfo& vpi, AbstrDataItem* adi, AbstrDataObject* borrowedReadResultHolder, tile_id t);
};

#endif // _AsciiStorageManager_H_
