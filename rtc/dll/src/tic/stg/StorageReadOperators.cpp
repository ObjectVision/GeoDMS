// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// *****************************************************************************
//
// storage_read_table / storage_read_value (#587)
//
// The read of a stored item as an operator application. A table unit that is read from a storage
// gets the calculator
//
//     storage_read_table(spec, uint32, 'attr1', vu1, 'attr2', vu2, ...)
//
// and each of its stored attributes subitem(<that key>, 'attr1') through the cache-root merge
// (TreeItemMetaInfo.cpp, TreeItem_InstallStorageReadCalculator); an attribute that is not read with
// its domain gets
//
//     storage_read_attr(spec, domain, 'attr', vu, extras...)
//
// and a stored parameter
//
//     storage_read_value(spec, vu)
//
// where spec = union_data(uint2, do(ES..., storageName), storageType, sqlString, tablePath) is the
// storage spec of decision 2 of doc/development/storage-read-operators.md. Nothing in the key names
// the configured item: the operator takes it from the origin item of its DataController, which
// TreeItem::UpdateDC sets before the result is made, so that two configured items with the same key
// share one DataController and one read (identity by value, S4). The operators follow PhaseContainer:
// CreateResultCaller builds the result skeleton
// (the unit and one member per stored attribute), PreCalcUpdate collects the members that carry
// interest and are not read yet into the root's m_ReadAssets, and CalcResult reads exactly those under
// the storage manager's critical section, taken at the scheduling gate through
// GetRequiredStorageManager (#933). A member that gains interest after the read completed re-enters the
// operator for that member alone (oper_policy::members_on_demand, the #1167 re-entry).
//
// *****************************************************************************

#include "Parallel.h"       // IsMultiThreaded3, MaxConcurrentTreads
#include "RtcTypeLists.h"
#include "act/any.h"
#include "dbg/Diagnostics.h"
#include "dbg/SeverityType.h"
#include "mci/ValueComposition.h"
#include "mci/ValueWrap.h"
#include "utl/FileSystem.h" // IsFileOrDirAccessible: the mapping arm
#include "utl/Registry.h"   // IsPerformanceLogging
#include "utl/scoped_exit.h"
#include "utl/splitPath.h"  // DelimitedConcat
#include "utl/StrFormat.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataArrayValue.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "ItemLocks.h"
#include "LispTreeType.h"
#include "MoreDataControllers.h"
#include "OperationContext.h"
#include "OperGroups.h"
#include "Operator.h"
#include "ParallelTiles.h"   // serial_for
#include "PerfMeasurement.h" // the per-read performance line
#include "Projection.h"
#include "SessionData.h"
#include "TileFunctorImpl.h" // make_unique_LazyTileFunctor
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "stg/AbstrStorageManager.h"

#include <optional>
#include <semaphore>

namespace {

// *****************************************************************************
// the state of one read
// *****************************************************************************

// One member of a read: the cache item that receives the data, the configured item whose storage meta
// info says where it comes from, and the interest that keeps the member wanted between PreCalcUpdate
// and CalcResult.
struct storage_read_member
{
	SharedTreeItemInterestPtr m_Keep;
	TreeItem*                 m_CacheItem = nullptr; // owned by the result root
	SharedTreeItem            m_ConfigItem;
	StorageMetaInfoPtr        m_MetaInfo;
};

// Kept in the result root's m_ReadAssets from PreCalcUpdate to the end of CalcResult, as PhaseContainer
// keeps its phase_resource; TSF_ReadAssetsInterestScoped releases it when the read is abandoned.
struct storage_read_request
{
	SharedTreeItem                       m_ConfigItem; // the configured table unit, attribute or parameter
	SharedTreeItem                       m_Holder;     // its storage holder
	SharedPtr<AbstrStorageManager>       m_SM;
	SharedPtr<NonmappableStorageManager> m_NSM;        // m_SM when it reads through StorageReadHandle; null for a memory-mapped store, whose members are mapped
	std::vector<storage_read_member>     m_Pending;    // in read order: the root first when it is to be read

