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
struct SafeFileWriterArray;

/*
 *	MmdStorageManager
 *
 */

class MmdStorageManager : public AbstrStorageManager
{
public:
	using base_type = AbstrStorageManager;

	TIC_CALL SharedStr GetFullFileName(CharPtr name) const;

	auto GetSFWA() const-> std::shared_ptr<SafeFileWriterArray>;

protected:
//	implement AbstrStorageManager interface
//	void DropStream(const TreeItem* item, CharPtr path) override;
	FileDateTime GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const override;

	bool AllowRandomTileAccess() const override { return true; }
	bool EasyRereadTiles() const override { return true; }
	TIC_CALL virtual bool CanWriteTiles() const { return true; }

	bool DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const override; // Default implementation now checks existence of m_Name as a file

	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	void DoWriteTree(const TreeItem* storageHolder) override;

	mutable FileHandle m_MmdLockFile;
	mutable std::shared_ptr<SafeFileWriterArray> m_SFWA;

	friend class FileSystemStorageOutStreamBuff;
	friend class FileSystemStorageInpStreamBuff;

	DECL_RTTI(TIC_CALL, StorageClass)
};

using AppendTreeFromConfigurationFuncPtr = auto (*) (const char* fileName, TreeItem* treeItem)->TreeItem*;
extern TIC_CALL AppendTreeFromConfigurationFuncPtr s_AppendTreeFromConfigurationPtr;


#endif // !defined(__STG_MMD_STORAGEMANAGER_H)
