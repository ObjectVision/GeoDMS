// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"
#include "LockLevels.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// TreeItem data usage: reading an item from and writing it to its storage, the PrepareData
// state machine that brings an item's data into a usable state, committing data changes,
// and releasing memory again.

#include "TreeItem.h"
#include "TreeItemFunctionSpec.h"
//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcInterface.h"
#include "mci/ValueClass.h"
#include "mci/ValueComposition.h"
#include "act/ActorLock.h"
#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"

bool LedgerHasRoomForDeferral(); // OperationContext.cpp, same module (#1259)
bool Mmd_QualifiesAsRuleOnly(const TreeItem* storageHolder, const TreeItem* item); // stg/MemoryMappedDataStorageManager.cpp, same module (#1264)
bool IsInsideInlineOperation(); // idem
void StartOperationContexts(); // idem: hands what was just scheduled to the worker pool
void LedgerNoteDeferral(const TreeItem* item);
UInt32 LedgerDeferredCommits(); // idem
void LedgerNoteReady(const TreeItem* item);
#include "act/UpdateMark.h"
#include "act/Waiter.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "mci/PropDef.h"
#include "stg/AbstrStorageManager.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"
#include "utl/scoped_exit.h"
#include "utl/SourceLocation.h"
#include "xct/DmsException.h"

#include "LispList.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLockContainers.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataController.h"
#include "DataLocks.h"
#include "LispTreeType.h"
#include "OperationContext.h"
#include "OperGroups.h"
#include "PropFuncs.h"
#include "SessionData.h"
#include "SupplCache.h"
#include "StateChangeNotification.h"
#include "TreeItemClass.h"
#include "TreeItemSet.h"
#include "TreeItemUtils.h"
#include "TicInterface.h"
#include "TicPropDefConst.h"
#include "TreeItemProps.h"
#include "TreeItemContextHandle.h"
#include "UsingCache.h"
#include "stg/MemoryMappedDataStorageManager.h"

#include "cs_lock_map.h"

#include <unordered_set>

// raw identity key (transient, non-owning), like actor_section_lock_map / data_flags_lock_map
using treeitem_lock_map = cs_lock_map<const TreeItem*>;

//----------------------------------------------------------------------
// Read / Write Data
//----------------------------------------------------------------------


bool TreeItem::DoWriteItem(StorageMetaInfoPtr&&) const
{
	// can return false because of suspension or failure
	// caller must check state and suspend trigger to find out
	if (HasCalculator())
	{
		auto apr = GetCalculator();
		if (!apr)
		{
			assert(IsUnit(this));
			return true;
		}
		auto result = CalledCalcHandle(apr.get(), GetDynamicObjClass());
		if (!result)
		{
			dms_assert(SuspendTrigger::DidSuspend() || WasFailed());
			return false;
		}
		if (result->IsFailed())
		{
			Fail(result.get_ptr());
			return false;
		}
		if (!result->GetOld())
		{
			dms_assert(SuspendTrigger::DidSuspend());
			return false;
		}
	}
	return true;
}

//=============================== ConcurrentMap (client is responsible for scoping and stack unwinding issues)


treeitem_lock_map sg_PrepareDataUsageLockMap("PrepareDataUsage", ord_level_type::PrepareDataUsageLock);

bool TreeItem::PrepareDataUsage(DrlType drlFlags) const 
// returns false when 
//	- failed without data or 
//	- suspendend or 
//	- no calcrule etc and not a dataitem
//	doesn't suspend when drlType == DrlType::Certain, 
//	but can still fail, thus IsFailed() == true and return false
{
	DMS_ENTERS_ITEM(ord_level_type::PrepareDataUsageLock, dms_exclusive_v);
	dms_assert(m_State.GetProgress() >= ProgressState::MetaInfo || IsPassor() || WasFailed(FailType::Data));
	if (UpdateMarker::PrepareDataInvalidatorLock::IsLocked())
		drlFlags = DrlType(UInt32(drlFlags) & ~UInt32(DrlType::UpdateMask));

	dms_assert(!IsTemplate()); // formation of FuncDC's should prevent args to be calculated that fail to meet this precondition
	dms_assert(IsMetaThread() || !(UInt32(drlFlags) & UInt32(DrlType::UpdateMask)));

	if ((UInt32(drlFlags) & UInt32(DrlType::Certain)) && !SuspendTrigger::BlockerBase::IsBlocked())
	{
		SuspendTrigger::FencedBlocker lockSuspend("@TreeItem::PrepareDataUsage");
		auto result = PrepareDataUsageImpl(drlFlags);
		dms_assert(result || WasFailed());
		return result;
	}

	dms_assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides SuspendTrigger::GetLastResult
	auto result = PrepareDataUsageImpl(drlFlags);
	dms_assert(result || SuspendTrigger::DidSuspend() || WasFailed());
	return result;
}

