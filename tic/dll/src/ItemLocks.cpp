// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <assert.h>
#include "act/ActorLock.h"
#include "act/MainThread.h"
#include "dbg/DebugContext.h"
#include "mci/ValueClass.h"
#include "utl/scoped_exit.h"

#include "LockLevels.h"

#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "ItemLocks.h"
#include "Parallel.h"
#include "Unit.h"

#include "OperationContext.h"

//----------------------------------------------------------------------
// impl details
//----------------------------------------------------------------------
#if defined(MG_DEBUG)
bool MG_DEBUG_TPT_LOCKS(const TreeItem* self)
{
	return false; // return self->md_FullName == "/BAG_MakeSnapshot/snapshot_20170701/vbo/src";
}
#endif

namespace treeitem_production_task
{
	// const bool MG_DEBUG_TPT_LOCKS = MG_DEBUG_LOCKS;
	leveled_critical_section cs_lockCounterUpdate(item_level_type(0), ord_level_type::ObjectRegister, "LockCounter");
	std::condition_variable cv_lockrelease;

	void lock_unique(const TreeItem* self, std::weak_ptr<OperationContext> oc)
	{
		DBG_START("treeitem_production_task", "lock_unique", MG_DEBUG_TPT_LOCKS(self));

		assert(IsMetaThread() || oc.expired()); // creator tasks are initiated sequentialluy from the MainThread; Cleanup can come from any reading tasks that gives up the last iterest.

		leveled_critical_section::unique_lock lock(cs_lockCounterUpdate);
		cv_lockrelease.wait(lock.m_BaseLock, [self]() {return self->m_ItemCount <= 0;  });

//		assert(!self->m_ItemCount); // TODO: Check that earlier lock_unique is from the same thread
		--self->m_ItemCount;
		DBG_TRACE(("count=%d", self->m_ItemCount));
		self->m_Producer = oc;
	}

	void lock_unique(const TreeItem* self)
	{
		DBG_START("treeitem_production_task", "lock_unique", MG_DEBUG_TPT_LOCKS(self));
		DBG_TRACE(("count=%d, producer = %s", self->m_ItemCount, self->m_Producer.lock() ? "available" : "null"));

		leveled_critical_section::unique_lock lock(cs_lockCounterUpdate);
		cv_lockrelease.wait(lock.m_BaseLock, [self]() {return self->m_ItemCount == 0; });

		assert(self->m_Producer.expired()); // was cleaned up by producers task
		--self->m_ItemCount;
		DBG_TRACE(("count=%d", self->m_ItemCount));
	}

	void lock_shared(const TreeItem* self)
	{
		DBG_START("treeitem_production_task", "lock_shared", MG_DEBUG_TPT_LOCKS(self));
		DBG_TRACE(("count=%d, producer = %s", self->m_ItemCount, self->m_Producer.lock() ? "available" : "null"));

		if (self->m_ItemCount < 0)
		{
			std::shared_ptr<OperationContext> producer;
			{
				leveled_critical_section::scoped_lock lock(cs_lockCounterUpdate);
				producer = self->m_Producer.lock();
			}
			if (producer)
				producer->Join();
		}

		leveled_critical_section::unique_lock lock(cs_lockCounterUpdate);
		cv_lockrelease.wait(lock.m_BaseLock, [self]() {return self->m_ItemCount >= 0;  });

		assert(self->m_Producer.expired()); // was cleaned up by producers task
		assert(self->m_ItemCount >= 0);
		++self->m_ItemCount;
		DBG_TRACE(("count=%d", self->m_ItemCount));
	}

	bool try_lock_unique(const TreeItem* self)
	{
		DBG_START("treeitem_production_task", "try_lock", MG_DEBUG_TPT_LOCKS(self));

		leveled_critical_section::scoped_lock lock(cs_lockCounterUpdate);

		DBG_TRACE(("count=%d, producer = %s", self->m_ItemCount, self->m_Producer.lock() ? "available" : "null"));

		if (self->m_ItemCount != 0)
			return false;

		--self->m_ItemCount;
		assert(self->m_ItemCount == -1);
		DBG_TRACE(("count=%d", self->m_ItemCount));
		return true;
	}
	bool try_lock_shared(const TreeItem* self)
	{
		DBG_START("treeitem_production_task", "try_lock_read", MG_DEBUG_TPT_LOCKS(self));

		leveled_critical_section::scoped_lock lock(cs_lockCounterUpdate);

		DBG_TRACE(("count=%d", self->m_ItemCount));
		if (self->m_ItemCount < 0)
			return false;

		++self->m_ItemCount;
		DBG_TRACE(("count=%d", self->m_ItemCount));
		return true;
	}

