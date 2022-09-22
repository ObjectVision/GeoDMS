#include "TicPCH.h"
#pragma hdrstop

#include "ppltasks.h"

#include "DataStoreManager.h"
#include "DataStoreManagerCaller.h"

#include "act/ActorLock.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "dbg/Debug.h"
#include "dbg/DmsCatch.h"
#include "geo/PointOrder.h"
#include "ptr/SharedBase.h"
#include "ser/PolyStream.h"
#include "ser/PointStream.h"
#include "set/IndexedStrings.h"
#include "set/Token.h"
#include "utl/Environment.h"
#include "utl/SplitPath.h"

#include "stg/AbstrStreamManager.h"

#include "AbstrUnit.h"
#include "DataLocks.h"
#include "DataController.h"
#include "FreeDataManager.h"
#include "ItemLocks.h"
#include "OperationContext.h"
#include "SessionData.h"

#include "LispRef.h"
#include "LispTreeType.h"

#if defined(MG_DEBUG)
const bool MG_DEBUG_DATASTOREMANAGER      = true;
const bool MG_DEBUG_DATASTOREMANAGER_LOAD = false;
#endif

#define MG_REPORT_ALL_CACHE_TRANSACTIONS

// *****************************************************************************
//										DataStoreManager
// *****************************************************************************


/* ISSUES
- Explicit Supplier Items
  Voorbeeld: a: maak en bewaar data in externe storage x; b: result of externe processing van x
- Parents
- Compound Results
- Unit props
- Lookahead locks
  zijn future data request counts die beginnen bij een 
  DMS_TreeItem_Updata (in pre-update fase) of al eerder,
  die geplaatst worden op een DataController, 
  die ze doorplaatst bij z'n args indien deze invalide is.
  na gebruik van het arg (na berekening van de data van this),
  wordt de data weer afgelaagd; bij 0 wordt de data geflushd
  Lookahead locks betreffen interesse in de data, te onderscheiden van managed actors, 
  die interesse in het up-to-date van de gehele item-state betreft (incl. externe storage).
- Read Locks / Write Locks. Moeten tijdelijk zijn (thread-local).
- Flushen of storen.
  gezien de hoge kosten van opslaan, is free-en een optie indien
  de opvraagfrequentie gering of de rekentijd niet al te hoog is en de suppliers beschikbaar.

  Dit is een moeilijke afweging. 
  Vooralsnog moet dit per item geconfigureerd worden.
  Alle virtuele items hebben geen storage
- Invalidation by datachange, this->exprchange, supplier->exprchange, etc.
  Doe in idletime van alle up-to date dingen een state check en update vervolgens alle (zojuist) geinvalideerde items
*/

/*

Relatie DataCache en DiscCache

state characteristics:

	TI->KeepData()           implies DC.InterestCount()

	DC.InterestCount()       implies DataStoreEntry
									 and     (DataInMem() OR DCE->IsSaved())

	DC.m_Data.DataReadLock() implies DataInMem()
	not DC.IsValid()         implies not DCE->IsSaved()

Persistence:
	Terminals krijgen in nieuwe sessie timestamp van file (config file igv config-data).
	Andere cache entries 

Dynamics

*/

dsm_section_type  s_DataStoreManagerSection(item_level_type(0), ord_level_type(2), "DataStoreManageOperations");
leveled_counted_section s_DataStoreManagerUsageCounter(item_level_type(0), ord_level_type(1), "DataStoreManagerUsageCounter");

//----------------------------------------------------------------------
// helper func
//----------------------------------------------------------------------

const TreeItem* GetCacheRoot(const TreeItem* subItem)
{
	if (subItem->IsCacheItem())
	{
		while (auto parent = subItem->GetTreeParent())
			subItem = parent;
		dms_assert(subItem->IsCacheRoot());
	}
	return subItem;
}

void Report(const AbstrDataItem* adi, CharPtr func)
{
#if !defined(MG_REPORT_ALL_CACHE_TRANSACTIONS)
	if ( !(adi->IsFnKnown()) && !adi->HasConfigSource())
		return; // don't report on transient MappedFiling
#endif defined(MG_REPORT_ALL_CACHE_TRANSACTIONS)

	reportF(SeverityTypeID::ST_MajorTrace, "%s: %s",
		adi->GetSourceName().c_str()
	,	func
	);
}

TIC_CALL bool CanHaveData(const TreeItem* item)
{
	return IsDataItem(item) || IsUnit(item) && AsUnit(item)->HasVarRange();
}