enum class how_to_proceed { nothing, data_ready, failed, suspended, suspended_or_failed}; // return_suspended_or_failed, return_OK ;

static how_to_proceed PrepareDataCalc(std::shared_ptr<const TreeItem> self, const TreeItem* refItem, DrlType drlFlags)
{
	DMS_ENTERS_ITEM(ord_level_type::PrepareDataUsageLock, dms_exclusive_v);
	dms_assert(!SuspendTrigger::DidSuspend() && !self->WasFailed(FailType::Determine)); // Postcondition when CreateResultingTreeItem returns a result

//				FutureData dc = GetDC(GetCalculator());
//	self->UpdateDC();
	FutureData dc = self->GetCheckedDC();
	dms_check(self->HasInterest());

	if (dc)
	{
		auto dc2 = dc->CallCalcResult();
		if (SuspendTrigger::DidSuspend())
			return how_to_proceed::suspended;

		dms_assert(dc2 || SuspendTrigger::DidSuspend() || dc->WasFailed(FailType::Data));
		if (dc->WasFailed()) //  && !WasFailed())
		{
			self->StopSupplInterest();
			self->Fail(dc.get_ptr());
		}
		if (self->WasFailed(FailType::Data))
			return how_to_proceed::failed;
		if (!dc->GetOld())
		{
			dms_assert(SuspendTrigger::DidSuspend()); // Postcondition when CreateResultingTreeItem returns no result, yet hasn't failed
			return how_to_proceed::suspended;
		}
		if (SuspendTrigger::DidSuspend())
			return how_to_proceed::suspended;
		self->StopSupplInterest();
		dms_assert(dc2);
	}
	else
	{
		if (!refItem->IsCacheItem())
		{
			bool res = refItem->PrepareDataUsage(DrlType::Certain);
			if (refItem->IsFailed())
				self->Fail(refItem);
			if (!res)
				return how_to_proceed::suspended_or_failed;
		}
	}
	dms_assert(!SuspendTrigger::DidSuspend()); // Postcondition when CreateResultingTreeItem returns a result

	if (dc && dc->GetOld() != self.get() && !dc->GetOld()->IsCacheItem()) // could be config item that can be read from external source
	{
		bool res = dc->GetOld()->PrepareDataUsageImpl(drlFlags);
		if (dc->GetOld()->IsFailed())
			self->Fail(dc->GetOld());

		if (!res)
			return how_to_proceed::suspended_or_failed;
	}
	dms_assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides MustSuspend
	if (CheckCalculatingOrReady(refItem))
	{
		dms_assert(!self->WasFailed(FailType::Data));
		return how_to_proceed::data_ready;
	}
	dms_assert(!CheckCalculatingOrReady(refItem)); // PrepareDataUsage loads from cache if possible
	return how_to_proceed::nothing;
}