	void unlock_unique(const TreeItem* self) noexcept
	{
		DBG_START("treeitem_production_task", "unlock_unique", MG_DEBUG_TPT_LOCKS(self));

		leveled_critical_section::scoped_lock lock(cs_lockCounterUpdate);

		DBG_TRACE(("count=%d, producer = %s", self->m_ItemCount, self->m_Producer.lock() ? "available" : "null"));

		assert(self->m_ItemCount < 0);
		auto newCount = ++self->m_ItemCount;
		DBG_TRACE(("count=%d", self->m_ItemCount));
		if (newCount < 0)
			return;

		self->m_Producer.reset();
		cv_lockrelease.notify_all();
	}

	void unlock_shared(const TreeItem* self) noexcept
	{
		DBG_START("treeitem_production_task", "unlock_shared", MG_DEBUG_TPT_LOCKS(self));

		leveled_critical_section::scoped_lock lock(cs_lockCounterUpdate);

		DBG_TRACE(("count=%d, producer = %s", self->m_ItemCount, self->m_Producer.lock() ? "available" : "null"));

		assert(self->m_ItemCount > 0);
		assert(self->m_Producer.expired());
		if (!--self->m_ItemCount)
			cv_lockrelease.notify_all();
		DBG_TRACE(("count=%d", self->m_ItemCount));
	}
/*  REMOVE
	void unlock(const TreeItem* self) noexcept
	{
		assert(self->m_ItemCount != 0); // assume this thread did lock one way or the other
		if (self->m_ItemCount > 0)
			unlock_shared(self);
		else
			unlock_unique(self);
	}
*/

}; // namespace treeitem_production_task;


namespace cs_lock {
	// only works for reader_writer_lock, caller must call ReadFree
	void ReadLock(const TreeItem* key);
	bool TryReadLock(const TreeItem* key);
	void ReadFree(const TreeItem* key);

	void PrepareReadAccess(const TreeItem* key) // only works for reader_writer_lock
	{
		assert(key);
		if (!key->IsCacheItem())
			return;
		const struct TreeItem* parent = key->GetTreeParent();
		if (!parent)
			return;
		ReadLock(parent); // let assoc_ptr slip here; caller must call FreeReadAccess(key) that will calltreeitem_production_task::unlock(parent); (through ReadFree)
	}

	bool TryPrepareReadAccess(const TreeItem* key) // only works for reader_writer_lock
	{
		assert(key);
		if (!key->IsCacheItem())
			return true;
		const struct TreeItem* parent = key->GetTreeParent();
		if (!parent)
			return true;
		return TryReadLock(parent); // let assoc_ptr slip here; caller must call FreeReadAccess(key) that will calltreeitem_production_task::unlock(parent); (through ReadFree)
	}

	void FreeReadAccess(const TreeItem* key)
	{
		assert(key);
		if (!key->IsCacheItem())
			return;
		const struct TreeItem* parent = key->GetTreeParent();
		if (parent)
			ReadFree(parent);
	}

	void ThrowIfNotReady(const TreeItem* item)
	{
		// for opening actual data for shared (readonly) use, non-shared preparation action might be required; i.e LoadBlobIfAny OR read from CalcCache (if IsFnKnown and not DataAllocated).
		if (!CheckDataReady(item))
		{
			if (item->WasFailed(FR_Data))
				item->ThrowFail(); // item will be unlocked at catch section
			DSM::CancelOrThrow(item);
		}
	}