//----------------------------------------------------------------------
// DataStoreManager
//----------------------------------------------------------------------

WeakPtr<DataStoreManager> DataStoreManager::s_CurrDSM;

// ===============  ctor/dtor

DataStoreManager::DataStoreManager(const TreeItem* configRoot) //, SharedPtr<AbstrStreamManager> sm)
	: m_ConfigRoot(configRoot)
{
	s_CurrDSM = this;
/*
	try {
		sm->OpenForWrite(StorageMetaInfo(m_StreamManager));
	}
	catch (const DmsException& x) {
		s_CurrDSM = nullptr;
		auto errMsg = x.AsErrMsg();
		errMsg.TellExtraF("While opening the CalcCache for a configuration. It seems that this CalcCache is already open.");
		throw DmsException(errMsg);
	}
	catch (...) {
		s_CurrDSM = nullptr;
		throw;
	}
*/
	DMS_SetASyncContinueCheck([]() { DSM::CancelIfOutOfInterest();  });
}

DataStoreManager::~DataStoreManager()
{
//	m_StreamManager->CloseStorage();

//	dms_assert(m_StoreMap.empty());
	s_CurrDSM = nullptr;
}

template <typename Set>
void move_and_clear(Set& s)
{
#if defined(MG_DEBUG)
	Set tmp; // effectively remove before destroy refs.
	s.swap( tmp );
	tmp.clear();
#else
	s.clear();
#endif
	dms_assert(s.empty());
}

const DataStoreRecord* DataStoreManager::GetStoreRecord(LispPtr keyExpr) const 
{ 
	return nullptr;
/*
	auto pos = m_StoreMap.find(keyExpr);
	if (pos == m_StoreMap.end())
		return nullptr;
	return &(pos->second);
	*/
}
void DataStoreManager::Unregister(LispPtr keyExpr)
{
//	m_StoreMap.erase(keyExpr);
}

void DataStoreManager::Clear()
{
	dms_assert(IsMainThread());

//	move_and_clear( m_StoreMap); 
}

// ===============  DataStoreManager Persistence

void CleanupMess(AbstrStreamManager* sm)
{
	KillFileOrDir(DelimitedConcat(sm->GetNameStr().c_str(), ".tmp")); // clean up mess from last session before reading stuff.
}

#include "xct/DmsException.h"

void DataStoreManager::LoadCacheInfo()
{
	dms_assert(IsMainThread());
/*
	leveled_counted_section::scoped_lock storeLock(s_DataStoreManagerUsageCounter);
	dsm_section_type::scoped_lock sectionLock(s_DataStoreManagerSection);

	m_IsDirty = false;

	DBG_START("DataStoreManager", "LoadCacheInfo", MG_DEBUG_DATASTOREMANAGER_LOAD);

	dms_assert(m_StreamManager);
	dms_assert(SessionData::Curr()->GetConfigRoot() == m_ConfigRoot);

	CleanupTmpCache();

	if (!m_StreamManager->IsOpen())
		return;

	auto is = m_StreamManager->OpenInpStream(StorageMetaInfo(m_StreamManager), "CacheInfo");
	if (!is)
		return;

	std::vector<UpdateMarker::TimeStampPtr> timeStampRenumberReg;
	PolymorphInpStream pis(is.get());
	TimeStamp lastTS;
	stream_id_t lastSID;
	pis >> lastTS >> lastSID;
	m_LastStreamID = lastSID;
	pis.Buffer().ReadBytes((char*)&m_CacheCreationTime, sizeof(FileDateTime));

	DBG_TRACE(("Last TS      %u", lastTS));
	DBG_TRACE(("LastStreamID %I64u", (UInt64)m_LastStreamID));
	DBG_TRACE(("CreationTime %s", AsDateTimeString(m_CacheCreationTime).c_str()));

	// Read DcFileNameMap

	SizeT nrAssocs;

	// Read TimeStamp records for each ExternalSource FullName token
	pis >> nrAssocs; 
	timeStampRenumberReg.reserve(nrAssocs);
	DBG_TRACE(("nrTimeAssocs %I64u", (UInt64)nrAssocs));
	while (nrAssocs--)
	{
		FileDateTime externalTime;
		pis.Buffer().ReadBytes((char*)&externalTime, sizeof(FileDateTime));
		TimeStamp& internalTime = m_Ft2TsMap[externalTime];
		internalTime = pis.ReadUInt32();

		dms_assert(internalTime > 0);
		if (internalTime > 0)
			timeStampRenumberReg.push_back(&(internalTime));
		DBG_TRACE(("External FileDateTime %s has TimeStamp 0x%x", 
			AsDateTimeString(externalTime).c_str(),
			internalTime
		));
	}

	// read StoreRecords
	pis >> nrAssocs; 
	DBG_TRACE(("nrFileAssocs %I64u", UInt64(nrAssocs)));
	timeStampRenumberReg.reserve(timeStampRenumberReg.size()+nrAssocs);
	while (nrAssocs--)
	{
		LispRef lispRef;
		pis >> lispRef;

		auto& storeRecord = m_StoreMap[lispRef];

		pis >> storeRecord.fileNameBase;
		dms_assert(storeRecord.fileNameBase.find('\\') == storeRecord.fileNameBase.send());

		SizeT sz; sz = pis.ReadUInt32();
		storeRecord.blob.reserve(sz);
		pis.Buffer().ReadBytes(storeRecord.blob.begin(), sz);

		storeRecord.ts = pis.ReadUInt32();
		if (storeRecord.ts)
			timeStampRenumberReg.push_back(&(storeRecord.ts));
	}

	UpdateMarker::Renumber(
		begin_ptr( timeStampRenumberReg ), 
		end_ptr  ( timeStampRenumberReg )
	);
	// reserve TS for external out-of-session change events, 
	// to be signalled in DetermineLastSupplierChange
	UpdateMarker::GetFreshTS(MG_DEBUG_TS_SOURCE_CODE("DataStoreManager::LoadCacheInfo() after UpdateMarker::Renumber")); 
*/
}