bool TreeItem::PrepareDataUsageImpl(DrlType drlFlags) const
// returns false when 
//	- failed without data or 
//	- suspendend or 
//	- no calcrule etc and not a dataitem
//	doesn't suspend when drlType == DrlType::Certain, 
//	but can still fail, thus IsFailed() == true and return false
{
	DMS_ENTERS_ITEM(ord_level_type::PrepareDataUsageLock, dms_exclusive_v);
	UpdateMetaInfo();
	bool throwOnFail = UInt32(drlFlags) &  UInt32(DrlType::ThrowOnFail);

	DrlType drlType = DrlType(UInt32(drlFlags) & UInt32(DrlType::UpdateMask));
	dms_assert(drlType <= DrlType::Certain);

	dms_assert(!IsTemplate()); // formation of FuncDC's should prevent args to be calculated that fail to meet this precondition

	assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides SuspendTrigger::GetLastResult

	assert(!(UInt32(drlType) &  UInt32(DrlType::Certain)) || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility

	// Checks State against suppliers if any changes occured after m_LastCheckedTS and Invalidates if any changes occured in any supplier

	UpdateMarker::ChangeSourceLock changeStamp(this, "PrepareDataUsage");

	assert(IsPassor() || HasConfigData() || (m_State.GetProgress()>=ProgressState::MetaInfo) || WasFailed(FailType::MetaInfo)); // reset by DetermineState when supplier was invalidated

	const TreeItem* refItem = nullptr;

	treeitem_lock_map::ScopedTryLock localPreparedataLock(MG_SOURCE_INFO_CODE("TreeItem::PrepareDataUsageImpl ScopedTry") sg_PrepareDataUsageLockMap, this);
	if (!localPreparedataLock)
	{
		if (!WaitForReadyOrSuspendTrigger(this))
			goto suspended_or_failed;

		assert(!SuspendTrigger::DidSuspend());
		assert(!WasFailed(FailType::Data));
		assert(!IsDataItem(this) || HasConfigData() || CheckCalculatingOrReady(GetCurrUltimateItem().get()));
		goto data_ready;
	}

	if (m_State.IsDataFailed())  // may have been arranged in an alternative thread.
		goto failed_norefitem;
	refItem = GetCurrUltimateItem().get();

	assert(refItem->IsPassor() || HasConfigData() || refItem->m_State.GetProgress() >= ProgressState::MetaInfo || refItem->WasFailed(FailType::MetaInfo));
	assert(GetInterestCount() || !IsDataItem(this)); // interest consistency

	if (CheckCalculatingOrReady(refItem)) // quick route first
		goto data_ready; // may have been arranged in an alternative thread.
	if (IsDataItem(this))
	{
		auto avu = AbstrValuesUnit( AsDataItem(this) );
		if (avu && !avu->IsCacheItem())
		{
			if (!avu->PrepareDataUsage(drlFlags))
			{
				if (!SuspendTrigger::DidSuspend())
					Fail(avu);
				return false;
			}
		}
		if (!IsCacheItem() && !IsReadFromStorage()) // an item read from an MMD store is mapped by storage_read_attr (#587), below through PrepareDataCalc
		{
			if (auto sp = GetCurrStorageParent(false))
			{
				auto sm = sp->GetStorageManager();
				assert(sm);
				if (auto mmd = dynamic_cast<MmdStorageManager*>(sm))
				{
					// the write side of an MMD store: an item calculated into it opens the store for
					// writing before its data is produced, which the DataWriteLock then maps into the
					// store's file (write-through); a data block is neither read nor written
					bool mustWrite = HasConfiguredCalcRule() && !GetCalculator()->IsDataBlock();
					// #1264: decide HERE, before this item is produced, whether the dictionary
					// carries its rule instead of its bytes. DataWriteLock reads TSF_MmdRuleOnly
					// when it maps the produced array, and that production starts below, so the
					// flag has to exist by now. Decided once per item: the walk it needs is meta
					// work, and the answer cannot change while the configuration does not.
					if (mustWrite && !IsMmdRuleOnly() && Mmd_QualifiesAsRuleOnly(sp.get(), this))
						SetTSF(TSF_MmdRuleOnly);
					if (mustWrite && !mmd->IsOpenForWrite())
						if (auto parent = GetStorageParent(true))
						{
							auto lock = AbstrStorageManager::lock_t(mmd->m_CriticalSection);
							auto smi = StorageMetaInfo(parent.get(), this);
							mmd->OpenForWrite(smi);
						}
				}
			}
		}
	}
	assert(!SuspendTrigger::DidSuspend());

	try {
		while (true)
		{
			if (CheckCalculatingOrReady(refItem))
				goto data_ready_or_in_cache;
			//		_ProcessConfigData(true, false);
			//		Now try to actually get valid data
			if (refItem != this && refItem->IsFailed())
				Fail(refItem);
			if (WasFailed(FailType::Data))
				goto failed;

			if ((drlType != DrlType::UpdateNever) && HasCalculator())
				switch (PrepareDataCalc(make_shared_tree(this, existing_obj{}), refItem, drlFlags))
				{
				case how_to_proceed::nothing: break;
				case how_to_proceed::data_ready: goto data_ready;
				case how_to_proceed::failed: goto failed;
				case how_to_proceed::suspended:
					assert(SuspendTrigger::DidSuspend());
					goto suspended;
				case how_to_proceed::suspended_or_failed: goto suspended_or_failed;
				default: MG_CHECK2(false, "unexpected how_to_proceed"); // an assert here was __assume(false) in Release
				}

			dms_assert(!SuspendTrigger::DidSuspend());

			// these two checks solve for_each(xx[SubItem(Combine(...), 'Nr_1')])
			if (SuspendTrigger::DidSuspend()) goto suspended;
			if (WasFailed(FailType::Data))           goto failed;

			if (refItem->IsCacheItem() || HasCalculator())
			{
				if (drlType == DrlType::UpdateNever)
					return false;

				if (IsUnit(refItem))
					goto nodata;
			}
			else
			{
				if (IsUnit(refItem))
				{
					AsUnit(const_cast<TreeItem*>(refItem))->SetMaxRange();
					goto data_ready; // assume default range
				}
			}
			dms_assert(!IsUnit(refItem));
			if (IsDataItem(refItem))
				goto nodata;

			refItem->SetIsInstantiated();
			goto data_ready;
			//*/

		data_ready_or_in_cache:
			if (IsCalculatingOrReady(refItem))
				goto data_ready; // may have been arranged in an alternative thread.

			ItemReadLock lock(refItem);
			if (IsDataReady(refItem))
				goto data_ready;

		}
	}
	catch (...)
	{
		// TODO: add Actor::Fail(const DmsException&) to Actor.h and use it here.
		auto err = catchException(true);
		DoFailCaller(err, FailType::Data);
		goto failed;
	}

data_ready:
	assert(!SuspendTrigger::DidSuspend());
	assert(!IsDataItem(this) || HasConfigData() || CheckCalculatingOrReady(refItem) || WasFailed(FailType::Data));
	return SuspendTrigger::BlockerBase::IsBlocked() 
		|| IsPassor() 
		|| (m_State.GetTransState() >= actor_flag_set::AF_ValidatingAndCommitting) 
		|| SuspendibleUpdate() 
		|| !SuspendTrigger::DidSuspend();

suspended_or_failed:
	assert(drlType != DrlType::Certain || !SuspendTrigger::DidSuspend());
	assert(SuspendTrigger::DidSuspend() || WasFailed()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode
	if (SuspendTrigger::DidSuspend())
		goto suspended;

failed:
	assert(WasFailed());
	assert(!SuspendTrigger::DidSuspend());
	if (refItem && IsCalculatingOrReady(refItem))
		return true;

failed_norefitem:
	assert(WasFailed());
	if (throwOnFail)
		ThrowFail();
	return false;

suspended:
	assert(drlType != DrlType::Certain);
	assert(SuspendTrigger::DidSuspend());
	return false;

nodata:
	Fail("No calculation rule or storage manager was specified and no specific primary data was provided", FailType::Data);
	goto failed;
}


bool TreeItem::PrepareData() const
{
	DMS_ENTERS_ITEM(ord_level_type::PrepareDataUsageLock, dms_exclusive_v);
	assert(IsMetaThread());

	if (!PrepareDataUsage(DrlType::Suspendible))
		return false;
	auto ultItem = GetCurrUltimateItem();
	if (!WaitForReadyOrSuspendTrigger(ultItem.get()))
	{
		if (SuspendTrigger::DidSuspend())
			return false;
		assert(ultItem->WasFailed());
		if (ultItem.get() != this && ultItem->WasFailed())
			this->Fail(ultItem.get());
		return false;
	}
	return true;
}


// called in idle time for items that will soon be visible, returns false when Suspended, true when Failed
bool TreeItem::TryPrepareDataUsage() const
{
	DMS_ENTERS_ITEM(ord_level_type::PrepareDataUsageLock, dms_exclusive_v);
	if (!GetInterestCount())
		return true;
	try { 
		return PrepareDataUsage(DrlType::Suspendible) || WasFailed();
	}
	catch (const DmsException&)
	{
		return true;
	}
}

TIC_CALL void TreeItem::DisableStorage(bool disabledStorage) // does not call UpdateMetaInfo
{
	AssignTSF(TSF_DisabledStorage, disabledStorage);
	if (m_StorageManager && disabledStorage)
	{
		m_StorageManager->DoNotCommitOnClose();
		m_StorageManager = nullptr;
	}
}

[[maybe_unused]] static bool HasCfsStorage(const TreeItem* obj)
{
	auto storageHolder = obj->GetStorageParent(false);

	return storageHolder && !stricmp(storageHolder->GetStorageManager()->GetClsName().c_str(), "cfs");
}

bool TreeItem::HasConfigData() const
{
	if (IsCacheItem())
		return false;
	if (!IsDataItem(this) && !IsUnit(this))
		return false;

	if (GetCalculatorMember())  // DC_Ptr: false
		return GetCalculatorMember()->IsDataBlock(); // DC_Ptr: false, ExprCalculator: false;
	if (!GetExprMember().empty())
		return false;
	if (GetStorageParent(false) != nullptr)
		return false;
	if (IsUnit(this))
		return AsUnit(this)->HasTiledRangeData();
	if (IsDataItem(this))
		return AsDataItem(this)->m_DataObject != nullptr;
	return false;
}

bool TreeItem::HasCurrConfigData() const
{
	if (IsCacheItem())
		return false;
	if (!IsDataItem(this) && !IsUnit(this))
		return false;

	if (GetCalculatorMember())  // DC_Ptr: false
		return GetCalculatorMember()->IsDataBlock(); // DC_Ptr: false, ExprCalculator: false;
	if (!GetExprMember().empty())
		return false;
	if (GetCurrStorageParent(false) != nullptr)
		return false;
	if (IsUnit(this))
		return AsUnit(this)->HasTiledRangeData();
	if (IsDataItem(this))
		return AsDataItem(this)->m_DataObject != nullptr;
	return false;
}

template <typename FailReasonFunc>
bool FinalizeFailure(const TreeItem* self, FailReasonFunc&& func)
{
	if (SuspendTrigger::DidSuspend())
	{
		if (self->GetInterestCount() < 2)
			ReportSuspension();
	}
	else
	{
		if (self->GetCurrRangeItem()->WasFailed(FailType::Committed))
			self->Fail(self->GetCurrRangeItem().get());

		if (!self->WasFailed(FailType::Committed))
			self->Fail(func(), FailType::Committed);
		assert(SuspendTrigger::DidSuspend() || self->WasFailed(FailType::Committed));
	}
	return false; // suspended or failed, try again later
}

bool TreeItem::CommitDataChanges() const
{
	DMS_ENTERS_ITEM(ord_level_type::PrepareDataUsageLock, dms_exclusive_v);
	assert(IsMetaThread());

	assert(m_State.GetProgress() >= ProgressState::MetaInfo);
	if (m_State.GetProgress() >= ProgressState::Committed)
		return true;
	if (!IsStorable())
		return true;
	if (IsReadFromStorage()) // #587: an item read from a storage is never written back to it, whatever its calculator says
		return true;
	if (IsFailed())
		return false;

	auto storageHolder = GetStorageParent(true);
	assert(storageHolder); // guaranteed by IsStorable();

	bool hasCalculator = HasCalculator();
	if (!hasCalculator)
		return true;

	if (GetCurrRangeItem()->WasFailed(FailType::Committed))
	{
		Fail(GetCurrRangeItem().get());
		return false;
	}

	DBG_START("TreeItem", "CommitDataChanges", false);
	DBG_TRACE(("self = {}", GetSourceName().c_str()));

	auto interestHolder = GetInterestPtrOrNull();
	assert(interestHolder); // Commit is Called from DoUpdate

	auto sm = storageHolder->GetStorageManager();
	assert(sm); // guaranteed by IsStorable();

	if ((!IsCalculatingOrReady(GetCurrRangeItem().get()) && !PrepareDataUsage(DrlType::Suspendible)) || GetCurrRangeItem()->WasFailed(FailType::Committed))
		// can have failed just because PrepareDataUsage suspended or failed; 
		return FinalizeFailure(this, [this]() { return mySSPrintF("Unable to start calculating data when trying to store it in {}", DMS_TreeItem_GetAssociatedFilename(this)); });

	// #1259 The producer is in flight. Waiting for it here, on the meta thread and inside the
	// supplier walk, is what serialised every stored item of a run: nothing after this item was
	// scheduled until it was written. Inside a DeferScope the commit is deferred instead: this
	// item stays below Committed, the walk goes on to schedule the next supplier's producer, and
	// the update loop comes back for the write once the data is ready.
	// Only while a producer holds the range item's write lock: that is the one state in which a
	// retry can find the data ready without this thread's help. A range item that is neither
	// ready nor being produced (its producer is done and a sub-item's data was released since,
	// or it never had a producer) is waited for below as before #1259, which loads what the
	// write needs or fails with the message it always gave. And only while the budget has room
	// or other commits are in flight: without room the deferral stops the walk at this item,
	// which is then the one producer started beyond the budget, and the retries drain what is
	// in flight with the meta thread free to commit; not starting it starved the walk instead
	// (the ready items behind it were never reached). With nothing in flight the wait below is
	// the pre-#1259 path, which /SB1 forces for every commit. Never inside an operation that
	// runs inline on this thread: that run expects the data.
	if (SuspendTrigger::DeferScope::IsAllowed() && !IsInsideInlineOperation() && (LedgerHasRoomForDeferral() || LedgerDeferredCommits())
		&& IsCalculating(GetCurrRangeItem().get()) && !IsDataReady(GetCurrRangeItem().get()) && !GetCurrRangeItem()->WasFailed())
	{
		// the producer runs or is queued: waiting for it here would only idle the meta thread;
		// its memory is counted as in flight until ready. The pool is told now, not when the
		// pass ends: a pass over a large graph takes seconds, and the stack of the hung Hestia
		// run showed every worker parked while the walk held the scheduled producers.
		LedgerNoteDeferral(this);
		SuspendTrigger::DeferScope::Register();
		StartOperationContexts();
		return false; // deferred, not failed
	}
	LedgerNoteReady(this); // ready, or about to be waited for: no longer in flight

	if (!WaitForReadyOrSuspendTrigger(GetCurrRangeItem().get()) || GetCurrRangeItem()->WasFailed(FailType::Committed))
		return FinalizeFailure(this, [this]() { return mySSPrintF("Unable to complete calculating data when trying to store it in {}", DMS_TreeItem_GetAssociatedFilename(this)); });

	assert(!SuspendTrigger::DidSuspend());

	auto mmd = dynamic_cast<MmdStorageManager*>(sm);
	if (mmd)
	{
		if (IsUnit(this))
		{
			AsUnit(this)->GetCount();
			mmd->UpdateDictionary(storageHolder.get()); // #1155: the dictionary emitted at OpenForWrite lacked this unit's Range
		}
		if (IsDataItem(this))
		{
			DataReadLock lock(AsDataItem(this)); // make sure data is calculated and stored
			// #1247: an item whose content another store item produced has no file of its own;
			// give it one before the dictionary below declares it.
			if (!IsMmdRuleOnly()) // #1264: not stored at all; the dictionary carries its rule instead
				mmd->MaterializeSharedContent(storageHolder.get(), AsDataItem(this));
			// #1154: writing the data required the domain's range, so here -- and not at
			// OpenForWrite, where the dictionary was first emitted -- the extent of a domain
			// declared OUTSIDE this storage is finally readable and can be recorded.
			mmd->UpdateDictionary(storageHolder.get());
		}
		return true;
	}

	auto nmsm = dynamic_cast<NonmappableStorageManager*>(sm);
	MG_CHECK(nmsm); // mmd's have been handled above
	if (!nmsm)
		return true;

	if (!DoWriteItem(nmsm->GetMetaInfo(storageHolder.get(), const_cast<TreeItem*>(this), StorageAction::write))
		|| GetCurrRangeItem()->WasFailed(FailType::Committed))
		return FinalizeFailure(this, [this]() { return mySSPrintF("Unable to write data to storage {}", DMS_TreeItem_GetAssociatedFilename(this)); });

	return !WasFailed(FailType::Committed);
}

static bool PartOfInterestImpl(const TreeItem* self)
{
	while (self)
	{
		if (self->GetInterestCount())
			return true;
		self = self->GetTreeParent().get();
	}
	return false;
}

bool TreeItem::PartOfInterest() const
{ 
	if (GetInterestCount())
		return true;
	if (!IsCacheItem())
		return false;

	return PartOfInterestImpl(GetTreeParent().get());
}

garbage_can TreeItem::TryCleanupMem() const
{
	DMS_ENTERS(ord_level_type::CountSection, dms_exclusive_v);
	if (IsCacheItem() && !IsCacheRoot())
		return {};


	leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);

	garbage_can garbage;
	TryCleanupMemImpl(garbage);
	return garbage;
}

bool TreeItem::TryCleanupMemImpl(garbage_can& garbageCan) const
{
	if (PartOfInterestOrKeep())
		return false;

	if (m_ItemLockCount < 0)
		return false;

	if (IsDataItem(this))
		if (!AsDataItem(this)->HasVoidDomainGuarantee())
			ClearDataObject(garbageCan);

	if (IsCacheItem())
		for (const TreeItem* subTI = _GetFirstSubItem(); subTI; subTI = subTI->GetNextItem())
			subTI->TryCleanupMemImpl(garbageCan);

	return true;
}

void TreeItem::ClearDataObject(garbage_can&) const
{}