	void ReadLock(const TreeItem* item) // only works for reader_writer_lock, caller must call ReadFree
	{
		DBG_START("cs_lock", "ReadLock", MG_DEBUG_LOCKS);
		DBG_TRACE(("key=%s", AsString(item).c_str()));

//		std::optional < ItemReadLock > lockDomain;
//		if (IsDataItem(item))
//			lockDomain.emplace(AsDataItem(item)->GetAbstrDomainUnit()->GetCurrRangeItem());

		// acquire read lock of context first, to guarantee that no write lock active on anchestor 
		// TODO: only when m_Count == 0 and then hold until lock is given or refused, see other TODO's
		PrepareReadAccess(item); // acquire read lock of context first, to guarantee that no write lock active on anchestor
		auto undoPrepare = make_releasable_scoped_exit([item]() { FreeReadAccess(item); });

		// acquire read lock of item so that a write lock has been lifted due to completion, cancellation or failure
		treeitem_production_task::lock_shared(item);
/* REMOVE
		if (loadData && !item->DataAllocated() && (IsDataItem(item) || IsUnit(item)))
		{
			treeitem_production_task::unlock_shared(item);
			LoadItem(item);
			treeitem_production_task::lock_shared(item);
		}
		*/
		undoPrepare.release();
	}

	bool TryReadLock(const TreeItem* item) // only works for reader_writer_lock, caller must call ReadFree
	{
		DBG_START("cs_lock", "ReadLock", MG_DEBUG_LOCKS);
		DBG_TRACE(("key=%s", AsString(item).c_str()));

		//		std::optional < ItemReadLock > lockDomain;
		//		if (IsDataItem(item))
		//			lockDomain.emplace(AsDataItem(item)->GetAbstrDomainUnit()->GetCurrRangeItem());

				// acquire read lock of context first, to guarantee that no write lock active on anchestor 
				// TODO: only when m_Count == 0 and then hold until lock is given or refused, see other TODO's
		if (!TryPrepareReadAccess(item)) // acquire read lock of context first, to guarantee that no write lock active on anchestor
			return false;
		auto undoPrepare = make_releasable_scoped_exit([item]() { FreeReadAccess(item); });

		// acquire read lock of item so that a write lock has been lifted due to completion, cancellation or failure
		if (!treeitem_production_task::try_lock_shared(item))
			return false;
		/* REMOVE
				if (loadData && !item->DataAllocated() && (IsDataItem(item) || IsUnit(item)))
				{
					treeitem_production_task::unlock_shared(item);
					LoadItem(item);
					treeitem_production_task::lock_shared(item);
				}
				*/
		undoPrepare.release();
		return true;
	}

	void ReadFree(const TreeItem* key) // only works for reader_writer_lock
	{
		treeitem_production_task::unlock_shared(key);
		FreeReadAccess(key); // TODO: only if m_Count became 0, see other TODO's
	}

	void ReadLockInit(const TreeItem* item)
	{
		assert(item);
		assert(!std::uncaught_exceptions());

		if (!s_SessionUsageCounter.try_lock_shared())
		{
			assert(DSM::IsCancelling());
			assert(OperationContext::CancelableFrame::CurrActive());
			DSM::CancelOrThrow(item);
		}
		auto unlockDsmUsageCounter = make_releasable_scoped_exit([]() { s_SessionUsageCounter.unlock_shared(); });

		assert(item == item->GetCurrRangeItem());
		dbg_assert(item->CheckMetaInfoReadyOrPassor());

		// may wait for the completion of ItemWriteLock from a generating operation that was started by PrepareDataUsage.
		cs_lock::ReadLock(item);

		// from here nothrow
		unlockDsmUsageCounter.release();
		assert(item->WasFailed(FR_Data) || CheckDataReady(item));
	}

	bool TryReadLockInit(const TreeItem* item)
	{
		assert(item);
		assert(!std::uncaught_exceptions());

		if (!s_SessionUsageCounter.try_lock_shared())
		{
			assert(DSM::IsCancelling());
			assert(OperationContext::CancelableFrame::CurrActive());
			DSM::CancelOrThrow(item);
		}
		auto unlockDsmUsageCounter = make_releasable_scoped_exit([]() { s_SessionUsageCounter.unlock_shared(); });

		assert(item == item->GetCurrRangeItem());
		dbg_assert(item->CheckMetaInfoReadyOrPassor());

		// may wait for the completion of ItemWriteLock from a generating operation that was started by PrepareDataUsage.
		if (!cs_lock::TryReadLock(item))
			return false;

		// from here nothrow
		unlockDsmUsageCounter.release();
		assert(item->WasFailed(FR_Data) || CheckDataReady(item));
		return true;
	}

