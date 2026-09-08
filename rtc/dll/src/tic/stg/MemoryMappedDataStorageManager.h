// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

#if !defined(__STG_MMD_STORAGEMANAGER_H)
#define __STG_MMD_STORAGEMANAGER_H


#include <map>
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

	// #587: an attribute of the store is read by mapping its file, as storage_read_attr / storage_read_value
	bool SupportsReadOperator() const override { return true; }
	ReadCallSpec DescribeReadCall(const TreeItem* storageHolder, const TreeItem* item) const override;

	// #1155: re-emit the dictionary once a var-range unit's range has become available;
	// the dictionary written at OpenForWrite time lacks the Range of units not calculated yet
	void UpdateDictionary(const TreeItem* storageHolder);

	// #1247: give a stored item that shares its content with another item a file of its own.
	//
	// A stored reference does not keep its config item as referred item: TreeItem::SetReferredItem
	// swaps in a DataController over 'convert(<source key>, <values unit key>)' so the produced
	// array can be mapped into the store, and GetOrCreateDataController interns those by key
	// expression. Two stored items over one source therefore share ONE cache item, whose single
	// back reference is what DataWriteLock names the file from, so only the first claimant got a
	// file while the dictionary declared both, and the store promised data it did not have.
	// Everything a store declares must be in it, so the other claimants get a copy here.
	// Called from TreeItem::CommitDataChanges once the data is ready, before UpdateDictionary.
	void MaterializeSharedContent(const TreeItem* storageHolder, const AbstrDataItem* adi);

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

	// #1247: the items this manager has already copied, with the change stamp they were copied at.
	// NOT a bare presence set: within one session an item can be invalidated and recommitted -- a
	// source change, a GUI edit, #1155's re-emission after a later attribute moves a unit's range --
	// and the file then has to be rewritten. Skipping that would leave a stale file declared as
	// current, which is the very defect this fixes. The stamp is the CONFIG item's
	// Actor::GetLastChangeTS, which covers the item and its suppliers: a recommit produces a FRESH
	// cache item, so a cache-keyed guard could only ever say "unknown" and would never skip at all.
	mutable std::map<const TreeItem*, TimeStamp> m_MaterializedAt;

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
