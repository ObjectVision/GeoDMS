// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once
// *****************************************************************************
// 
// DMS object to store and retrieve Windows bmp files (false color)
//
// *****************************************************************************

#if !defined(__STG_BMP_STORAGEMANAGER_H)
#define __STG_BMP_STORAGEMANAGER_H

#include "StgBase.h"
#include "GridStorageManager.h"

#include "vt/color.h"

struct BmpPalStorageManager : AbstrGridStorageManager
{
//	implement AbstrStorageManager interface
	FileResult ReadDataItem (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	FileResult WriteDataItem(StorageMetaInfoPtr&& smi) override;

	bool ReadUnitRange(const StorageMetaInfo& smi) const override;

	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

	bool CanWriteTiles() const override { return true; }

protected:
	virtual bool HasGridData() = 0;
};


#endif // __STG_BMP_STORAGEMANAGER_H
