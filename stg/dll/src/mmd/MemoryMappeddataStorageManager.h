// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

#if !defined(__STG_MMD_STORAGEMANAGER_H)
#define __STG_MMD_STORAGEMANAGER_H


#include "StgBase.h"
#include "ptr/OlePtr.h"
#include "ser/SafeFileWriter.h"
#include "ser/FileMapHandle.h"

#include "stg/AsmUtil.h"
#include "stg/AbstrStreamManager.h"
/*
 *	MmdStorageManager
 *
 */

class MmdStorageManager : public AbstrStreamManager
{
	friend class CompoundStorageOutStreamBuff; // can only be accessed by this class

public:

	STGDLL_CALL ~MmdStorageManager();

	STGDLL_CALL SharedStr GetFullFileName(CharPtr name) const;

protected:
//	implement AbstrStorageManager interface
	void DropStream(const TreeItem* item, CharPtr path) override;
	FileDateTime GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const override;

	std::unique_ptr<OutStreamBuff> DoOpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t) override;
	std::unique_ptr<InpStreamBuff> DoOpenInpStream(const StorageMetaInfo& smi, CharPtr path) const override;

	void DoOpenStorage  (const StorageMetaInfo& smi, dms_rw_mode rwMode) const override;
	void DoCloseStorage (bool mustCommit) const override;

	mutable FileHandle m_FssLockFile;

	friend class FileSystemStorageOutStreamBuff;
	friend class FileSystemStorageInpStreamBuff;

	DECL_RTTI(STGDLL_CALL, StorageClass)
};

#endif // !defined(__STG_MMD_STORAGEMANAGER_H)
