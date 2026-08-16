// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

// AStorageManager.h: interface for the AStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__ABSTRSTORAGEMANAGER_H)
#define __ABSTRSTORAGEMANAGER_H

/*
 *	AbstrStorageManager
 *		NonmapppableStoragemanager
 *			AbstrStreamManager
 *			AbstrGridStorageManager
 *		MemoryMappableStorageManager
 *
 *	These are contracts for a storage definition based on a file	mechanism
 */

/* Popular use case scenario's

2. Creation from a configuration file (read-only / read-write)
3. Creation from user intervention
4a. storage to a persistent storage
4b. retrieval from a persistent storage
meta data synchronisation

The question is whether the storage exists; if it does not, creation must be 
suspended until the first write action, all sub-items must set isStored to false 
to prevent premature read actions.
If the storage does exist; synchronisation of the storage meta data must be done 
with the existing tree item info. New tree items could be found. 
This must be done before any expressions are evaluated.
The isStored and lastUpdate property can  be retrieved from the previous session (4a)
unless 2. or 3. are taken in which case isStored is assumed to be true only 
if a tree item has no expression.

5a. Read Meta data (as in 1a) to new tree
5b  Read Meta data into existing tree
5c. Write Meta data on commitment (as in 1b)
5d. Write Meta data on creation / update

6. OpenStreamForRead/Write
7. Retrieve directory info

Storage State:
(storageName, storageType, isOpen, isReadWrite)

Stream state, represented by accompaning tree item
- type, compression type, etc.
- is stored, last update date


Issues
1. Opening and connecting storages to multiple tree items
2. DoWriteTree requires meta info on all sub-item of a storage manager that might not be ready
*/

#include <map>
#include <semaphore>

#include "TicBase.h"
#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "FileResult.h"

#include "act/ActorEnums.h"
struct ActorVisitor;		
#include "ptr/SharedStr.h"

class NonmappableStorageManager;
enum class SyncMode { AllTables, AttrsOfConfiguredTables, None };

struct StorageCloseHandle;
struct StorageReadHandle;

// *****************************************************************************
// 
// polymorphic StorageMetaInfo, provided prior to read/write tasks commence
//
// *****************************************************************************

template <typename Base>
struct enable_shared_from_this_base : std::enable_shared_from_this<Base>
{
	template <typename Derived>
	std::shared_ptr<Derived> shared_from_base()
	{
		return std::static_pointer_cast<Derived>(this->shared_from_this());
	}
	template <typename Derived>
	std::shared_ptr<const Derived> shared_from_base() const
	{
		return std::static_pointer_cast<const Derived>(this->shared_from_this());
	}

//	using weak_from_this;

	template <typename Derived>
	std::weak_ptr<Derived> weak_from_base()
	{
		auto sp = this->weak_from_this().lock();  // get shared_ptr<Base>
		if (!sp)
			return std::weak_ptr<Derived>(); // return empty if not initialized

		return std::static_pointer_cast<Derived>(sp); // cast and return as weak_ptr	}
	}

	template <typename Derived>
	std::weak_ptr<const Derived> weak_from_base() const
	{
		auto sp = this->weak_from_this().lock();  // get shared_ptr<Base>
		if (!sp)
			return std::weak_ptr<Derived>(); // return empty if not initialized

		return std::static_pointer_cast<const Derived>(sp); // cast and return as weak_ptr	}
	}

};

enum class StorageAction { read, write, updatetree, writetree };

// *****************************************************************************
// 
// movable StorageMetaInfo
//
// *****************************************************************************

struct StorageMetaInfo : std::enable_shared_from_this<StorageMetaInfo>
{
	StorageMetaInfo(NonmappableStorageManager* storageManager)
		: m_StorageManager(storageManager, existing_obj{})
	{
	}

