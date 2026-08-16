// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

#if !defined(__STG_MMD_STORAGEMANAGER_H)
#define __STG_MMD_STORAGEMANAGER_H


#include <set>

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

	SharedStr GetFullFileName(CharPtr name) const;

	// #1155: re-emit the dictionary once a var-range unit's range has become available;
	// the dictionary written at OpenForWrite time lacks the Range of units not calculated yet
	void UpdateDictionary(const TreeItem* storageHolder);

protected:
//	implement AbstrStorageManager interface
//	void DropStream(const TreeItem* item, CharPtr path) override;
	FileDateTime GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const override;

	bool AllowRandomTileAccess() const override { return true; }
	bool EasyRereadTiles() const override { return true; }
	virtual bool CanWriteTiles() const { return true; }

	bool DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const override; // Default implementation now checks existence of m_Name as a file

	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	void DoWriteTree(const TreeItem* storageHolder) override;

	mutable FileHandle m_MmdLockFile;

	// #1154/#1179: read holders whose dictionary has been merged. Consulted only to keep
	// DoUpdateTree idempotent -- after the merge the holder HAS sub-items, which must not trip
	// the reader-declared-sub-items refusal on a revisit. Pointers are keys, never dereferenced.
	mutable std::set<const TreeItem*> m_MergedReadHolders;

	DECL_RTTI(, StorageClass)
};

using AppendTreeFromConfigurationFuncPtr = auto (*) (const char* fileName, TreeItem* treeItem)->TreeItem*;
extern TIC_CALL AppendTreeFromConfigurationFuncPtr s_AppendTreeFromConfigurationPtr;

// #1154: while DoWriteTree dumps a dictionary, the root being dumped. TreeItem::XML_Dump emits
// the synthesized restrictions of that root as an IntegrityCheck subtag, which the read holder
// merges and, through #1180, applies to every sub-item read through it.
// No TIC_CALL: thread_local data cannot carry a dll interface (C2492); definition and consumer
// both live in DmRtc.
extern thread_local const TreeItem* t_MmdDictionaryRoot;
auto Mmd_SynthesizeExternalUnitRestrictions(const TreeItem* dictRoot) -> SharedStr;


#endif // !defined(__STG_MMD_STORAGEMANAGER_H)