	bool IsPending(const TreeItem* cacheItem) const
	{
		for (auto& m : m_Pending)
			if (m.m_CacheItem == cacheItem)
				return true;
		return false;
	}
};

SharedStr ArgString(const ArgRefs& args, arg_index i)
{
	return GetTheCurrValue<SharedStr>(GetItem(args[i]));
}

// The configured item whose read this is: the origin item of the DataController. TreeItem::UpdateDC
// hands it over before the result is made, and TreeItem_InstallStorageReadCalculator makes sure of
// it. Two configured items with the same key share the DataController and the first one's storage
// serves both; the key says everything the read depends on (identity by value, S4).
SharedTreeItem ResolveConfigItem(const TreeItemDualRef& resultHolder)
{
	auto item = resultHolder.GetOriginItem();
	if (!item)
		throwDmsErrF("storage read: the configured item of the result {} is not known", resultHolder.GetItemNameStr());
	if (!item->GetStorageParent(false))
		item->throwItemError("storage read: the item has no storage to read from");
	return item;
}

// #1259: put a storage's own answer about its read volume into an estimate. See the long note above
// ApplyStorageReadBytes below, which resolves the three arguments from a result holder; this is the
// core, so that the read's performance line and the operator's estimate cannot disagree -- and they
// did, visibly: the same 346 MB read reported 1.00x against the operator estimate and 51131x against
// the report's own, because only one of the two consulted the storage.
void ApplyStorageReadBytes(PerformanceEstimationData& result, const AbstrStorageManager* sm
	, const TreeItem* storageHolder, const TreeItem* configItem)
{
	if (!sm || !storageHolder || !configItem)
		return;
	SizeT bytes = 0;
	try { bytes = sm->EstimateReadBytes(storageHolder, configItem); }
	catch (...) { return; } // a storage that cannot look: keep the assumed figure, never fail an estimate
	if (!bytes)
		return;

	result.resultingMemory = bytes;
	result.resultingMemoryUpperBound = Max<SizeT>(bytes, result.resultingMemoryUpperBound);
	result.ioBytes = bytes;              // the volume that crosses the storage boundary
	result.residentMemory = bytes;       // eager: the read writes into the result array and keeps it
	if (result.nrChores <= 1)
		result.choreMemory = bytes;
}

storage_read_request& GetOrCreateRequest(TreeItem* root, const TreeItemDualRef& resultHolder)
{
	root->UpdateMetaInfo(); // a passor: marks it MetaInfo-ready, which the DataWriteLock of a member asserts on a worker thread
	if (!root->m_ReadAssets.has_value())
	{
		root->m_ReadAssets.emplace<storage_read_request>();
		root->SetTSF(TSF_ReadAssetsInterestScoped); // abandoned before CalcResult completes: StopInterest releases the request and the interest it holds
	}
	MG_CHECK(root->m_ReadAssets.is_a<storage_read_request>());
	auto& req = root->m_ReadAssets.Get<storage_read_request>();
	if (!req.m_SM)
	{
		req.m_ConfigItem = ResolveConfigItem(resultHolder);
		req.m_Holder = req.m_ConfigItem->GetStorageParent(false);
		MG_CHECK(req.m_Holder);
		auto sm = req.m_Holder->GetStorageManager();
		MG_CHECK(sm);
		req.m_SM = sm;
		req.m_NSM = dynamic_cast<NonmappableStorageManager*>(sm);
	}
	return req;
}

// Collect cacheItem when it is wanted and not read yet: the storage meta info is that of its configured
// counterpart, with the read redirected to the cache item.
void CollectMember(storage_read_request& req, TreeItem* cacheItem, const TreeItem* configItem)
{
	auto keep = cacheItem->GetInterestPtrOrNull();
	if (!keep)
		return; // nobody asked for it: not read (the point of #587)
	if (IsDataReady(cacheItem) || req.IsPending(cacheItem))
		return;
	MG_CHECK(configItem);
	StorageMetaInfoPtr smi;
	if (req.m_NSM) // a memory-mapped store needs none: MapMember names the file by the described item
	{
		smi = req.m_NSM->GetMetaInfo(req.m_Holder.get(), const_cast<TreeItem*>(configItem), StorageAction::read);
		MG_CHECK(smi);
		smi->SetDataTarget(cacheItem);
	}
	req.m_Pending.emplace_back(storage_read_member{ std::move(keep), cacheItem, make_shared_tree(configItem, existing_obj{}), std::move(smi) });
}

// the configured counterpart of a member of the result root: the same relative path, through raw links
const TreeItem* FindConfigMember(const TreeItem* configRoot, const TreeItem* cacheRoot, const TreeItem* cacheItem)
{
	auto relName = cacheItem->GetRelativeName(cacheRoot);
	auto path = relName.AsRange();
	auto curr = configRoot;
	while (curr && !path.empty())
	{
		auto sep = std::find(path.first, path.second, '/');
		TokenID id = GetTokenID_mt(path.first, sep);
		const TreeItem* found = nullptr;
		for (auto sub = curr->_GetFirstSubItem(); sub; sub = sub->GetNextItem())
			if (sub->GetNameID() == id)
			{
				found = sub;
				break;
			}
		curr = found;
		path.first = (sep == path.second) ? sep : sep + 1;
	}
	return curr;
}

// *****************************************************************************
// the read of one data item: what AbstrDataItem::DoReadItem did for the configured item before #587
// *****************************************************************************

// Per-thread reader clones for the tiles of one attribute: a storage manager is opened once per
// clone, and MaxConcurrentTreads clones serve the tile functor's concurrent demands.
using semaphore_t = std::counting_semaphore<>;
struct reader_clone_farm
{
	semaphore_t m_Countdown;
	std::vector<std::unique_ptr<StorageReadHandle>> m_ClonePtrs;
	std::mutex m_CloneCS;
	std::vector<UInt32> m_Tokens;

