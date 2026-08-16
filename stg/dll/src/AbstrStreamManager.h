// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  AbstrStreamManager: intermediate base for storage managers that read and
 *  write their data through sequential streams (one InpStreamBuff/OutStreamBuff
 *  per data item), such as the compound (cfs) and file-system (fss) storage
 *  managers. Derived classes provide DoOpenInpStream/DoOpenOutStream; this base
 *  implements the ReadDataItem/WriteDataItem and unit-range plumbing on top of
 *  them. Moved here from rtc/tic/stg (2026-08): stg was its only consumer.
 */

#if !defined(__STG_ABSTRSTREAMMANAGER_H)
#define __STG_ABSTRSTREAMMANAGER_H

#include "stg/AbstrStorageManager.h"

class InpStreamBuff;
class OutStreamBuff;

class AbstrStreamManager : public NonmappableStorageManager
{
public:
	AbstrStreamManager();

	std::unique_ptr<OutStreamBuff> OpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t);
	std::unique_ptr<InpStreamBuff> OpenInpStream(const StorageMetaInfo& smi, CharPtr path) const;

	FileResult ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	FileResult WriteDataItem(StorageMetaInfoPtr&& smi) override;

protected:
	virtual std::unique_ptr<OutStreamBuff> DoOpenOutStream(const StorageMetaInfo& smi, CharPtr, tile_id t) = 0;
	virtual std::unique_ptr<InpStreamBuff> DoOpenInpStream(const StorageMetaInfo& smi, CharPtr) const = 0;
	bool ReadUnitRange(const StorageMetaInfo& smi) const override;
	bool WriteUnitRange(StorageMetaInfoPtr&& smi) override;
	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
};

#endif // __STG_ABSTRSTREAMMANAGER_H