	Int32 GetItemLockCount(const TreeItem* key)
	{
		return key->m_ItemCount;
	}
} // namespace cs_lock

//----------------------------------------------------------------------
// ItemLocks
//----------------------------------------------------------------------

#if defined(MG_DEBUG_DATASTORELOCK)

std::atomic<UInt32> sd_ItemReadLockCounter = 0; // DEBUG;

#endif

ItemReadLock::ItemReadLock() noexcept
{
	assert(!m_Ptr.has_ptr());
}

ItemReadLock::ItemReadLock(const TreeItem* item)
	: ItemReadLock(SharedTreeItemInterestPtr(item))
{}

ItemReadLock::ItemReadLock(SharedTreeItemInterestPtr&& rhs)
{
	if (!rhs)
		return;

	cs_lock::ReadLockInit(rhs);
	m_Ptr = std::move(rhs);

#if defined(MG_DEBUG_DATASTORELOCK)
	++sd_ItemReadLockCounter;
#endif
}

ItemReadLock::ItemReadLock(SharedTreeItemInterestPtr&& rhs, try_token_t justTry)
{
	if (!rhs)
		return;

	if (!cs_lock::TryReadLockInit(rhs))
		return;

	m_Ptr = std::move(rhs);

#if defined(MG_DEBUG_DATASTORELOCK)
	++sd_ItemReadLockCounter;
#endif
}

ItemReadLock::ItemReadLock(ItemReadLock&& rhs) noexcept
	:	m_Ptr(std::move(rhs.m_Ptr))
{}

ItemReadLock::~ItemReadLock()
{
	if (!m_Ptr.has_ptr())
		return;

	cs_lock::ReadFree(m_Ptr);
	s_SessionUsageCounter.unlock_shared();

#if defined(MG_DEBUG_DATASTORELOCK)
	--sd_ItemReadLockCounter;
#endif
}

//----------------------------------------------------------------------
// ItemWriteLock
//----------------------------------------------------------------------

#if defined(MG_DEBUG_DATASTORELOCK)
std::atomic<UInt32> sd_ItemWriteLockCounter = 0;
#endif

ItemWriteLock::ItemWriteLock() noexcept
{}

ItemWriteLock::ItemWriteLock(TreeItem* item, std::weak_ptr<OperationContext> ocb)
{
	if (item)
	{
		s_SessionUsageCounter.lock_shared();

		treeitem_production_task::lock_unique(item, ocb);
		m_ItemPtr = item;

#if defined(MG_DEBUG_DATASTORELOCK)
		++sd_ItemWriteLockCounter;
#endif
	}
}

ItemWriteLock::~ItemWriteLock()
{
	if (!has_ptr())
		return;

	SharedPtr<const TreeItem> garbage = GetItem();

	treeitem_production_task::unlock_unique(m_ItemPtr);
	s_SessionUsageCounter.unlock_shared();
#if defined(MG_DEBUG_DATASTORELOCK)
	--sd_ItemWriteLockCounter;
#endif
}

Int32 GetItemLockCount(const TreeItem* item)
{
	return cs_lock::GetItemLockCount(item);
}


bool IsCalculating(const TreeItem* item)
{
	assert(item);
	assert(item == item->GetCurrRangeItem());

	assert(item);
	do {
		Int32 itemLockCount = GetItemLockCount(item);
		if (itemLockCount < 0)
		{
			// CONTEXT
			return true;
		}
		if (itemLockCount > 0)
			return false; // read locks active
		if (!item->IsCacheItem())
			return false;
		item = item->GetTreeParent();
		assert(!item || item->IsCacheItem());
	} while (item);
	return false;
}
/*
bool CheckFilesPresent(const AbstrDataItem* adi)
{
	assert(IsMetaThread());

	if (DataStoreManager::Curr()->CheckFilesPresent(adi))
	{
		assert(adi->GetTSF(DSF_DSM_Allocated)); // avoid extra work
		return true;
	}
	assert(!adi->IsFnKnown()); // avoid extra work
	return false;
}

bool CheckFilesPresent(const DataController* dc, const TreeItem* cacheRoot, const AbstrDataItem* adi)
{
	if (DataStoreManager::Curr()->CheckFilesPresent(dc, cacheRoot, adi))
	{
		assert(adi->GetTSF(DSF_DSM_Allocated)); // avoid extra work
		return true;
	}
	assert(!adi->IsFnKnown()); // avoid extra work
	return false;
}
*/