	reader_clone_farm()
		: m_Countdown(MaxConcurrentTreads())
	{
		auto nrThreads = MaxConcurrentTreads();
		m_ClonePtrs.resize(nrThreads);
		m_Tokens.reserve(nrThreads);
		while (nrThreads)
			m_Tokens.emplace_back(--nrThreads);
	}

	UInt32 acquire()
	{
		m_Countdown.acquire();
		std::lock_guard csLock(m_CloneCS);
		auto token = m_Tokens.back();
		m_Tokens.pop_back();
		return token;
	}
	void release(UInt32 token)
	{
		{
			std::lock_guard csLock(m_CloneCS);
			m_Tokens.emplace_back(token);
		}
		m_Countdown.release();
	}
};

// Read the attribute the meta info describes into target: every tile through the manager's
// ReadDataItem, either at once into a write lock or, for a random-access storage and a tiled
// domain, on demand through a lazy tile functor that re-reads a tile whenever it is asked for
// (doc/tile-data-retainment.md 4.3). The section of the manager is held by the caller.
bool ReadDataItemInto(NonmappableStorageManager* sm_, StorageMetaInfoPtr smi, AbstrDataItem* target)
{
	assert(CheckCalculatingOrReady(target->GetAbstrDomainUnit()->GetCurrRangeItem().get()));

	auto sm = MakeSharedFromBorrowedObjectPtr(sm_);
	MG_CHECK(sm);
	assert(sm->IsOpen());
	assert(!sm->m_CriticalSection.try_acquire());

	if (!sm->DoesExist(smi->StorageHolder()))
		target->throwItemErrorF("Storage {} does not exist", sm->GetNameStr().c_str());

	try {
		auto adu = target->GetAbstrDomainUnit();
		assert(adu);

		if (adu->GetNrDimensions() == 2)
		{
			sm->DoCheckFactorSimilarity(smi);
			sm->DoCheck50PercentExtentOverlap(smi);
		}

		auto tn = adu->GetNrTiles();
		if (IsMultiThreaded3() && tn > 1 && sm->AllowRandomTileAccess())
		{
			auto readerFarm = std::make_shared<reader_clone_farm>();

			auto tileGenerator = [target, sm, smi, readerFarm](AbstrDataObject* self, tile_id t)
			{
				auto context = TreeItemContextHandle(target, "storage read");
				auto token = readerFarm->acquire();
				auto returnTokenOnExit = make_scoped_exit([&readerFarm, token]() { readerFarm->release(token); });

				auto& readerClonePtr = readerFarm->m_ClonePtrs[token];
				if (!readerClonePtr)
					readerClonePtr = sm->ReaderClone(smi);
				if (auto r = readerClonePtr->StorageManager()->ReadDataItem(smi, self, t); !r)
					r.Throw("Failure during Reading from storage");
			};
			auto rangeDomainUnit = AsUnit(adu->GetCurrRangeItem()); assert(rangeDomainUnit);
			auto tileRangeData = rangeDomainUnit->GetTiledRangeData();
			auto rangeValuesUnit = AsUnit(target->GetAbstrValuesUnit()->GetCurrRangeItem()); assert(rangeValuesUnit);
			MG_CHECK(tileRangeData);
			visit<typelists::numerics>(rangeValuesUnit.get(), [target, tileRangeData, &tileGenerator]<typename V>(const Unit<V>* valuesUnit) {
				target->m_DataObject = make_unique_LazyTileFunctor<V>(make_shared_tree(target, existing_obj{}), tileRangeData.get(), valuesUnit->m_RangeDataPtr, std::move(tileGenerator)
					MG_DEBUG_ALLOCATOR_SRC(target->md_FullName + ".storage read: lazy tiles of a random-access storage")
				).release();
			});
		}
		else
		{
			// mustzero: the storage managers that fill this buffer (Shp/dbf/Odbc/Xdb) ask for
			// write_only_mustzero on it, so a short or partial read leaves zeros rather than
			// indeterminate memory. That mode has to be given HERE -- an untiled result allocates in
			// the DataWriteLock ctor, so the mode passed to GetDataWrite() cannot zero anything.
			DataWriteLock readResultHolder(target, dms_rw_mode::write_only_mustzero);
			MG_CHECK(readResultHolder.get_ptr());
			serial_for<tile_id>(0, adu->GetNrTiles(),
				[sm, smi, &readResultHolder](tile_id t)->void
				{
					auto r = sm->ReadDataItem(smi, readResultHolder.get_ptr(), t);
					if (!r)
						r.Throw("Failure during Reading from storage");
				}
			);
			readResultHolder.Commit();
		}
	}
	catch (const DmsException& x)
	{
		if (!target->WasFailed(FailType::Data))
			target->DoFailCaller(x.AsErrMsg(), FailType::Data);
		throw;
	}
	return true;
}

// The range of a table into its result unit.
bool ReadUnitRangeInto(NonmappableStorageManager* sm, const StorageMetaInfo& smi, AbstrUnit* target)
{
	if (!sm->ReadUnitRange(smi))
		return false;
	MG_CHECK(target->HasTiledRangeData() || target->IsDefaultUnit());
	return true;
}

// The projection and spatial reference that DoUpdateTree gave a configured grid domain from the file
// go to the result unit of its read: the storage supplies them, and the configured unit and its result
// have to unify. Meta thread: asking the configured unit updates it.
void CopyProjection(const TreeItem* configItem, AbstrUnit* target)
{
	assert(IsMetaThread());
	auto configUnit = AsDynamicUnit(configItem);
	if (!configUnit)
		return;
	if (!target->GetProjection())
		if (auto p = configUnit->GetProjection())
			target->SetProjection(SharedPtr<const UnitProjection>(p));
	if (!target->GetSpatialReference())
		if (auto sr = configUnit->GetSpatialReference())
			target->SetSpatialReference(sr);
}

// A read of one attribute or parameter needs the ranges of its domain and values units (its
// arguments) for its meta info and its data allocation: wait for them, or suspend.
bool UnitsReadyOrSuspend(const AbstrDataItem* adi)
{
	for (auto unit : { adi->GetAbstrDomainUnit(), adi->GetAbstrValuesUnit() })
		if (unit)
			if (auto rangeItem = unit->GetCurrRangeItem(); rangeItem && !WaitForReadyOrSuspendTrigger(rangeItem.get()))
				return false;
	return true;
}

// The unit arguments of a read (its domain and values units, the extras a manager adds such as a
// grid's own domain) must have their ranges before the read maps and allocates: wait, or suspend.
bool UnitArgsReadyOrSuspend(const ArgRefs& args)
{
	for (const auto& arg : args)
	{
		auto item = GetItem(arg);
		if (!IsUnit(item))
			continue;
		if (auto rangeItem = AsUnit(item)->GetCurrRangeItem(); rangeItem && !WaitForReadyOrSuspendTrigger(rangeItem.get()))
			return false;
	}
	return true;
}

// the request goes, with the interest it holds
void ReleaseRequest(TreeItem* root)
{
	if (root->m_ReadAssets.is_a<storage_read_request>())
		root->m_ReadAssets.Get<storage_read_request>().m_Pending.clear();
	root->m_ReadAssets.Clear();
	root->ClearTSF(TSF_ReadAssetsInterestScoped);
}

// The read of a member of a memory-mapped store (MMD): its file is mapped and becomes the member's data
// object, as PrepareDataUsageImpl did for the configured item before #587. No storage handle: the
// manager has no ReadDataItem; opening the store once establishes its existence and its lock file.
void MapMember(storage_read_request& req, storage_read_member& m)
{
	auto sm = req.m_SM.get();
	MG_CHECK(IsDataItem(m.m_CacheItem));
	auto cacheItem = AsDataItem(m.m_CacheItem);

	auto relName = m.m_ConfigItem->GetRelativeName(req.m_Holder.get());
	if (relName.empty())
		relName = SharedStr("@main");
	auto fileName = DelimitedConcat(sm->GetNameStr().AsRange(), relName.AsRange());
	if (!IsFileOrDirAccessible(fileName))
	{
		cacheItem->Fail("Data not found in .MMD storage folder", FailType::Data);
		return;
	}
	if (!sm->IsOpen())
	{
		// open and close under the section, as the configured item's read did: the meta info's
		// destructor closes the store, and CloseStorage requires the section to be held
		AbstrStorageManager::lock_t lock(sm->m_CriticalSection);
		if (!sm->IsOpen())
		{
			StorageMetaInfo smi(req.m_Holder.get(), m.m_ConfigItem.get());
			sm->OpenForRead(smi);
		}
	}
	auto avu = AbstrValuesUnit(cacheItem);
	auto fh = OpenFileData(cacheItem, avu ? avu->GetTiledRangeData().get() : nullptr, fileName);
	if (!fh)
	{
		cacheItem->Fail("Cannot open data in .MMD storage folder", FailType::Data);
		return;
	}
	cacheItem->m_DataObject.reset(fh.release());
}

// The attributes of one table in one pass (#587 S4): one handle opens the storage for all of them, the
// manager reads what it can at once, and what it leaves (a geometry) is read on its own while the
// storage is still open. One performance line for the pass, against the sum of the members' estimates.
void ReadMembersAtOnce(NonmappableStorageManager* sm, TreeItem* root, const std::vector<storage_read_member*>& members, const SharedStr& storageName)
{
	std::vector<NonmappableStorageManager::ReadTarget> targets;
	targets.reserve(members.size());
	for (auto m : members)
	{
		auto progressMsg = mySSPrintF("Read {} from {}", m->m_ConfigItem->GetFullName(), storageName);
		reportD(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, progressMsg.c_str());
		targets.push_back(NonmappableStorageManager::ReadTarget{ m->m_MetaInfo, AsDataItem(m->m_CacheItem), false });
	}

	bool measure = IsPerformanceLogging();
	PerformanceEstimationData estimate;
	if (measure)
		for (SizeT i = 0, n = targets.size(); i != n; ++i)
		{
			const auto& t = targets[i];
			auto e = EstimateReadResources(t.m_Item);
			// #1259: a manager that knows its per-member volume beforehand overrules the assumed
			// widths here too, so that this pass and the gate weigh the same bytes. None of the
			// at-once managers answers today; the single-member path below is where strfiles lands.
			ApplyStorageReadBytes(e, sm, t.m_MetaInfo->StorageHolder(), members[i]->m_ConfigItem.get());
			estimate.residentMemory      += e.residentMemory;
			estimate.choreMemory         += e.choreMemory;
			estimate.resultingNrElements += e.resultingNrElements;
			if (e.nrChores > estimate.nrChores)
				estimate.nrChores = e.nrChores;
		}
	PerfTimer timer(measure);

	try {
		StorageReadHandle srh(sm, StorageMetaInfoPtr(targets.front().m_MetaInfo), no_storage_lock); // opens the table's layer, for all of them
		if (!sm->IsOpen())
			throwDmsErrF("Reading from {} failed", storageName);
		sm->ReadDataItemsAtOnce(targets);
		for (auto& t : targets)
			if (!t.m_Done && !t.m_Item->WasFailed(FailType::Data))
			{
				try {
					if (!ReadDataItemInto(sm, t.m_MetaInfo, t.m_Item))
						t.m_Item->Fail(mySSPrintF("Reading from {} failed", storageName).c_str(), FailType::Data);
				}
				catch (const DmsException& x)
				{
					if (!t.m_Item->WasFailed(FailType::Data))
						t.m_Item->DoFailCaller(x.AsErrMsg(), FailType::Data);
				}
				t.m_Done = true;
			}
	}
	catch (const DmsException& x)
	{
		for (auto& t : targets) // the pass as a whole failed: every member still to be served fails with it
			if (!t.m_Done && !t.m_Item->WasFailed(FailType::Data))
				t.m_Item->DoFailCaller(x.AsErrMsg(), FailType::Data);
	}

	for (auto& t : targets)
		if (!t.m_Item->WasFailed(FailType::Data))
			PublishMeasuredElementWidth(t.m_Item);
	if (measure)
		ReportReadPerformance(root, estimate, timer.ElapsedMSec());
	for (auto m : members)
		m->m_MetaInfo.reset(); // while the section is held: a meta info closes the storage when it dies
}

// CalcResult of the operators: read the pending members, one storage handle each, under the storage
// manager's critical section, which the scheduling gate acquired when this operation named the manager
// through GetRequiredStorageManager (#933) and which is taken here otherwise. A memory-mapped store
// has no handle and no gate: its members are mapped.
bool ReadPendingMembers(TreeItem* root)
{
	MG_CHECK(root->m_ReadAssets.is_a<storage_read_request>());
	auto& req = root->m_ReadAssets.Get<storage_read_request>();
	MG_CHECK(req.m_SM);

	// The request goes with this pass, whichever way it ends: it holds interest in the members and
	// their meta info, which a failed read must not keep alive (a dangling interest keeps the keys,
	// and their string literals, alive up to the teardown of the token registry). The meta infos go
	// first, while the storage section below is still held.
	auto releaseRequest = [root]() noexcept { ReleaseRequest(root); };

	if (!req.m_NSM)
	{
		auto release = make_scoped_exit([&releaseRequest] { releaseRequest(); });
		for (auto& m : req.m_Pending)
		{
			if (m.m_CacheItem == root && req.m_Pending.size() > 1)
				root->throwItemError("storage read: a memory-mapped store maps attributes and parameters, not tables");
			auto progressMsg = mySSPrintF("Map {} from {}", m.m_ConfigItem->GetFullName(), req.m_SM->GetNameStr());
			reportD(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, progressMsg.c_str());
			MapMember(req, m);
		}
		return true;
	}

	auto sm = req.m_NSM;
	std::optional<AbstrStorageManager::lock_t> csLock;
	if (auto oc = CancelableFrame::CurrActive(); oc && oc->m_StorageLockHeld && oc->m_RequiredStorageManager.get() == sm.get())
	{
		oc->m_StorageLockHeld = false; // this frame releases it, when the last handle is gone
		csLock.emplace(sm->m_CriticalSection, adopt_storage_lock);
	}
	else
		csLock.emplace(sm->m_CriticalSection);

	auto release = make_scoped_exit([&releaseRequest] { releaseRequest(); }); // after csLock: destroyed before it, so the meta infos close the storage under the section

	auto storageName = sm->GetNameStr();

	// The table's range goes first and on its own, as does every member of a manager that reads one
	// attribute at a time; the attributes of a manager that can read them at once go in one pass over
	// the storage's records (gdal.vect), the pass leaving what it does not serve (a geometry) to a read
	// of its own with the storage still open.
	std::vector<storage_read_member*> atOnce;
	for (auto& m : req.m_Pending)
	{
		auto cacheItem = m.m_CacheItem;
		if (cacheItem != root && IsDataItem(cacheItem) && sm->CanReadDataItemsAtOnce())
		{
			atOnce.push_back(&m);
			continue;
		}
		auto progressMsg = mySSPrintF("Read {} from {}", m.m_ConfigItem->GetFullName(), storageName);
		reportD(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, progressMsg.c_str());
		try {
			StorageReadHandle srh(sm.get(), std::move(m.m_MetaInfo), no_storage_lock); // opens the storage; closes it when it dies
			bool ok = sm->IsOpen();
			if (ok)
			{
				if (IsUnit(cacheItem))
					ok = ReadUnitRangeInto(sm.get(), *srh.MetaInfo(), AsUnit(cacheItem));
				else
				{
					bool measure = IsPerformanceLogging();
					auto estimate = measure ? EstimateReadResources(cacheItem) : PerformanceEstimationData(); // the domain count is known: the table's range was read first
					if (measure)
						ApplyStorageReadBytes(estimate, sm.get(), req.m_Holder.get(), m.m_ConfigItem.get()); // #1259: the same figure the gate was given
					PerfTimer timer(measure);

					ok = ReadDataItemInto(sm.get(), srh.MetaInfo(), AsDataItem(cacheItem));

					// A variable-width attribute read from an external source is the LEAF of every estimate that
					// consumes it, and right after the read its true volume is simply known -- the tiles are
					// resident. Publish measured bytes-per-row (always on: the width feeds the admission gate),
					// so consumers stop inheriting ASSUMED_SEQ_LENGTH guesses (schedule-with-lookahead 8.1.19).
					if (ok)
						PublishMeasuredElementWidth(AsDataItem(cacheItem));
					if (measure)
						ReportReadPerformance(cacheItem, estimate, timer.ElapsedMSec());
				}
			}
			if (!ok)
				m.m_ConfigItem->throwItemErrorF("Reading from {} failed", storageName);
		}
		catch (const DmsException& x)
		{
			if (cacheItem == root)
				throw; // the table's range or the parameter itself: nothing is left to deliver
			if (!cacheItem->WasFailed(FailType::Data))
				cacheItem->DoFailCaller(x.AsErrMsg(), FailType::Data); // this member fails; the others are still read
		}
	}
	if (!atOnce.empty())
		ReadMembersAtOnce(sm.get(), root, atOnce, storageName);
	return true;
}

SharedPtr<NonmappableStorageManager> RequiredStorageManager(const TreeItemDualRef& resultHolder)
{
	auto root = resultHolder.GetNew();
	if (root && root->m_ReadAssets.is_a<storage_read_request>())
		return root->m_ReadAssets.Get<storage_read_request>().m_NSM; // null for a memory-mapped store: mapping needs no gate
	return {};
}

// The parts of a member spec: 'geometry:poly' -> 'geometry', Polygon (decision 9: no suffix for
// Single); a trailing '@' says that the member's values unit is the table itself (a relation to its
// own table), whose key cannot be an argument of the read that produces it.
struct member_spec_parts
{
	SharedStr        m_Name;
	ValueComposition m_VC = ValueComposition::Single;
	bool             m_ValuesAreTheTable = false;
};

member_spec_parts SplitMemberSpec(SharedStr memberSpec)
{
	member_spec_parts result;
	auto r = memberSpec.AsRange();
	if (!r.empty() && r.second[-1] == '@')
	{
		result.m_ValuesAreTheTable = true;
		--r.second;
	}
	auto colon = std::find(r.first, r.second, ':');
	if (colon == r.second)
	{
		result.m_Name = SharedStr(CharPtrRange(r.first, r.second));
		return result;
	}
	SharedStr suffix(CharPtrRange(colon + 1, r.second));
	result.m_VC = DetermineValueComposition(suffix.c_str());
	if (result.m_VC == ValueComposition::Unknown || result.m_VC == ValueComposition::Single)
		throwDmsErrF("storage read: unknown value composition '{}' in member spec '{}'", suffix, memberSpec);
	result.m_Name = SharedStr(CharPtrRange(r.first, colon));
	return result;
}

// #1259: what the admission gate is told a read will cost.
//
// Operator::EstimatePerformance charges EstimateDataBytes over the result's domain. For a
// fixed-width result that is exact, but for a variable-width one with no measured width yet it is
// ASSUMED_STRING_BYTES (32) plus an index entry per element -- and the first read of an item is
// precisely when no width has been measured. Logged on a strfiles fileset of 50 whole XML files:
//
//   read .../fs_1/XmlData: 20.8ms n=50 (1.00x derived) B=43.30M (22702.60x) 2085.2MB/s 1 chores
//
// 2 KB charged against 43 MB allocated. A storage that knows its volume beforehand says so through
// AbstrStorageManager::EstimateReadBytes, and this puts that answer where the gate reads it:
// resultingMemory, which is what LedgerChargeOf adds up, and ioBytes, which is what the read's own
// performance line compares against.
//
// The confidence is left as the base set it, deliberately, for the reason spelled out in
// AbstrPolygonConnectivityOperator: RefreshEstimateForAdmission installs nothing above 'declared',
// so downgrading here would discard the very figure this exists to supply.
//
// Applied to the attribute and value reads. The table read is a unit whose MEMBERS carry the data;
// its members are estimated when they are collected, and giving the table operator a total would
// need the pending list, which does not exist at schedule time. Left alone rather than guessed.
void ApplyStorageReadBytes(PerformanceEstimationData& result, const TreeItemDualRef& resultHolder)
{
	SharedTreeItem configItem;
	try { configItem = ResolveConfigItem(resultHolder); }
	catch (...) { return; } // no origin item, or no storage: the base's figures stand

	auto storageHolder = configItem->GetStorageParent(false);
	if (!storageHolder)
		return;
	auto sm = storageHolder->GetStorageManager(false);
	if (!sm)
		return;

	ApplyStorageReadBytes(result, sm, storageHolder.get(), configItem.get());
}

// *****************************************************************************
// storage_read_table(spec, domainType, [memberSpec, valuesUnit]*) -> unit with members
// *****************************************************************************

CommonOperGroup cog_storage_read_table("storage_read_table", oper_policy::allow_extra_args | oper_policy::dynamic_result_class | oper_policy::members_on_demand);

struct StorageReadTableOperator : BinaryOperator
{
	StorageReadTableOperator(AbstrOperGroup& og)
		: BinaryOperator(&og, AbstrUnit::GetStaticClass()
			, DataArray<SharedStr>::GetStaticClass() // spec
			, AbstrUnit::GetStaticClass()            // the domain's value type, as a unit
		)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;
		MG_CHECK(IsMetaThread());
		MG_CHECK(args.size() >= 2 && (args.size() - 2) % 2 == 0);