	StorageMetaInfo(const TreeItem* storageHolder, const TreeItem* curr)
		: m_StorageManager(storageHolder->GetStorageManager(), existing_obj{})
		, m_StorageHolder(make_shared_tree(storageHolder, existing_obj{}))
		, m_Curr(make_shared_tree(curr, existing_obj{}))
		, m_RelativeName(storageHolder->DoesContain(curr) ? curr->GetRelativeName(storageHolder) : curr->GetFullName())
	{
	}
	TIC_CALL virtual ~StorageMetaInfo();
	TIC_CALL virtual void PrepareReadDataOrSuspend(); // #933: resolve supplier prerequisites for the read (may suspend); formerly OnPreLock
	TIC_CALL virtual void OnOpenForRead(StorageReadHandle*);
	TIC_CALL virtual void OnClose(StorageCloseHandle*);

	TIC_CALL auto CurrRI() const -> std::shared_ptr<const TreeItem> { return m_Curr; }
	TIC_CALL auto CurrRD() const -> std::shared_ptr<const AbstrDataItem>;
	TIC_CALL auto CurrRU() const -> std::shared_ptr<const AbstrUnit>;
	AbstrDataItem* CurrWD() const { return const_cast<AbstrDataItem*>(CurrRD().get()); }
	AbstrUnit*     CurrWU() const { return const_cast<AbstrUnit*>(CurrRU().get()); }
	TreeItem*      CurrWI() const { return const_cast<TreeItem*>(CurrRI().get()); }

	AbstrStorageManager* StorageManager() const { return m_StorageManager.get(); }
	const TreeItem* StorageHolder() const { return m_StorageHolder.get(); }

protected:
	SharedPtr<AbstrStorageManager> m_StorageManager;
	std::shared_ptr<const TreeItem> m_StorageHolder, m_Curr;
public:
	SharedStr m_RelativeName;
	bool      m_MustRememberFailure :1 = true;
	bool      mf_WarningFlag1       :1 = false;
	std::mutex m_TileReadSection;
	std::once_flag m_compare_tile_size_flag;
};

struct GdalMetaInfo :StorageMetaInfo
{
	TIC_CALL GdalMetaInfo(const TreeItem* storageHolder, const TreeItem* curr);

	SharedTreeItemInterestPtr m_OptionsItem;
	SharedTreeItemInterestPtr m_LayerCreationOptions;
	SharedTreeItemInterestPtr m_ConfigurationOptions;
	SharedTreeItemInterestPtr m_DriverItem;
	SharedStr m_Driver, m_Options;
};

// *****************************************************************************
// 
// AbstrStorageManager
//
// *****************************************************************************

// #933: tag for constructing a lock_t / StorageCloseHandle that ADOPTS an
// already-held m_CriticalSection (acquired earlier at the scheduling gate in
// OperationContext::getUniqueLicenseToRun) instead of acquiring it again.
struct adopt_storage_lock_t { explicit adopt_storage_lock_t() = default; };
inline constexpr adopt_storage_lock_t adopt_storage_lock{};

// Reduce a native on-disk file-block dimension to an internal grid-tile dimension that fits the
// UInt16 blockSize params of SetRangeAsIPoint (so a tile/strip dim > 65535 cannot wrap to a bogus
// small value when narrowed). Rather than hard-clamping (which would misalign internal tiles with
// file blocks), split the native block into the FEWEST equal-ish sub-tiles, keeping tile boundaries
// aligned with file blocks (exactly so when nativeBlockSize is a multiple of the part count, e.g.
// powers of two). The result is always <= maxTileDim.
inline UInt32 GridBlockSubdivide(UInt32 imageSize, UInt32 nativeBlockSize, UInt32 maxTileDim)
{
	if (nativeBlockSize <= maxTileDim)
		return nativeBlockSize;
	UInt32 minParts = (nativeBlockSize + maxTileDim - 1) / maxTileDim; // fewest parts s.t. nativeBlockSize/parts <= maxTileDim
	// Tile/block boundaries only repeat across the image when it spans MORE THAN ONE block; only then
	// can a non-dividing split cause partial tile/block overlap. In that case prefer a part count that
	// divides nativeBlockSize evenly so memory tiles stay aligned with file blocks. Bounded search keeps
	// the tile >= ~maxTileDim/2; fall back to an equal-ish ceil-divide (e.g. single block, or prime size).
	if (imageSize > nativeBlockSize)
		for (UInt32 parts = minParts; parts < minParts * 2 && parts <= nativeBlockSize; ++parts)
			if (nativeBlockSize % parts == 0)
				return nativeBlockSize / parts; // exact: parts equal tiles tile the file block, aligned
	return 256; // default tile size for non-divisible blocks, or small images that don't span multiple blocks
}