void DataStoreManager::CloseCacheInfo(bool alsoSave)
{
	m_IsCancelling = true;
	{
		leveled_counted_section::scoped_lock lock(s_DataStoreManagerUsageCounter); // wait for readers

		if (alsoSave)
			SaveCacheInfo();
	}
	CleanupTmpCache();
}

[[noreturn]] void DSM::CancelOrThrow(const TreeItem* item)
{
	if (OperationContext::CancelableFrame::CurrActive())
		concurrency::cancel_current_task(); // assume it was cancelled due to outdated suppliers

	if (item)
		throwPreconditionFailed(item->GetConfigFileName().c_str(), item->GetConfigFileLineNr(), "CancelOrThrow requested without CancelableFrame");
	throwCheckFailed(MG_POS, "CancelOrThrow requested without CancelableFrame and without Item");
}

void DSM::CancelIfOutOfInterest(const TreeItem* item)
{
	if (IsMainThread() && !OperationContext::CancelableFrame::CurrActive())
		return;

	OperationContext::CancelableFrame::CurrActiveCancelIfNoInterestOrForced(DSM::IsCancelling());

	if (OperationContext::CancelableFrame::CurrActiveCanceled() && !std::uncaught_exceptions())
	{
		CancelOrThrow(item);
	}
}