		auto domainType = AsUnit(GetItem(args[1]));
		MG_CHECK(domainType);
		auto root = domainType->GetUnitClass()->CreateResultUnit(nullptr);
		MG_CHECK(root);
		CopyProjection(ResolveConfigItem(resultHolder).get(), root.get()); // a grid domain: the file's projection, as DoUpdateTree gave it to the configured unit

		for (arg_index i = 2; i < args.size(); i += 2)
		{
			auto parts = SplitMemberSpec(ArgString(args, i));
			const AbstrUnit* vu = parts.m_ValuesAreTheTable ? root.get() : AsUnit(GetItem(args[i + 1]));
			MG_CHECK(vu);
			CreateDataItemFromPath(root.get(), parts.m_Name.c_str(), root.get(), vu, parts.m_VC);
		}
		resultHolder = SharedMutableTreeItem(root);
	}

	bool PreCalcUpdate(TreeItemDualRef& resultHolder, ArgRefs& args) const override
	{
		MG_CHECK(IsMetaThread());
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		if (!UnitArgsReadyOrSuspend(args))
			return false;
		auto releaseOnThrow = make_releasable_scoped_exit([root] { ReleaseRequest(root); }); // a request left behind holds interest
		auto& req = GetOrCreateRequest(root, resultHolder);

		CollectMember(req, root, req.m_ConfigItem.get()); // the table's range, once

		for (auto member = root->WalkConstSubTree(root); member; member = root->WalkConstSubTree(member))
		{
			if (!IsDataItem(member))
				continue;
			auto configMember = FindConfigMember(req.m_ConfigItem.get(), root, member);
			if (!configMember)
				const_cast<TreeItem*>(member)->Fail(mySSPrintF("storage_read_table: no configured counterpart of member {} under {}", member->GetRelativeName(root), req.m_ConfigItem->GetFullName()).c_str(), FailType::Data);
			else
				CollectMember(req, const_cast<TreeItem*>(member), configMember);
		}
		releaseOnThrow.release();
		return true;
	}

