// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__STG_TIF_STORAGEMANAGER_H)
#define __STG_TIF_STORAGEMANAGER_H

#include "StgBase.h"
#include "GridStorageManager.h"
#include "vt/color.h"
#include "ptr/OwningPtr.h"

// hidden implementation
class TifImp;

// *****************************************************************************

// storagemanager for 'Tif-grids'
struct TiffSM : AbstrGridStorageManager
{
	TiffSM();
	~TiffSM();

    // Implement AbstrStorageManager interface
	void DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const override;
	void DoCloseStorage(bool mustCommit) const override;
	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	bool ReadUnitRange(const StorageMetaInfo& smi) const override;

	FileResult ReadDataItem (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	FileResult WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

	bool CanWriteTiles() const override { return true; }

	UInt32 GetNativeTileSizeX() const override;
	UInt32 GetNativeTileSizeY() const override;

private:
	void ReadGridCounts(const StgViewPortInfo& vpi, AbstrDataItem* adi, AbstrDataObject* borrowedReadResultHolder, tile_id t, StorageMetaInfoPtr smi);
	void ReadGridData  (const StgViewPortInfo& vpi, AbstrDataItem* adi, AbstrDataObject* borrowedReadResultHolder, tile_id t, StorageMetaInfoPtr smi);
	bool ReadPalette   (AbstrDataObject* ado);

	void WriteGridData(TifImp& imp, const StgViewPortInfo& vpi, const TreeItem* storageHolder, const AbstrDataItem* adi, const ValueClass* streamType);
	void WritePalette (TifImp& imp, const TreeItem* storageHolder, const AbstrDataItem* adi);

	void EnsureBlockSizeCached() const;

private:
	mutable std::unique_ptr<TifImp> m_pImp;
	mutable UInt32 m_CachedBlockSizeX = 0;
	mutable UInt32 m_CachedBlockSizeY = 0;

	DECL_RTTI(, StorageClass)
};


#endif // __STG_TIF_STORAGEMANAGER_H