void DataStoreManager::SaveCacheInfo()
{

	dsm_section_type::scoped_lock sectionLock(s_DataStoreManagerSection);
/*
	DBG_START("DataStoreManager", "SaveCacheInfo", MG_DEBUG_DATASTOREMANAGER_LOAD);

	if (!m_IsDirty)
		return;

	// SAVE persistent info of DataStoreManager
	if (m_StreamManager->IsOpenForWrite())
	{
		auto os = m_StreamManager->OpenOutStream(StorageMetaInfo(m_StreamManager), "CacheInfo", no_tile);
		if (os)
		{
			if (!m_CacheCreationTime)
				m_CacheCreationTime = m_StreamManager->GetLastChangeDateTime(m_ConfigRoot, "CacheInfo");
			PolymorphOutStream pos(os.get());
			pos << UpdateMarker::GetLastTS();
			pos << m_LastStreamID;
			pos.Buffer().WriteBytes((char*)&m_CacheCreationTime, sizeof(FileDateTime));
			
			DBG_TRACE(("Last TS      %d", UpdateMarker::GetLastTS()));
			DBG_TRACE(("LastStreamID %d", m_LastStreamID));
			DBG_TRACE(("CreationTime %s", AsDateTimeString(m_CacheCreationTime).c_str()));

		//	Write TimeStamp for each ExternalSource item indicated by its FullName token
			pos << m_Ft2TsMap.size();
			DBG_TRACE(("nrFt2TsAssocs %I64u", (UInt64)m_Ft2TsMap.size()));

			for(Ft2TsMap::const_iterator tii = m_Ft2TsMap.begin(), tie = m_Ft2TsMap.end(); tii != tie; ++tii)
			{
				dms_assert(!(tii->first == FileDateTime()));
				dms_assert(tii->second > 0);
				pos.Buffer().WriteBytes((char*)&tii->first, sizeof(FileDateTime));
				pos.WriteUInt32( tii->second);

				DBG_TRACE(("External FileDateTime %s has TimeStamp 0x%x", 
					AsDateTimeString(tii->first).c_str(),
					tii->second
				));
			}

		//	Write DcFileNameMap
			//	ISSUE:    In next session we want to be sure that each registered DiscCacheEntry
			//	          was valid relative to the registered versions of the ExternalSources
			//  GOAL:     remove:
			//            - Dc's that uses SymDC's that refer to no longer existing TreeItems
			//            - Invalidated Dc's (seen unprocessed change in suppliers -> external data was removed)
			//            - Invalid Dc's (unseen changes in suppliers -> invalid external data is still out there)
			//	ORG SOL:  clean up invalid stuff and don't save them
			//	          let GetState invalidate on the basis of changed but not-yet evaluated
			//	          TimeStamp changes of ExternalSources
			//	PROBLEMS: - GetState would search for currently non-existing items.or items from other configs
			//	          - GetState would expand non visited template-instantiations
			//            - GetState is only valid for DC's that were associated to - and accessed from - a TI.
			//            - when to call?
			//	ACTION:   Just save lastChangeTS and invalidate in next session 
			//	          IF and WHEN DC is queried for result
			//	CONS:     Read-back timestamps remain essential for assessing validity of DiscCacheEntries
			//	SOLUTION: Renumber written timestamps in order to save the ChroneSpace
			//  DRAWBACK: CacheInfo.dmsdata only becomes bigger; no cleanup

			SizeT nrFileMaps = m_StoreMap.size();
			pos << nrFileMaps;
			DBG_TRACE(("nrFileAssocs %I64u", (UInt64)nrFileMaps));

			for (const auto& storeRecord: m_StoreMap)
			{
				pos << storeRecord.first;

				pos << storeRecord.second.fileNameBase; // don't determine, just register which change was seen

				pos.WriteUInt32(storeRecord.second.blob.size());
				pos.Buffer().WriteBytes(storeRecord.second.blob.begin(), storeRecord.second.blob.size()); // write blob

				pos.WriteUInt32( storeRecord.second.ts );
			}
		}
	}
*/
	m_IsDirty = false;
}

void DataStoreManager::SetDirty() noexcept
{
	m_IsDirty = true;
}

void DataStoreManager::CleanupTmpCache()
{
//	CleanupMess(m_StreamManager);
}

// ===============  public statics for near singleton management 

DataStoreManager* DataStoreManager::Create(const TreeItem* configRoot)
{
	dms_assert(configRoot);
	dms_assert(!configRoot->GetTreeParent());
	dms_assert(!configRoot->IsCacheItem());
	dms_assert(configRoot->IsAutoDeleteDisabled()); // after destruction has started no more new creations

	#if defined(MG_DEBUG_DATA)
		dms_assert(!configRoot->Actor::m_State.Get(ASFD_SetAutoDeleteLock)); // after closing this, no more creations wanted
	#endif

		
	configRoot->DetermineState();

	return new DataStoreManager(configRoot);
}

DataStoreManager* DataStoreManager::Get(const TreeItem* configRoot)
{
	SessionData::ActivateIt(configRoot);
	return Curr();
}

bool DataStoreManager::HasCurr()
{
	return s_CurrDSM;
}

DataStoreManager* DataStoreManager::Curr()
{
	auto currDSM = s_CurrDSM;
	dbg_assert(SessionData::Curr()->GetDSM() == currDSM);
	return currDSM;
}

void DataStoreManager::RegisterFilename(const DataController* dc, const TreeItem* config)
{
	dms_assert(dc);
	dms_assert(config);
//	m_StoreMap[dc->GetLispRef()].fileNameBase = config->GetFullName();
	dc->m_State.Set(DCF_CacheRootKnown);
}

// ===============  implement public data store functions