	auto GetRequiredStorageManager(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> SharedPtr<NonmappableStorageManager> override
	{
		return RequiredStorageManager(resultHolder);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		return ReadPendingMembers(root);
	}
};

// *****************************************************************************
// storage_read_value(spec, valuesUnit) -> parameter
// *****************************************************************************

CommonOperGroup cog_storage_read_value("storage_read_value", oper_policy::dynamic_result_class);

struct StorageReadValueOperator : BinaryOperator
{
	StorageReadValueOperator(AbstrOperGroup& og)
		: BinaryOperator(&og, AbstrDataItem::GetStaticClass()
			, DataArray<SharedStr>::GetStaticClass() // spec
			, AbstrUnit::GetStaticClass()            // the parameter's values unit
		)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;
		MG_CHECK(IsMetaThread());
		MG_CHECK(args.size() == 2);

		auto vu = AsUnit(GetItem(args[1]));
		MG_CHECK(vu);
		resultHolder = SharedMutableTreeItem(CreateCacheDataItem(Unit<Void>::GetStaticClass()->CreateDefault(), vu, ValueComposition::Single));
	}

	bool PreCalcUpdate(TreeItemDualRef& resultHolder, ArgRefs& args) const override
	{
		MG_CHECK(IsMetaThread());
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		if (!UnitsReadyOrSuspend(AsDataItem(root)) || !UnitArgsReadyOrSuspend(args))
			return false;
		auto releaseOnThrow = make_releasable_scoped_exit([root] { ReleaseRequest(root); });
		auto& req = GetOrCreateRequest(root, resultHolder);
		CollectMember(req, root, req.m_ConfigItem.get());
		releaseOnThrow.release();
		return true;
	}