class AbstrStorageManager : public SharedObj
{
	using base_type = SharedObj;

public:
	//	Static interface functions
	static AbstrStorageManagerRef Construct(CharPtr fullStorageName, TokenID typeID, StorageReadOnlySetting readOnly, bool throwOnFailure);
	static AbstrStorageManagerRef Construct(const TreeItem* holder, SharedStr relStorageName, TokenID typeID, StorageReadOnlySetting readOnly, bool throwOnFailure = true);
	static bool                 DoesExistEx(CharPtr name, TokenID typeID, const TreeItem* storageHolder); // XXX TODO, REPLACE CharPtr by SharedCharArray*
	TIC_CALL static SharedStr            Expand(const TreeItem* configStore, SharedStr storageName);
	TIC_CALL static SharedStr            Expand(CharPtr configDir, CharPtr storageName);
	TIC_CALL static SharedStr            GetFullStorageName(const TreeItem* configStore, SharedStr storageName); // ForItem
	TIC_CALL static SharedStr            GetFullStorageName(CharPtr subDir, CharPtr storageNameCStr); // ForFolder
	static SyncMode             GetSyncMode(const TreeItem* storageHolder);

	//	override / extent PerssistentRefObject interface
	TIC_CALL SharedStr GetNameStr() const;

protected:
	// construction only from StorageMangers friends	
	AbstrStorageManager();
	virtual ~AbstrStorageManager();

public:
	void InitStorageManager(CharPtr storageName, bool readOnly);
	void DoNotCommitOnClose() { m_Commit = false; }
	TIC_CALL bool DoesExist(const TreeItem* storageHolder)  const; // returns IsOpen() || DoCheckExistence()
	bool IsWritable() const;
	bool IsOpen() const { return m_IsOpen; }
	bool IsReadOnly() const { return m_IsReadOnly; }
	bool IsOpenForWrite() const { return IsOpen() && !IsReadOnly(); }

public:
	// Wrapper functions for consistent StorageManager derivations
	// of the public interface funcs
	void OpenForWrite(const StorageMetaInfo& smi); friend struct StorageWriteHandle;
	TIC_CALL void CloseStorage() const; friend struct StorageCloseHandle;
	TIC_CALL bool OpenForRead(const StorageMetaInfo& smi) const; friend struct StorageReadHandle; // POSTCONDITION: m_IsOpen == returnValue


	//	Abstact interface
	TIC_CALL virtual bool AllowRandomTileAccess() const { return false; }
	TIC_CALL virtual bool EasyRereadTiles() const { return false; }
	TIC_CALL virtual bool CanWriteTiles() const { return false;  }
	TIC_CALL virtual bool IsWriteOnlyStorage() const { return false; }
	TIC_CALL virtual bool DoCheckFactorSimilarity(StorageMetaInfoPtr smi) const { return true; }
	TIC_CALL virtual bool DoCheck50PercentExtentOverlap(StorageMetaInfoPtr smi) const { return true; }