bool IsDataCurrReady(const TreeItem* item)
{
	assert(item);
	assert(item->GetCurrRangeItem() == item);

	if (IsDataItem(item))
	{
		auto adi = AsDataItem(item);
		if (!adi->m_DataObject.has_ptr())
			return false;
		if (adi->WasFailed(FR_Data))
		{
			adi->m_DataObject.reset();
			return false;
		}
	}
	else if (IsUnit(item))
	{
		auto u = AsUnit(item);
		if (!u->HasTiledRangeData() and !u->IsDefaultUnit())
			return false;
	}
	else // just a container that may have been populated by template instantiation or for_each or other MetaCurryApplicator
		return item->GetIsInstantiated();

	if (item->m_ItemCount < 0) // still being processed ?
		return false;
	return true;
}

bool IsAllDataCurrReady(const TreeItem* item)
{
	if (!IsDataCurrReady(item))
		return false;
	if (item->IsCacheItem())
		for (auto subItem = item->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			if (!IsAllDataCurrReady(subItem->GetCurrUltimateItem()))
				return false;
	return true;
}

bool IsDataReady(const TreeItem* item)
{
	assert(item);
	assert(item->GetInterestCount()); // or else result would be volatile
	return IsDataCurrReady(item);
}

bool IsAllDataReady(const TreeItem* item)
{
	assert(item->GetInterestCount()); // or else result would be volatile
	return IsAllDataCurrReady(item);
}

bool CheckDataReady(const TreeItem* item) // TODO G8: REMOVE
{
	return IsDataReady(item);
}

bool CheckAllSubDataReady(const TreeItem* item)
{
	if (!CheckDataReady(item))
		return false;
	if (!item->IsCacheItem())
		return true;
	for (auto walker = item->WalkConstSubTree(nullptr); walker = item->WalkConstSubTree(walker); walker)
		if (!CheckDataReady(walker))
			return false;
	return true;
}

bool IsAllocated(const TreeItem* item) // TODO G8: kan dit weg ?
{
//	assert(IsDataItem(item) || !item->IsFnKnown());

	if (IsDataReady(item))
		return true;

	if (IsUnit(item))
		return AsUnit(item)->GetValueType()->HasFixedValues(); // range known?

	return false;
}

/*
bool IsDcReady(const DataController* dc, const TreeItem* cacheRoot, const TreeItem* cacheItem)
{
	if (IsDataReady(cacheItem, true))
	{
		if (!cacheItem->IsFnKnown())
			return true;
		assert(IsDataItem(cacheItem)); // implied by TSF_DSM_FnKnown
		assert(IsMetaThread()); // ???
		actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("IsDcReady") sg_ActorLockMap, cacheItem);
		return CheckFilesPresent(dc, cacheRoot, AsDataItem(cacheItem));
	}

	if (!IsDataItem(cacheItem) && !IsUnit(cacheItem))
		return true;

	assert(!cacheItem->IsFnKnown()); // it would have been ready
	return false;
}
*/

bool IsCalculatingOrReady(const TreeItem* item)
{
//	if (item->DataAllocated())
//		return true;
	if (IsCalculating(item))
		return true;
	if (IsDataReady(item))
		return true;
	return false;
}


static std::set< SharedPtr<const TreeItem>>  s_ActiveProducerSet;
leveled_critical_section s_ActiveProducerSetMutex(item_level_type(0), ord_level_type::ActiveProducerSet, "ActiveProducerSet");
std::atomic<bool> s_RunTaskActive = false;
dms_task s_RunTask;

SharedPtr<const TreeItem> GetATask()
{
	leveled_critical_section::scoped_lock lock(s_ActiveProducerSetMutex);
	if (s_ActiveProducerSet.empty())
		return {};
	auto task = *s_ActiveProducerSet.begin();
	s_ActiveProducerSet.erase(s_ActiveProducerSet.begin());
	return  task;
}

void RunTasks() {
	while (true) {
		auto nextTask = GetATask();
		if (!nextTask)
			break;
		if (nextTask->HasInterest())
			nextTask->PrepareData();
	}
	s_RunTaskActive = false;
}

bool RunTask(const TreeItem* item)
{
	assert(IsMetaThread());
	assert(item);
	assert(item->HasInterest());

	bool ready = IsDataReady(item->GetCurrUltimateItem());
	if (!ready)
	{
		if (IsMultiThreaded2()) {
			leveled_critical_section::scoped_lock lock(s_ActiveProducerSetMutex);

			s_ActiveProducerSet.insert(item);
			if (!s_RunTaskActive)
			{
				s_RunTaskActive = true;
				s_RunTask = dms_task(&RunTasks);
			}
		}
//		else
//			ready = item->PrepareData();
	}
	return ready;
}

bool CheckCalculatingOrReady(const TreeItem* item)
{
	assert(item);
	assert(item == item->GetCurrRangeItem());

//	if (item->DataAllocated())
//		return true;
	if (IsCalculating(item))
		return true;
	if (CheckDataReady(item))
		return true;
	return false;
}

bool IsCalculatingOrReady(const DataController* dc, const TreeItem* cacheRoot, const TreeItem* cacheItem)
{
//	if (cacheItem->GetTSF(TSF_DSM_SdKnown | TSF_DataInMem))
//		return true;
	if (IsDataReady(cacheItem))
		return true;
	return CheckCalculatingOrReady(cacheItem);
}


bool IsReadLocked(const TreeItem* item)
{
	assert(item);
	return GetItemLockCount(item) > 0;
}

bool IsInWriteLock(const TreeItem* item)
{
	do {
		assert(item);
		Int32 itemLockCount = GetItemLockCount(item);
		if (itemLockCount > 0)
			return false; // read locks active
		if (itemLockCount < 0)
			return true;
		if (!item->IsCacheItem())
			return false;
		item = item->GetTreeParent(); // cache items can inherit write rights from parent
	}	while (item);
	return false;
}


// TODO: zoek OperationContext op en oc->Join()
bool WaitForReadyOrSuspendTrigger(const TreeItem* item)
{
	assert(item);
	assert(item == item->GetCurrRangeItem());

	assert(!SuspendTrigger::DidSuspend()); // PRECONDITION

	if (SuspendTrigger::BlockerBase::IsBlocked())
		return WaitReady(item);

	assert(CheckCalculatingOrReady(item));

	UInt32 counter = 0;
	do {
		assert(!SuspendTrigger::DidSuspend()); // cotrolflow logic, POSTCONDITION for not MustSuspend
		if (!IsCalculating(item))
		{
			assert(!SuspendTrigger::DidSuspend()); // cotrolflow logic
			return IsDataReady(item) || item->WasFailed(FR_Data);
		}

		assert(IsMultiThreaded2());
		WaitForCompletedTaskOrTimeout(); // max 300 milliseconds
		if (IsMainThread())
			ProcessMainThreadOpers();
		SuspendTrigger::MarkProgress(); // Is ti or any other item indeed progressing without dropping off from scope
		if (counter++ == 34) // sporadious wakeup at least every 10 secs to release from mysterious hang
			SuspendTrigger::DoSuspend();
	} while (!SuspendTrigger::MustSuspend());

	assert(SuspendTrigger::DidSuspend()); // POSTCONDITION for MustSuspend returing true

	return false;
}

bool WaitReady(const TreeItem* item)
{
	assert(item);
	assert(item == item->GetCurrRangeItem());
	assert(CheckCalculatingOrReady(item) || item->WasFailed(FR_Data));
	if (IsDataReady(item))
		return true;
	if (item->WasFailed(FR_Data))
		return false;

	dbg_assert(!SuspendTrigger::DidSuspend());
	ItemReadLock lock(item); // maybe faster way, just call producer->Join; also calls LoadBlobIfAny
	return lock.has_ptr();
}

std::shared_ptr<OperationContext> GetOperationContext(const TreeItem* item)
{
	do {
		assert(item);

		auto result = item->m_Producer.lock();
		if (result)
			return result;
		if (!item->IsCacheItem())
			break;
		if (IsDataReady(item))
			break;
		item = item->GetTreeParent(); // cache items can inherit write rights from parent
	}	while (item);
	assert(!item || CheckDataReady(item) || item->IsDataReadable() || item->WasFailed());
	return std::shared_ptr<OperationContext>();
}