	auto GetRequiredStorageManager(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> SharedPtr<NonmappableStorageManager> override
	{
		return RequiredStorageManager(resultHolder);
	}

	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = BinaryOperator::EstimatePerformance(resultHolder, args);
		ApplyStorageReadBytes(result, resultHolder);
		return result;
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		return ReadPendingMembers(root);
	}
};

// *****************************************************************************
// storage_read_attr(spec, domain, memberSpec, valuesUnit, extras...) -> attribute
//
// One stored attribute over a domain that is not read with it: a grid, a stream item (FSS, cfs), a
// strfiles attribute, an attribute of a memory-mapped store. The extras are further arguments the
// storage manager wants calculated before the read (strfiles: its FileName attribute).
// *****************************************************************************

CommonOperGroup cog_storage_read_attr("storage_read_attr", oper_policy::allow_extra_args | oper_policy::dynamic_result_class);

struct StorageReadAttrOperator : QuaternaryOperator
{
	StorageReadAttrOperator(AbstrOperGroup& og)
		: QuaternaryOperator(&og, AbstrDataItem::GetStaticClass()
			, DataArray<SharedStr>::GetStaticClass() // spec
			, AbstrUnit::GetStaticClass()            // the attribute's domain
			, DataArray<SharedStr>::GetStaticClass() // 'name[:composition]'
			, AbstrUnit::GetStaticClass()            // its values unit
		)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;
		MG_CHECK(IsMetaThread());
		MG_CHECK(args.size() >= 4);

