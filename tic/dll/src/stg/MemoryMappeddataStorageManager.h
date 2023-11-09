// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

#if !defined(__STG_MMD_STORAGEMANAGER_H)
#define __STG_MMD_STORAGEMANAGER_H


#include "ptr/OlePtr.h"
#include "ser/SafeFileWriter.h"
#include "ser/FileMapHandle.h"

#include "mci/Object.h"
#include "stg/AsmUtil.h"
#include "stg/AbstrStorageManager.h"
struct StorageClass;

/*
 *	MmdStorageManager
 *
 */

class MmdStorageManager : public AbstrStorageManager
{
public:
	using base_type = AbstrStorageManager;

//	TIC_CALL MmdStorageManager();
//	TIC_CALL ~MmdStorageManager();

	TIC_CALL SharedStr GetFullFileName(CharPtr name) const;

protected:
//	implement AbstrStorageManager interface
//	void DropStream(const TreeItem* item, CharPtr path) override;
	FileDateTime GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const override;

	bool AllowRandomTileAccess() const override { return true; }
	bool EasyRereadTiles() const override { return true; }
	TIC_CALL virtual bool CanWriteTiles() const { return true; }


//	std::unique_ptr<OutStreamBuff> DoOpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t) override;
//	std::unique_ptr<InpStreamBuff> DoOpenInpStream(const StorageMetaInfo& smi, CharPtr path) const override;

//	void DoOpenStorage  (const StorageMetaInfo& smi, dms_rw_mode rwMode) const override;
//	void DoCloseStorage (bool mustCommit) const override;

	mutable FileHandle m_MmdLockFile;

	friend class FileSystemStorageOutStreamBuff;
	friend class FileSystemStorageInpStreamBuff;

	DECL_RTTI(TIC_CALL, StorageClass)
};

#endif // !defined(__STG_MMD_STORAGEMANAGER_H)
