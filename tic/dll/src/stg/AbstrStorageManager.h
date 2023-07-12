//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once
// AStorageManager.h: interface for the AStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__ABSTRSTORAGEMANAGER_H)
#define __ABSTRSTORAGEMANAGER_H

/*
 *	AbstrStorageManager
 *
 *	This is a contract for a storage definition based on a file	mechanism
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

#include "TicBase.h"
#include "act/ActorEnums.h"
#include "TreeItem.h"
struct ActorVisitor;

enum SyncMode { SM_AllTables, SM_AttrsOfConfiguredTables, SM_None };
		
#include "ptr/InterestHolders.h"
#include "ptr/SharedStr.h"

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

	template <typename Derived>
	std::weak_ptr<Derived> weak_from_base()
	{
		return shared_from_base<Derived>();
	}

	template <typename Derived>
	std::weak_ptr<const Derived> weak_from_base() const
	{
		return shared_from_base<Derived>();
	}

	std::weak_ptr<Base> weak_from_this()
	{
		return this->shared_from_this();
	}

	std::weak_ptr<const Base> weak_from_this() const
	{
		return this->shared_from_this();
	}
};

enum class StorageAction { read, write, updatetree, writetree };

struct StorageMetaInfo : std::enable_shared_from_this<StorageMetaInfo>
{
	StorageMetaInfo(AbstrStorageManager* storageManager)
		: m_StorageManager(storageManager)
	{}

	StorageMetaInfo(const TreeItem* storageHolder, const TreeItem* curr)
		: m_StorageManager(storageHolder->GetStorageManager())
		, m_StorageHolder(storageHolder)
		, m_Curr(curr)
		, m_RelativeName(storageHolder->DoesContain(curr) ? curr->GetRelativeName(storageHolder).c_str() : curr->GetFullName().c_str())
	{}
	TIC_CALL virtual ~StorageMetaInfo();
	TIC_CALL virtual void OnPreLock();
	TIC_CALL virtual void OnOpen();

	TIC_CALL const TreeItem*      CurrRI() const { return m_Curr;  }
	TIC_CALL const AbstrDataItem* CurrRD() const;
	TIC_CALL const AbstrUnit*     CurrRU() const;
	AbstrDataItem* CurrWD() const { return const_cast<AbstrDataItem*>(CurrRD()); }
	AbstrUnit*     CurrWU() const { return const_cast<AbstrUnit*>(CurrRU()); }
	TreeItem*      CurrWI() const { return const_cast<TreeItem*>(CurrRI()); }

	AbstrStorageManager* StorageManager() const { return m_StorageManager; }
	const TreeItem* StorageHolder() const { return m_StorageHolder; }

protected:
	SharedPtr<AbstrStorageManager> m_StorageManager;
	SharedPtr<const TreeItem> m_StorageHolder, m_Curr;
public:
	SharedStr m_RelativeName;
	bool      m_MustRememberFailure = true;
	std::mutex m_TileReadSection;
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



class AbstrStorageManager : public PersistentSharedObj
{
	typedef PersistentSharedObj base_type;

public:
//	Static interface functions
	TIC_CALL static AbstrStorageManagerRef Construct(CharPtr fullStorageName, TokenID typeID, bool readOnly, bool throwOnFailure, item_level_type itemLevel);
	TIC_CALL static AbstrStorageManagerRef Construct(const TreeItem* holder, SharedStr relStorageName, TokenID typeID, bool readOnly, bool throwOnFailure = true);
	TIC_CALL static bool                 DoesExistEx(CharPtr name, TokenID typeID, const TreeItem* storageHolder); // XXX TODO, REPLACE CharPtr by SharedCharArray*
	TIC_CALL static SharedStr            Expand(const TreeItem* configStore, SharedStr storageName);
	TIC_CALL static SharedStr            Expand(CharPtr configDir, CharPtr storageName);
	TIC_CALL static SharedStr            GetFullStorageName(const TreeItem* configStore, SharedStr storageName); // ForItem
	TIC_CALL static SharedStr            GetFullStorageName(CharPtr subDir, CharPtr storageNameCStr); // ForFolder
	TIC_CALL static SyncMode             GetSyncMode(const TreeItem* storageHolder);

//	override / extent PerssistentRefObject interface
public:
	TIC_CALL SharedStr GetNameStr() const;

	TIC_CALL void DoNotCommitOnClose() { m_Commit = false; }
	TIC_CALL bool DoesExist (const TreeItem* storageHolder)  const; // returns IsOpen() || DoCheckExistence()
	TIC_CALL bool IsWritable() const;
	bool IsOpen        () const { return m_IsOpen;  }
	bool IsReadOnly    () const { return m_IsReadOnly; }
	bool IsOpenForWrite() const { return IsOpen() && !IsReadOnly(); }
	TIC_CALL AbstrStorageManagerRef ReaderClone(const StorageMetaInfo& smi) const;

//	Abstact interface
	TIC_CALL virtual bool AllowRandomTileAccess() const { return false;  }
	TIC_CALL virtual bool EasyRereadTiles() const { return false; }

	TIC_CALL virtual FileDateTime GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr relativePath) const;
	TIC_CALL FileDateTime GetCachedChangeDateTime(const TreeItem* storageHolder, CharPtr relativePath) const;

	TIC_CALL virtual StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* curr, StorageAction sa) const;

	TIC_CALL virtual bool ReadDataItem  (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)=0;
	TIC_CALL virtual bool WriteDataItem (StorageMetaInfoPtr&& smiHolder);

	TIC_CALL virtual bool ReadUnitRange (const StorageMetaInfo& smi) const;
	TIC_CALL virtual bool WriteUnitRange(StorageMetaInfoPtr&& smi);

	TIC_CALL virtual ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* storageHolder, const TreeItem* self) const;
	TIC_CALL virtual void StartInterest(const TreeItem* storageHolder, const TreeItem* self) const;
	TIC_CALL virtual void StopInterest (const TreeItem* storageHolder, const TreeItem* self) const noexcept;

	TIC_CALL virtual void DropStream(const TreeItem* item, CharPtr path);

	// public interface funcs wrap derived StorageManagers
	// code is left here for instructional purposes and is to be removed to impl

	TIC_CALL void ExportMetaInfo(const TreeItem* storageHolder, const TreeItem* curr);
	TIC_CALL void UpdateTree(const TreeItem* storageHolder, TreeItem* curr) const;
	TIC_CALL virtual SharedStr GetUrl() const;
	TIC_CALL virtual bool CanWriteTiles() const;
	TIC_CALL virtual AbstrUnit* CreateGridDataDomain(const TreeItem* storageHolder);

protected:
	// construction only from StorageMangers friends	
	TIC_CALL AbstrStorageManager();
	TIC_CALL void InitStorageManager(CharPtr storageName, bool readOnly, item_level_type itemLevel);
	TIC_CALL virtual ~AbstrStorageManager();

	// overridable helper functions which are only called from the wrapper funcs 
	TIC_CALL virtual void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const;
	TIC_CALL virtual void DoWriteTree (const TreeItem* storageHolder);
	TIC_CALL virtual bool DoCheckExistence(const TreeItem* storageHolder)  const; // Default implementation now checks existence of m_Name as a file
	TIC_CALL virtual bool DoCheckWritability() const;

	TIC_CALL virtual void DoCreateStorage(const StorageMetaInfo& smi);
	TIC_CALL virtual void DoOpenStorage  (const StorageMetaInfo& smi, dms_rw_mode rwMode) const;
	TIC_CALL virtual void DoCloseStorage (bool mustCommit) const;

public:
	TIC_CALL void OpenForWrite(const StorageMetaInfo& smi); friend struct StorageWriteHandle;
	TIC_CALL void CloseStorage() const; friend struct StorageCloseHandle;
	//	helper functions
private:
	// Wrapper functions for consistent StorageManager derivations
	// of the public interface funcs
	TIC_CALL bool OpenForRead (const StorageMetaInfo& smi) const; friend struct StorageReadHandle; // POSTCONDITION: m_IsOpen == returnValue

public:
	using mutex_t = leveled_critical_section;
	using lock_t = mutex_t::scoped_lock;
	mutable mutex_t m_CriticalSection;

protected: friend struct StorageClass;
	TokenID              m_ID; // Token holding the name of the file
	mutable FileDateTime m_FileTime;
	mutable TimeStamp    m_LastCheckTS;

	bool   m_IsReadOnly        : 1; // true  indicates that writing or creation is forbidden
	bool   m_IsOpenedForWrite  : 1; // false indicates that so-far nothing was written. DoWriteTree is not desired

private:
	mutable bool   m_IsOpen : 1;
	bool   m_Commit         : 1;

	typedef std::vector<InterestPtr<const Actor*>> interest_holders_container;
	typedef Point<const TreeItem*> interest_holders_key;
	typedef std::map<interest_holders_key, interest_holders_container> interest_holders_map;
	mutable interest_holders_map m_InterestHolders;

	friend struct TreeItem;
	AbstrStorageManager(const AbstrStorageManager&);
	static bool s_ReduceResources(ClientHandle clientHandle);
	DECL_ABSTR(TIC_CALL, Class)

};


// *****************************************************************************
// 
// scoped StorageHandles
//
// *****************************************************************************


struct StorageCloseHandle
{
	TIC_CALL StorageCloseHandle(const AbstrStorageManager* sm, const TreeItem* storageHolder, const TreeItem* focusItem, StorageAction sa);
	TIC_CALL StorageCloseHandle(StorageMetaInfoPtr&& smi);
	TIC_CALL virtual ~StorageCloseHandle();

	explicit operator bool() const { return m_StorageManager; }

	StorageMetaInfoPtr MetaInfo() const { return m_MetaInfo; }
	TreeItem* FocusItem() const { return const_cast<TreeItem*>(m_FocusItem.get_ptr()); }

private:
	void Init(const AbstrStorageManager* storageManager, const TreeItem* storageHolder, const TreeItem* focusItem);

protected:
	StorageMetaInfoPtr                   m_MetaInfo;
	SharedPtr<const AbstrStorageManager> m_StorageManager;
	SharedTreeItem                       m_StorageHolder, m_FocusItem;
private:
	AbstrStorageManager::lock_t    m_StorageLock;
	TimeStamp                      m_TimeStampBefore;

	void operator =(const StorageCloseHandle&) = delete;
	void operator =(StorageCloseHandle&&) = delete;
	StorageCloseHandle(const StorageCloseHandle&) = delete;
	StorageCloseHandle(StorageCloseHandle&& rhs) = delete;
};

struct StorageReadHandle : StorageCloseHandle
{
	TIC_CALL StorageReadHandle(const AbstrStorageManager* sm, const TreeItem* storageHolder, TreeItem* focusItem, StorageAction sa, bool mustRegisterFailure = true);
	TIC_CALL StorageReadHandle(StorageMetaInfoPtr&& info);

	bool Read() const;

private:
	void Init();

	StorageReadHandle(const StorageReadHandle& rhs) = delete;
	void operator =(const StorageReadHandle&) = delete;
	void operator =(StorageReadHandle&&) = delete;
};

struct StorageWriteHandle : StorageCloseHandle
{
	TIC_CALL StorageWriteHandle(StorageMetaInfoPtr&&);
};

// *****************************************************************************
// 
// helper funcs
//
// *****************************************************************************

TIC_CALL SharedStr GetConfigIniFileName();
TIC_CALL SharedStr GetCaseDir(const TreeItem* configStore);
TIC_CALL bool GetEnvStr(CharPtr section, CharPtr key, SharedStr& result);

extern "C" TIC_CALL bool DMS_CONV DMS_IsConfigDirty(const TreeItem* configRoot);

#endif // __ABSTRSTORAGEMANAGER_H