		auto domain = AsUnit(GetItem(args[1]));
		MG_CHECK(domain);
		auto parts = SplitMemberSpec(ArgString(args, 2));
		auto vu = AsUnit(GetItem(args[3]));
		MG_CHECK(vu);
		resultHolder = SharedMutableTreeItem(CreateCacheDataItem(domain, vu, parts.m_VC));
	}

	bool PreCalcUpdate(TreeItemDualRef& resultHolder, ArgRefs& args) const override
	{
		MG_CHECK(IsMetaThread());
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		if (!UnitsReadyOrSuspend(AsDataItem(root)) || !UnitArgsReadyOrSuspend(args))
			return false;
		auto releaseOnThrow = make_releasable_scoped_exit([root] { ReleaseRequest(root); });
		auto& req = GetOrCreateRequest(root, resultHolder);
		CollectMember(req, root, req.m_ConfigItem.get());
		releaseOnThrow.release();
		return true;
	}

	auto GetRequiredStorageManager(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> SharedPtr<NonmappableStorageManager> override
	{
		return RequiredStorageManager(resultHolder);
	}

	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = QuaternaryOperator::EstimatePerformance(resultHolder, args);
		ApplyStorageReadBytes(result, resultHolder);
		return result;
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		return ReadPendingMembers(root);
	}
};

// *****************************************************************************
// instantiation
// *****************************************************************************

StorageReadTableOperator sro_table(cog_storage_read_table);
StorageReadValueOperator sro_value(cog_storage_read_value);
StorageReadAttrOperator  sro_attr (cog_storage_read_attr);

} // anonymous namespace
