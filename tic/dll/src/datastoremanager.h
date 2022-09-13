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

#if !defined(__TIC_DATASTOREMANAGER_H)
#define __TIC_DATASTOREMANAGER_H

#include "TicBase.h"

struct TreeItemDualRef;
struct DataStoreManager;
struct TreeItem;

#include "act/MainThread.h"
#include "mci/PropDef.h"
#include "mci/ValueClass.h"
#include "ptr/WeakPtr.h"
#include "ser/SafeFileWriter.h"
#include "utl/mySPrintF.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "AbstrDataObject.h"
#include "DataItemClass.h"
#include "DataController.h"

//#include "stg/AbstrStorageManager.h"
using stream_id_t = UInt32 ;
class AbstrStorageManager;
SharedStr MakeTmpStreamName(stream_id_t lastStreamID);

//	*****************************************************************************
//	DataStoreRegister
//
//	maintains persistent assocs:
//		di-expr: LispRef	->	fileName, domain-expr)
//		terminal			->	TimeStamp DataSource (config or storage)
//	is initialized by DataStoreManager on opening an existing Storage
//	
//	*****************************************************************************

// *****************************************************************************
// DataItemAssoc (singleton assoc: AbstrDataItem->DataItemInfo)
// *****************************************************************************
//
// Mappings
//
//	DataObj -> Seq 0 := (rw_file_view | allocated_vector)
//
//	DC 0-> DataObj -> KeepDataStore: bool
//	DC 0-> file_name


// mappings: DataItem -> DC -> RootItem -> DataStoreManager

/* 
struct TreeItemDcMap: std::map<const TreeItem*, DataControllerRef>
{
	~TreeItemDcMap() { dms_assert(!size()); }
};
*/

struct DataStoreRecord
{
	SharedStr fileNameBase;
	BlobBuffer blob;
	TimeStamp ts = 0;
};

using DataStoreMap = std::map<LispRef, DataStoreRecord>;

struct Ft2TsMap: std::map<FileDateTime, TimeStamp> {};

const TreeItem* GetCacheRoot(const TreeItem* subItem);

//	*****************************************************************************
//	DataStoreManager (logically one instance per configuration; physically a singleton)
//	
//		manages BigDataItems that can be 
//			Stored, 
//			Loaded, 
//			Dropped
//			Refreshed in a history list
//
// grannularity of operation: 
//	CacheRoots
// 
//	filenameBase from Config
//		cond for write: isPersistent := (root->FreeData is false)
//			inMapping: isPersistent, alleen dan in m_DcStoreMap
//			creation if FileTileArray by DataWriteLock constructor
//          completion of CalcData must arrange for StoreBlob of non-mapped parts of m_Data.
//			units are always written as soon as calculation is ready (as continuation)
//		cond for read: m_DcStoreMap
//		teruglezen data-item: check Hash domain en valuesunits uit LoadBlob en/of lees de hele data
// 
//	new calculations: FileTileArrays are created by DataWriteLock 
//	reopening...
//	contains
//	-	DataItemQueue: historical list of Big DataItems in mem that are
//		possibly elegible for TrySwapout() -> TryCleanupMem()
//  -	(fss) StorageManager for external storage of BigDataItem data
//
//	*****************************************************************************

struct DataStoreManager
{
	DataStoreManager(const TreeItem* configRoot); // , SharedPtr<AbstrStreamManager> );
	~DataStoreManager();
	void Clear();

//	store DC if no prior persistent data was found but adi is elegible for persistence: !GetFreeDataState() && IsFileable(adi)

	static DataStoreManager* Create(const TreeItem* configRoot);
	static DataStoreManager* Get   (const TreeItem* configRoot);
	TIC_CALL static DataStoreManager* Curr  ();
	static bool HasCurr();

	const DataStoreRecord* GetStoreRecord(LispPtr keyExpr) const;

	void RegisterFilename(const DataController* dc, const TreeItem* config);
	void Unregister(LispPtr keyExpr);

//	void LoadBlob(DataControllerRef dcRef) const;

	TimeStamp DetermineExternalChange(const FileDateTime& fdt);
	void      RegisterExternalTS(const FileDateTime& fdt, TimeStamp internalTS);

	void InvalidateDC(const DataController* dcPtr)
	{
		dms_assert(dcPtr);
		if (dcPtr->m_State.Get(DCF_CacheRootKnown))
			Unregister(dcPtr->GetLispRef());

	}

	SafeFileWriterArray* GetSafeFileWriterArray() { return &m_FileWriters; }

	void CloseCacheInfo(bool alsoSave);

	bool IsCancelling() const { return m_IsCancelling;  }

private:
	DataStoreManager(const DataStoreManager&); // not implemented
	void LoadCacheInfo();
	void SaveCacheInfo();

	void CleanupTmpCache();

	void SetDirty() noexcept;

//	SharedPtr<AbstrStreamManager> m_StreamManager; // CalcCacheDirInfo

	FileDateTime                  m_CacheCreationTime = 0;
public: // TODO G8.5: encapsulate
	std::atomic<stream_id_t>      m_LastStreamID = 0; // ordinal for generating unique tmp names
	const TreeItem*               m_ConfigRoot; // XXX

//	DataStoreMap                  m_StoreMap;

	Ft2TsMap                      m_Ft2TsMap;  // TODO G8.5: verwerk alle TimeStamps in LispExpr

	SafeFileWriterArray           m_FileWriters;

	bool                          m_IsDirty = false;
	std::atomic<bool>             m_IsCancelling = false;

	static WeakPtr<DataStoreManager> s_CurrDSM; friend struct SessionData; friend struct DataController;
	

public: // ==== code analysis support: DMS_TreeItem_SetAnalysisSource
	std::map<const Actor*, UInt32> m_SupplierLevels;
	SharedPtr<const TreeItem>      m_SourceItem;
};

struct dsm_reentrant_critical_section {
	dsm_reentrant_critical_section(item_level_type itemLevel, ord_level_type level, CharPtr descr)
		:	m_NonReentrantSection(itemLevel, level, descr)
		,	m_ThreadID(UNDEFINED_VALUE(dms_thread_id))
		,	m_RecursionCount(0)
	{}

	struct scoped_lock {
		scoped_lock(dsm_reentrant_critical_section& section)
			: m_Section(section)
		{
			section.lock();
		}
		~scoped_lock() // we still posess a reentrant lock on this thread, release if re-entrancy 
		{
			m_Section.unlock();
		}
		dsm_reentrant_critical_section& m_Section;
	};

private: friend struct scoped_lock;
		void lock()
		{
			if (m_ThreadID != GetThreadID())
			{
				m_NonReentrantSection.lock(); // if locked by other thread: wait
				dms_assert(m_RecursionCount == 0);
				m_ThreadID = GetThreadID();
			}
			++m_RecursionCount;
		}
		 
		void unlock()
		{
			dms_assert(m_ThreadID == GetThreadID());
			if (!--m_RecursionCount) // zero must be visible to other threads before section is unlocked.
			{
				m_ThreadID = UNDEFINED_VALUE(dms_thread_id);
				m_NonReentrantSection.unlock();
			}
		}
	leveled_critical_section m_NonReentrantSection;
	dms_thread_id   m_ThreadID;
	volatile UInt32 m_RecursionCount;
};

//typedef dsm_section_type::_Scoped_lock scoped_lock;
using dsm_section_type = dsm_reentrant_critical_section;

extern dsm_section_type  s_DataStoreManagerSection;
extern leveled_counted_section s_DataStoreManagerUsageCounter;


#endif //!defined(__TIC_DATASTOREMANAGER_H)