	TIC_CALL virtual prop_tables GetPropTables(const TreeItem* storageHolder=nullptr, TreeItem* curr=nullptr) const { return {}; }
	TIC_CALL virtual void OnTerminalDataItem(const AbstrDataItem* adi) const { };

	TIC_CALL virtual UInt32 GetNativeTileSizeX() const { return 0; }
	TIC_CALL virtual UInt32 GetNativeTileSizeY() const { return 0; }

	TIC_CALL virtual FileDateTime GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr relativePath) const;
	FileDateTime GetCachedChangeDateTime(const TreeItem* storageHolder, CharPtr relativePath) const;
	TIC_CALL virtual bool DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem = nullptr)  const; // Default implementation now checks existence of m_Name as a file
	TIC_CALL virtual bool DoCheckWritability() const;
	TIC_CALL virtual SharedStr GetUrl() const;

	// public interface funcs wrap derived StorageManagers virtual funcs
	void UpdateTree(const TreeItem* storageHolder, TreeItem* curr) const;

	TIC_CALL virtual ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* storageHolder, const TreeItem* self) const;

	// public interface funcs only implemented in NonmappableStorageManagers
	TIC_CALL virtual FileResult ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t);
	TIC_CALL virtual FileResult WriteDataItem(StorageMetaInfoPtr&& smiHolder);

	TIC_CALL virtual bool ReadUnitRange(const StorageMetaInfo& smi) const;
	TIC_CALL virtual bool WriteUnitRange(StorageMetaInfoPtr&& smi);

	void ExportMetaInfo(const TreeItem* storageHolder, const TreeItem* curr);

protected:
	// overridable helper functions which are only called from the wrapper funcs 
	TIC_CALL virtual void DoCreateStorage(const StorageMetaInfo& smi);
	TIC_CALL virtual void DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const;
	TIC_CALL virtual void DoCloseStorage(bool mustCommit) const;
	TIC_CALL virtual void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const;
	TIC_CALL virtual void DoWriteTree(const TreeItem* storageHolder);

public:
	using mutex_t = std::binary_semaphore;

	struct lock_t {
		lock_t(mutex_t& m) : m_Mutex(&m) { m_Mutex->acquire(); }
		lock_t(mutex_t& m, adopt_storage_lock_t) noexcept : m_Mutex(&m) {} // #933: already held; release on dtor, do not acquire
		lock_t(lock_t&& rhs) noexcept : m_Mutex(rhs.m_Mutex) { rhs.m_Mutex = nullptr; }
		lock_t& operator=(lock_t&&) = delete;
		~lock_t() { if (m_Mutex) m_Mutex->release(); }
		mutex_t* m_Mutex;
	};

	mutable mutex_t m_CriticalSection;

protected:
	TokenID              m_ID; // Token holding the name of the file
	mutable FileDateTime m_FileTime;
	mutable TimeStamp    m_LastCheckTS;

	bool   m_IsReadOnly : 1; // true  indicates that writing or creation is forbidden
	bool   m_IsOpenedForWrite : 1; // false indicates that so-far nothing was written. DoWriteTree is not desired
	mutable bool   m_IsOpen : 1;
	bool   m_Commit : 1;

//	friend struct TreeItem;

private:
	AbstrStorageManager(const AbstrStorageManager&) = delete;

	friend struct StorageClass;

	DECL_ABSTR(, Class)
};

// *****************************************************************************
// 
// NonmappableStorageManager
//
// *****************************************************************************


class NonmappableStorageManager : public AbstrStorageManager
{
	using base_type = AbstrStorageManager;
	// construction only from StorageMangers friends	
protected:
	TIC_CALL NonmappableStorageManager();
	TIC_CALL virtual ~NonmappableStorageManager();

public:

	auto ReaderClone(StorageMetaInfoPtr smi) const ->std::unique_ptr<StorageReadHandle>;

//	Abstact interface
	TIC_CALL virtual StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* curr, StorageAction sa) const;

	TIC_CALL virtual void StartInterest(const TreeItem* storageHolder, const TreeItem* self) const;
	TIC_CALL virtual void StopInterest (const TreeItem* storageHolder, const TreeItem* self) const noexcept;

	TIC_CALL virtual void DropStream(const TreeItem* item, CharPtr path);

	// public interface funcs wrap derived StorageManagers virtual funcs
	TIC_CALL virtual AbstrUnit* CreateGridDataDomain(const TreeItem* storageHolder);

private:
	using interest_holders_container = std::vector<SharedActorInterestPtr>;
	using interest_holders_key = Point<SharedTreeItem>;
	using interest_holders_map = std::map<interest_holders_key, interest_holders_container>;
	mutable interest_holders_map m_InterestHolders;

	DECL_ABSTR(, Class)
};


// *****************************************************************************
// 
// scoped StorageHandles
//
// *****************************************************************************


struct StorageCloseHandle
{
	TIC_CALL StorageCloseHandle(NonmappableStorageManager* storageManager, const TreeItem* storageHolder, const TreeItem* focusItem, StorageAction sa);
	TIC_CALL StorageCloseHandle(NonmappableStorageManager* storageManager, StorageMetaInfoPtr&& smi);
	TIC_CALL StorageCloseHandle(NonmappableStorageManager* storageManager, StorageMetaInfoPtr&& smi, adopt_storage_lock_t); // #933: adopt CS acquired at the scheduling gate

	TIC_CALL virtual ~StorageCloseHandle();

//	explicit operator bool() const { return m_StorageManager; }

	StorageMetaInfoPtr MetaInfo() const { return m_MetaInfo; }
	const TreeItem* StorageHolder() const { return MetaInfo()->StorageHolder(); }

	NonmappableStorageManager* StorageManager() const { return m_StorageManager.get(); }

	TreeItem* FocusItem() const { return const_cast<TreeItem*>(MetaInfo()->CurrRI().get()); }

protected:
	SharedPtr<NonmappableStorageManager> m_StorageManager;
	AbstrStorageManager::lock_t          m_StorageLock;
	StorageMetaInfoPtr                   m_MetaInfo;

private:
	TimeStamp                            m_TimeStampBefore = 0;

	void operator =(const StorageCloseHandle&) = delete;
	void operator =(StorageCloseHandle&&) = delete;
	StorageCloseHandle(const StorageCloseHandle&) = delete;
	StorageCloseHandle(StorageCloseHandle&& rhs) = delete;
};

struct StorageReadHandle : StorageCloseHandle
{
	TIC_CALL StorageReadHandle(NonmappableStorageManager* storageManager, const TreeItem* storageHolder, TreeItem* focusItem, StorageAction sa, bool mustRegisterFailure = true);
	TIC_CALL StorageReadHandle(NonmappableStorageManager* storageManager, StorageMetaInfoPtr&& smi);
	TIC_CALL StorageReadHandle(NonmappableStorageManager* storageManager, StorageMetaInfoPtr&& smi, adopt_storage_lock_t); // #933

	bool Read() const;

private:
	void Init();

	StorageReadHandle(const StorageReadHandle& rhs) = delete;
	void operator =(const StorageReadHandle&) = delete;
	void operator =(StorageReadHandle&&) = delete;
};

struct StorageWriteHandle : StorageCloseHandle
{
	TIC_CALL StorageWriteHandle(NonmappableStorageManager* storageManager, StorageMetaInfoPtr&&);
};

// *****************************************************************************
// 
// helper funcs
//
// *****************************************************************************

SharedStr GetConfigIniFileName();
SharedStr GetCaseDir(const TreeItem* configStore);
TIC_CALL bool IsInMMD(const AbstrDataItem*);

extern "C" TIC_CALL bool DMS_CONV DMS_IsConfigDirty(const TreeItem* configRoot);

#endif // __ABSTRSTORAGEMANAGER_H