TimeStamp DataStoreManager::DetermineExternalChange(const FileDateTime& fdt)
{
	dms_assert(!(fdt == FileDateTime()));

	dsm_section_type::scoped_lock sectionLock(s_DataStoreManagerSection);
	TimeStamp& ts= m_Ft2TsMap[fdt];

	if (!ts)	
	{
		ts = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("DataStoreManager::DetermineExternalChange"));
		SetDirty();
	}
	dms_assert(ts <= UpdateMarker::LastTS()); // check occurs at later era than last recorded change
	dms_assert(ts > 0);
	return ts;
};

void DataStoreManager::RegisterExternalTS(const FileDateTime& fdt, TimeStamp internalTS)
{
	dsm_section_type::scoped_lock sectionLock(s_DataStoreManagerSection);

	dms_assert(fdt != FileDateTime());
	dms_assert(internalTS);

	if (fdt == FileDateTime())
		return;

	Ft2TsMap::iterator assocPtr = m_Ft2TsMap.lower_bound(fdt);
	if ((assocPtr != m_Ft2TsMap.end()) && (assocPtr->first == fdt))
	{
		if (assocPtr->second != internalTS)
			reportF(SeverityTypeID::ST_Error, "External FileDateTime %s was already registered at TimeStamp 0x%x which conflict with current registration of 0x%x",
				AsDateTimeString(fdt).c_str(),
				assocPtr->second,
				internalTS
			);
		return; // fdt was alredy registered
	}

	m_Ft2TsMap.insert(assocPtr, std::make_pair(fdt, internalTS));
	SetDirty();
};

// ===================================================== usage of m_SupplierLevels;

#include "act/ActorVisitor.h"
#include "StateChangeNotification.h"

const UInt32 SL_USAGE_MASK  = 0x03;
const UInt32 SL_USES_SOURCE = 0x04;

bool MarkSources(DataStoreManager* dsm, const Actor* a, UInt32 level)
{
	dms_assert(a);
	if (a->IsPassorOrChecked())
		return false;

	UInt32& currLevel = dsm->m_SupplierLevels[a];

	bool hasSource = (a == dsm->m_SourceItem);

	if ((currLevel & SL_USAGE_MASK) < level) // Source bit is also already determined and irrelevant for the decision to search for next level.
	{
		currLevel = level; // if Source bit was set, it will be set again.

		auto ti = dynamic_cast<const TreeItem*>(a);
		if (ti)
			NotifyStateChange(ti, NC2_InterestChange);

		if (level == 1)
		{
			a->VisitSuppliers(SupplierVisitFlag::CalcAll, MakeDerivedProcVistor([dsm, &hasSource](const Actor* s) { hasSource |= MarkSources(dsm, s, 1); }));
			a->VisitSuppliers(SupplierVisitFlag::MetaAll, MakeDerivedProcVistor([dsm, &hasSource](const Actor* s) { hasSource |= MarkSources(dsm, s, 2); }));
		}
		else
		{
			dms_assert(currLevel == 2);
			a->VisitSuppliers(SupplierVisitFlag::All, MakeDerivedProcVistor([dsm, &hasSource](const Actor* s) { hasSource |= MarkSources(dsm, s, 2); }));
		}
		if (hasSource)
			currLevel |= SL_USES_SOURCE;
	}
	else
		hasSource |= ((currLevel & SL_USES_SOURCE) != 0);
	return hasSource;
}

extern "C" TIC_CALL void DMS_TreeItem_SetAnalysisTarget(const TreeItem* ti, bool mustClean)
{
	DMS_CALL_BEGIN

		DataStoreManager* dsm = DSM::Curr();
		if (mustClean)
		{
			dsm->m_SupplierLevels.clear();
			NotifyStateChange(ti, NC2_InterestChange);
		}
		if (!ti)
			return;
		MarkSources(dsm, ti, 1);

	DMS_CALL_END
}

extern "C" TIC_CALL void DMS_TreeItem_SetAnalysisSource(const TreeItem* ti)
{
	DMS_CALL_BEGIN

		DataStoreManager* dsm = DSM::Curr();
		dsm->m_SourceItem = ti;
		DMS_TreeItem_SetAnalysisTarget(ti, true); // sends a refresh at cleaning

	DMS_CALL_END
}

extern "C" TIC_CALL UInt32 DMS_TreeItem_GetSupplierLevel(const TreeItem* ti)
{
	DMS_CALL_BEGIN

	DataStoreManager* dsm = DSM::Curr();
	if (dsm) {
		auto iter = dsm->m_SupplierLevels.find(ti);
		if (iter != dsm->m_SupplierLevels.end())
			return iter->second;
	}

	DMS_CALL_END
	return 0;
}

