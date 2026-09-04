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

bool TreeItem::ReadItem(StorageReadHandle&& srh) // TODO: Make this a method of StorageReadHandle
{
	MG_DEBUGCODE( dms_assert( CheckMetaInfoReady() ); )

	assert(! GetCurrRefItem() ); // caller must take care of only calling ReadItem for UltimateItems

	MG_SIGNAL_ON_UPDATEMETAINFO

	auto keepInterest = GetInterestPtrOrCancel();

	if (WasFailed(FailType::Data))
		return false;
	if (IsDataReady(this))
		return true;

	const TreeItem* storageParent = GetCurrStorageParent(false).get();
	if (!storageParent)
		return false;
	
	TreeItemContextHandle checkPtr(this, "TreeItem::ReadItem");

	if (SuspendTrigger::MustSuspend())
		return false;

	try
	{
		auto progressMsg = mySSPrintF("Read {} from {}"
			, GetNameID()
			, storageParent->GetStorageManager()->GetNameStr()
		);

		reportD(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, progressMsg.AsRange());

		if (srh.Read())
			return true;
		else if (!SuspendTrigger::DidSuspend())
			throwItemError("DoReadItem returned Failure");
		assert(GetInterestCount());
	} 
	catch (...)
	{
 		assert(!HasCurrConfigData());

		if (!WasFailed(FailType::Data)) {
			auto err = catchException(true);
			err->TellExtraF("while reading data from {}", DMS_TreeItem_GetAssociatedFilename(this));
			DoFailCaller(err, FailType::Data);
		}
	}
	return false;
}

bool TreeItem::DoReadItem(StorageMetaInfoPtr smi)
{
	return false;
}

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
//	dms_assert(dc || self->GetCurrRefItem());
//	if (!dc) dc = GetDC(self->GetCalculator()); // TODO G8: unwind recursion
//	dms_assert(dc);
	//				const AbstrCalculator* apr = GetCalculator();
	//				dms_assert(apr); // guaranteed by HasCalculator
	//				dms_assert(!SuspendTrigger::DidSuspend()); // Postcondition when CreateResultingTreeItem returns a result
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

