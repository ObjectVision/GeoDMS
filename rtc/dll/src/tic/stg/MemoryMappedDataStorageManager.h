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

// #1275: the properties that say how an item is shown rather than what it computes or stores.
// A stored property may be read on the meta thread only (StoredPropDef::HasNonDefaultValue), and
// the dictionary is also dumped on whichever thread commits a unit under the store (#1155), so
// MmdStorageManager::DoWriteTree gathers them on the meta thread and hands them to
// TreeItem::XML_Dump in a map of these, keyed by item. The root has no entry: the reader declares
// its holder itself. An empty string is an absent property.
struct Mmd_PresentationTags { SharedStr descr, label, dialogType, dialogData, cdf; };
using Mmd_PresentationTagMap = std::map<const TreeItem*, Mmd_PresentationTags>;

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
	// #1318: and to every item whose current result was not written through into its own file,
	// such as a tile functor (const(), id()) that no DataWriteLock produced; see NoteWriteThrough.
	void MaterializeSharedContent(const TreeItem* storageHolder, const AbstrDataItem* adi);

	// #1318: DataWriteLock maps producedItem's array into the file of configItem in this store
	void NoteWriteThrough(const TreeItem* configItem, const TreeItem* producedItem);

	// #1317: open the write session for an item that produces nothing into it: a rule-only item
	// (#1264) whose result TreeItem::PrepareDataUsage found ready, before its MMD arm could open it
	void OpenForWriteFrom(const TreeItem* storageHolder, const TreeItem* item);

	// #1275: gather the presentation properties of the store's items (see Mmd_PresentationTags)
	// the first time an item under it is prepared, where a stored property may be read.
	void GatherPresentationTagsOnce(const TreeItem* storageHolder);

	// #1298: the lock file 0Lock of the store (see StoreLock in ser/FileMapHandle.h). The first request for
	// writing begins the write session: it creates the lock file, takes the lock that keeps other processes
	// from writing this store, and removes what an interrupted write of another process left. A reader gets
	// a lock without file when the store has none, which is a store written before 20.24.
	auto GetStoreLockForWrite() const -> std::shared_ptr<StoreLock>;
	auto GetStoreLockForRead () const -> std::shared_ptr<StoreLock>;

protected:
//	implement AbstrStorageManager interface
	FileDateTime GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const override;

	bool AllowRandomTileAccess() const override { return true; }

	bool DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const override; // Default implementation now checks existence of m_Name as a file

	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	void DoWriteTree(const TreeItem* storageHolder) override;

	// #1298: taken on producing threads and by readers; m_CriticalSection may be held then, never the other way round
	mutable std::mutex                 m_StoreLockMutex;
	mutable std::shared_ptr<StoreLock> m_StoreLock;

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

	// #1318: per configured item, the cache item whose array DataWriteLock mapped into that item's
	// file. Written on producing threads, so under a mutex of its own, a leaf: nothing is taken while
	// it is held. Weak, and compared with the item's current ultimate item: a recommit produces a
	// fresh cache item, so a record of an earlier production can never pass for the current one.
	mutable std::mutex m_WriteThroughMutex;
	std::map<const TreeItem*, std::weak_ptr<const TreeItem>> m_WrittenThrough;

	// #1275: the presentation properties of the store's items, gathered by DoWriteTree on the meta
	// thread at the first emission of the dictionary and read by every later one; both under
	// m_CriticalSection (OpenForWrite asserts it held, UpdateDictionary takes it).
	Mmd_PresentationTagMap m_PresentationTags;
	bool m_PresentationTagsGathered = false;

	DECL_RTTI(, StorageClass)
};

extern thread_local const Mmd_PresentationTagMap* t_MmdPresentationTags; // #1275, see Mmd_PresentationTags

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