static how_to_proceed PrepareDataRead(std::shared_ptr<const TreeItem> self, const TreeItem* refItem, DrlType drlFlags)
{
	DMS_ENTERS_ITEM(ord_level_type::PrepareDataUsageLock, dms_exclusive_v);
	// PSEUDOCODE:
	// - Ensure refItem is data-readable and not a cache item.
	// - Prepare all suppliers (Calc group) of 'self'; on failure/suspend, propagate result.
	// - If refItem is a DataItem, ensure its domain unit (adu) can provide cardinality.
	// - Acquire storage manager and meta info to start a read OperationContext (oc).
	// - Before scheduling/starting the read oc:
	//     - If 'self' is a DataItem:
	//         - Fetch its domain unit (adu).
	//         - If the adu has a DataController, add its FutureData to futureSuppliers so the adu calculation
	//           is guaranteed to run before/with this read Operation.
	// - Track oc in self->m_ReadAssets to keep it alive.
	// - Return data_ready/suspended/failed based on status and triggers.

	MG_DEBUGCODE(dms_assert(!refItem->HasCalculatorImpl())); // implied by IsDataReadable
	dms_assert(!refItem->IsCacheItem());        // how else to derive data

	// Collect this read's future suppliers. Declared before the visit so the visited Calc-suppliers -- which for
	// a storage read include the storage manager's own data suppliers (e.g. strfiles FileName, via
	// StrFilesStorageManager::VisitSuppliers) -- are added as futures here. Each future keeps its supplier
	// calculated and of-interest from now, through GetMetaInfo below, and (once moved into the read
	// OperationContext) across the read. This closes the gap where a storage supplier's SupplInterest was dropped
	// on data-generation before a (re)read still needed it: a read OC keeps its suppliers of-interest, but only
	// the ones it is told about. Domain/values units are NOT visited under the Calc flag, so their futures are
	// still added explicitly below.
	FutureSuppliers futureSuppliers;

	// Also visit the item's configured ExplicitSuppliers, so a stored item that declares "read me
	// only after X" gets X's future as a prerequisite of its read OperationContext. Without this
	// the read oc was scheduled with only the Calc-suppliers (storage data suppliers like FileName)
	// and the domain/values futures below, and a read raced ahead of an ExplicitSupplier that was
	// still producing the very file to read -- measured on the Hestia python coupling
	// (pbl-nl/model-hestia-development#139): the gdal.vect read of a python-written parquet started
	// 2s before the exec_ec that runs python. The actor-level visitation (SupplierVisitFlag::Update)
	// already includes ExplicitSuppliers; the oc-level future list did not.
	auto supplResult = VisitSupplBoolImpl(self.get(), SupplierVisitFlag::CalcAndExplicitSuppliers, [&futureSuppliers](auto a) -> bool
		{
			auto t = dynamic_cast<const TreeItem*>(a);
			if (t)
				if (auto dc = t->GetCheckedDC())
				{
					FutureData tmpFut = dc; // hold interest while obtaining the future (the DC might not yet have interest)
					if (auto fut = dc->CallCalcResult())
						futureSuppliers.emplace_back(std::move(fut));
					else if (SuspendTrigger::DidSuspend())
						return false;
				}
			return true;
		}
	);
	if (supplResult == AVS_SuspendedOrFailed)
	{
		if (SuspendTrigger::DidSuspend())
			return how_to_proceed::suspended;
		else
		{
			assert(self->WasFailed(FailType::Data));
			return how_to_proceed::failed;
		}
	}

	assert(!SuspendTrigger::DidSuspend());

	if (IsDataItem(refItem))
	{
		const AbstrUnit* adu = AsDataItem(refItem)->GetAbstrDomainUnit();
		if (!adu->PrepareDataUsageImpl(drlFlags)) // make sure that the cardinality can be calculated
		{
			if (!adu->WasFailed(FailType::Data))
				return how_to_proceed::suspended;
			self->Fail(adu);
			return how_to_proceed::failed;
		}
	}
	dms_assert(!CheckCalculatingOrReady(refItem)); // was tested before and nothing could have started the calculation

	auto storageParent = refItem->GetStorageParent(false); assert(storageParent);
	SharedPtr<AbstrStorageManager> sm = storageParent->GetStorageManager(); assert(sm);
	if (auto nmsm = MakeSharedFromBorrowedObjectPtr(dynamic_cast<NonmappableStorageManager*>(sm.get())))
		if (StorageMetaInfoPtr readInfo = nmsm->GetMetaInfo(storageParent.get(), const_cast<TreeItem*>(refItem), StorageAction::read))
		{
			// #933: resolve pre-lock prerequisites HERE (suspendable, on the requesting thread) so the
			// gated read payload performs only the locked read and never waits while holding the CS.
			readInfo->PrepareReadDataOrSuspend();
			if (SuspendTrigger::DidSuspend())
				return how_to_proceed::suspended;

			auto readInfoPtr = std::make_shared<std::atomic<StorageMetaInfoPtr>>(std::move(readInfo));
			assert(!CheckCalculatingOrReady(refItem));

			using OcPtr = std::atomic<std::shared_ptr<OperationContext>>;
			std::shared_ptr<OcPtr> ocPtrPtr = std::make_shared<OcPtr>();

			// futureSuppliers was started by the Calc-supplier visit above (storage data suppliers like FileName).
			// Domain/values units are visited under the DomainValues flag, not Calc, so add their futures here too.
			dms_check(refItem->GetInterestCount());
			if (IsDataItem(refItem))
			{
				if (auto refAdu = AsDataItem(refItem)->GetAbstrDomainUnit())
				{
					dms_check(refAdu->GetInterestCount());
					// If the domain unit has a DataController, make it a future supplier.
					if (auto aduDC = refAdu->GetCheckedDC())
					{
						FutureData tmpFut = aduDC; // SymbDC might not yet have interest
						auto fut = aduDC->CallCalcResult();
						if (fut)
							futureSuppliers.emplace_back(std::move(fut));
					}
				}
				if (auto refAvu = AsDataItem(refItem)->GetAbstrValuesUnit())
				{
					dms_check(refAvu->GetInterestCount());
					// If the values unit has a DataController, make it a future supplier.
					if (auto avuDC = refAvu->GetCheckedDC())
					{
						FutureData tmpFut = avuDC; // SymbDC might not yet have interest
						auto fut = avuDC->CallCalcResult();
						if (fut)
							futureSuppliers.emplace_back(std::move(fut));
					}
				}
			}

			*ocPtrPtr = OperationContext::CreateItemWriter(const_cast<TreeItem*>(refItem),
				[storageParent
				, ocWeakPtrPtr = std::weak_ptr<OcPtr>(ocPtrPtr)
				, readInfoPtr
				, nmsm](OperationContext* ocPtr, explain_context_ptr_t /*context*/)
				{
					auto onExit = make_scoped_exit([ocWeakPtrPtr]() {
						if (auto ocSharedPtrPtr = ocWeakPtrPtr.lock())
							*ocSharedPtrPtr = std::shared_ptr<OperationContext>();
					});
					assert(readInfoPtr);
					assert(readInfoPtr->load());
					auto smi = readInfoPtr->exchange(StorageMetaInfoPtr());
					assert(!readInfoPtr->load());
					if (ocPtr->m_StorageLockHeld) // #933: gate already acquired the CS; adopt it
					{
						ocPtr->m_StorageLockHeld = false; // ownership transfers to sHandle on the next line
						StorageReadHandle sHandle(nmsm.get(), std::move(smi), adopt_storage_lock);
						sHandle.FocusItem()->ReadItem(std::move(sHandle)); // Read Item
					}
					else // runDirect / inline path: no gate ran, acquire normally
					{
						StorageReadHandle sHandle(nmsm.get(), std::move(smi)); // locks storage manager
						sHandle.FocusItem()->ReadItem(std::move(sHandle)); // Read Item
					}
				}
				, std::move(futureSuppliers)
				, false
				, nmsm // #933: gate on this storage manager
			);

			if (auto loadedPtr = ocPtrPtr->load())
				if (loadedPtr->GetStatus() < task_status::running)
				{
					self->m_ReadAssets.emplace<std::shared_ptr<OcPtr>>(ocPtrPtr);
					self->SetTSF(TSF_ReadAssetsInterestScoped); // interest-scoped: StopInterest releases it when refItem goes out of interest
				}

			readInfoPtr.reset();
			assert(CheckCalculatingOrReady(refItem) || refItem->WasFailed(FailType::Data) || SuspendTrigger::DidSuspend());
			assert(self->GetInterestCount());
		}

	if (refItem->IsFailed())
	{
		if (refItem != self.get())
			self->Fail(refItem);
		return how_to_proceed::failed;
	}

	if (SuspendTrigger::DidSuspend())
		return how_to_proceed::suspended;

	if (!CheckCalculatingOrReady(refItem))
	{
		// #1152: no read was scheduled; a mappable storage manager (.mmd) serves data items by file
		// mapping in PrepareDataUsageImpl and unit ranges from the dictionary's Range subtag, so
		// reaching this point means the storage holds no primary data for this item, e.g. a
		// half-written .mmd whose dictionary lacks this unit's Range and that has no data folder for
		// it. Fail rather than report readiness: PrepareDataUsage's contract is that a true return
		// leaves the item calculating-or-ready or failed, and from a not-ready, not-failed state every
		// consumer breaks (ItemReadLock's WasFailed check, operators dereferencing GetTiledRangeData()).
		self->Fail(mySSPrintF("no primary data found in storage {} for {}"
			, sm->GetNameStr(), refItem->GetFullName()), FailType::Data);
		return how_to_proceed::failed;
	}
	return how_to_proceed::data_ready;
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
		if (!IsCacheItem())
		{
			if (auto sp = GetCurrStorageParent(false))
			{
				auto sm = sp->GetStorageManager();
				assert(sm);
				if (auto mmd = dynamic_cast<MmdStorageManager*>(sm))
				{
					bool mustWrite = HasCalculator() && !GetCalculator()->IsDataBlock();
					bool mustSkip = HasCalculator() && GetCalculator()->IsDataBlock();
					if ((!mustSkip && !mmd->IsOpen()) || (mustWrite && !mmd->IsOpenForWrite()))
					{
						auto parent = GetStorageParent(mustWrite);
						if (!parent)
							mustSkip = true;
						else
						{
							auto lock = AbstrStorageManager::lock_t(mmd->m_CriticalSection);
							auto smi = StorageMetaInfo(parent.get(), this);
							if (mustWrite)
								mmd->OpenForWrite(smi);
							else
								mmd->OpenForRead(smi);
						}
					}
					if (!mustSkip && !mustWrite)
					{
						auto fsn = sm->GetNameStr();
						auto rn = GetRelativeName(sp.get());
						if (rn.empty())
						{
							rn = "@main";
						}

						auto fn = DelimitedConcat(fsn, rn);
						if (!IsFileOrDirAccessible(fn))
						{
							DoFailCaller(std::make_shared<ErrMsg>("Data not found in .MMD storage folder"), FailType::Data);
							goto failed;
						}
						else
						{
							// fixes #1028?
							auto adu = AsDataItem(this)->GetAbstrDomainUnit();
							if (!adu->PrepareData())
							{
								if (adu->WasFailed())
									Fail(adu);	
								return false;
							}
							assert(adu->GetCount() != SizeT(-1));
							auto fh = OpenFileData(AsDataItem(this), avu ? avu->GetTiledRangeData().get() : nullptr, fn);
							if (!fh)
							{
								DoFailCaller(std::make_shared<ErrMsg>("Cannot open data in .MMD storage folder"), FailType::Data);
								goto failed;
							}
							AsDataItem(GetCurrUltimateItem())->m_DataObject.reset(fh.release()); // , !adi->IsPersistent(), true); // calls OpenFileData
							return true;
						}
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
				default: dms_assert(false);
				}

//			dms_assert(!refItem->DataAllocated());
			dms_assert(!SuspendTrigger::DidSuspend());

			if (refItem->IsDataReadable()) // could be this (no calculator)
				switch (PrepareDataRead(make_shared_tree(this, existing_obj{}), refItem, drlFlags))
				{
				case how_to_proceed::nothing: break;
				case how_to_proceed::data_ready: goto data_ready;
				case how_to_proceed::failed: goto failed;
				case how_to_proceed::suspended: 
					assert(SuspendTrigger::DidSuspend());
					goto suspended;
				default: dms_assert(false);
				}

			//* REMOVE, DEBUG, SOLVES: for_each(xx[SubItem(Combine(...), 'Nr_1')] )
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
		// REMOVE, TODO: Actor::Fail(const DmsException&) toevoegen in Actor.h en hier gebruiken.
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
	SetTSF(TSF_DisabledStorage, disabledStorage);
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
	if (IsDataReadable())
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

//	return {}; // DEBUG

	leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);

	garbage_can garbage;
	TryCleanupMemImpl(garbage);
	return garbage;
}

bool TreeItem::TryCleanupMemImpl(garbage_can& garbageCan) const
{
	if (PartOfInterestOrKeep())
		return false;

	if (m_ItemCount < 0)
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

