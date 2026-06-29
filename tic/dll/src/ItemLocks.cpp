// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <assert.h>
#include <cstdio>
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

#include <condition_variable>

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

#if defined(MG_DEBUG)
		auto producer = oc.lock();
		assert(!producer || producer->m_PhaseNumber);
		assert(!producer || self->GetCurrPhaseNumber() >= producer->m_PhaseNumber);
#endif defined(MG_DEBUG)

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
			{
				SuspendTrigger::FencedBlocker lock("treeitem_production_task");
				assert(self->GetCurrPhaseNumber() >= producer->m_PhaseNumber);

				producer->Join();
			}
		}

		// Wait for the write lock to be released. There is deliberately no instantaneous deadlock
		// test here: a held write lock (m_ItemCount < 0) may be released by an OperationContext, by
		// a main-thread-posted action, or by an ItemWriteLock destructor on another thread — none of
		// which are visible in a single (running-operations / lock-count) snapshot, so any such test
		// only produces false positives (#1126). unlock_unique always notifies cv_lockrelease on
		// release; genuine no-progress (task starvation) is detected over time by the
		// SuspendTrigger / progress watchdog, not from a snapshot here.
	retry:
		leveled_critical_section::unique_lock lock(cs_lockCounterUpdate);
		if (self->m_ItemCount < 0)
		{
			cv_lockrelease.wait_for(lock.m_BaseLock, std::chrono::milliseconds(500));
			if (self->m_ItemCount < 0)
				goto retry;
		}

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

	// Approach A: a read on a cache item must not proceed while a cache ANCESTOR is being (re)produced (an
	// ItemWriteLock on an ancestor rebuilds its whole subtree). Instead of read-LOCKING the ancestor chain (the
	// old design, which relied on the now-reversed rule that subItems own their parents, and otherwise leaves an
	// m_ItemCount on an unowned parent that then trips ~AbstrDataItem/ClearDataObject), we AWAIT any such write
	// without locking: walk up via GetTreeParent (owning per step) and, for any write-locked ancestor (its
	// m_ItemCount < 0), join its producer to drive it to completion, then re-scan. This is lifetime-safe: a
	// write-locked ancestor is kept alive by its own ItemWriteLock, which owns the chain downward. New reads thus
	// wait for ancestor production to finish; a read item's ancestors stay stable meanwhile via its interest.
	void AwaitAncestorWrites(const TreeItem* key)
	{
		assert(key);
		if (!key->IsCacheItem())
			return;
	restart:
		for (auto ancestor = key->GetTreeParent(); ancestor && ancestor->IsCacheItem(); ancestor = ancestor->GetTreeParent())
		{
			if (ancestor->m_ItemCount >= 0)
				continue; // not being produced
			std::shared_ptr<OperationContext> producer;
			{
				leveled_critical_section::scoped_lock lock(treeitem_production_task::cs_lockCounterUpdate);
				producer = ancestor->m_Producer.lock();
			}
			if (producer)
			{
				SuspendTrigger::FencedBlocker block("AwaitAncestorWrites");
				producer->Join();
			}
			else
			{
				leveled_critical_section::unique_lock lock(treeitem_production_task::cs_lockCounterUpdate);
				treeitem_production_task::cv_lockrelease.wait_for(lock.m_BaseLock, std::chrono::milliseconds(500));
			}
			goto restart; // chain may have changed; re-scan from the top
		}
	}

	// non-blocking variant: false if any cache ancestor is currently being produced.
	bool TryAwaitAncestorWrites(const TreeItem* key)
	{
		assert(key);
		if (key->IsCacheItem())
			for (auto ancestor = key->GetTreeParent(); ancestor && ancestor->IsCacheItem(); ancestor = ancestor->GetTreeParent())
				if (ancestor->m_ItemCount < 0)
					return false;
		return true;
	}

	void ThrowIfNotReady(const TreeItem* item)
	{
		// for opening actual data for shared (readonly) use, non-shared preparation action might be required; i.e LoadBlobIfAny OR read from CalcCache (if IsFnKnown and not DataAllocated).
		if (!CheckDataReady(item))
		{
			if (item->WasFailed(FailType::Data))
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

		// await (don't lock) any in-progress production on a cache ancestor, then read-lock ONLY this item
		AwaitAncestorWrites(item);
		treeitem_production_task::lock_shared(item);
	}

	bool TryReadLock(const TreeItem* item) // only works for reader_writer_lock, caller must call ReadFree
	{
		DBG_START("cs_lock", "ReadLock", MG_DEBUG_LOCKS);
		DBG_TRACE(("key=%s", AsString(item).c_str()));

		//		std::optional < ItemReadLock > lockDomain;
		//		if (IsDataItem(item))
		//			lockDomain.emplace(AsDataItem(item)->GetAbstrDomainUnit()->GetCurrRangeItem());

		// refuse if a cache ancestor is being produced, then try to read-lock ONLY this item
		if (!TryAwaitAncestorWrites(item))
			return false;
		if (!treeitem_production_task::try_lock_shared(item))
			return false;
		return true;
	}

	void ReadFree(const TreeItem* key) // only works for reader_writer_lock
	{
		treeitem_production_task::unlock_shared(key); // only this item was locked (Approach A)
	}

	void ReadLockInit(const TreeItem* item)
	{
		assert(item);
		assert(!std::uncaught_exceptions());

		if (!s_SessionUsageCounter.try_lock_shared())
		{
			assert(DSM::IsCancelling());
			assert(CancelableFrame::CurrActive());
			DSM::CancelOrThrow(item);
		}
		auto unlockDsmUsageCounter = make_releasable_scoped_exit([]() { s_SessionUsageCounter.unlock_shared(); });

		assert(item == item->GetCurrRangeItem());
		dbg_assert(item->CheckMetaInfoReadyOrPassor());

		// may wait for the completion of ItemWriteLock from a generating operation that was started by PrepareDataUsage.
		cs_lock::ReadLock(item);

		// from here nothrow
		unlockDsmUsageCounter.release();
		assert(item->WasFailed() || CheckDataReady(item));
	}

	bool TryReadLockInit(const TreeItem* item)
	{
		assert(item);
		assert(!std::uncaught_exceptions());

		if (!s_SessionUsageCounter.try_lock_shared())
		{
			assert(DSM::IsCancelling());
			assert(CancelableFrame::CurrActive());
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
		assert(item->WasFailed(FailType::Data) || CheckDataReady(item));
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
	if (IsDataItem(m_Ptr.get_ptr()) || IsUnit(m_Ptr.get_ptr()))
	{
		if (!IsCalculatingOrReady(m_Ptr.get_ptr()))
		{
			cs_lock::ReadFree(m_Ptr);
			s_SessionUsageCounter.unlock_shared();

			MG_CHECK(m_Ptr->WasFailed());
			m_Ptr->ThrowFail();
		}
	}

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
	MG_CHECK(!IsDataItem(m_Ptr.get_ptr()) || AsDataItem(m_Ptr.get_ptr())->GetCurrRefObj());

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
		m_ItemPtr = shared_tree_ptr<const TreeItem>(item, existing_obj{});

#if defined(MG_DEBUG_DATASTORELOCK)
		++sd_ItemWriteLockCounter;
#endif
	}
}

void ItemWriteLock::releaseHeldLock() noexcept
{
	if (!has_ptr())
		return;

	shared_tree_ptr<const TreeItem> garbage(GetItem(), existing_obj{});

	treeitem_production_task::unlock_unique(m_ItemPtr.get());
	s_SessionUsageCounter.unlock_shared();
#if defined(MG_DEBUG_DATASTORELOCK)
	--sd_ItemWriteLockCounter;
#endif
	m_ItemPtr = {};
}

ItemWriteLock::~ItemWriteLock()
{
	releaseHeldLock();
}

ItemWriteLock& ItemWriteLock::operator =(ItemWriteLock&& rhs) noexcept
{
	if (this != &rhs)
	{
		releaseHeldLock();                  // release the lock we currently hold (a defaulted move would leak it)
		m_ItemPtr = std::move(rhs.m_ItemPtr); // adopt rhs's lock; rhs becomes empty so its dtor is a no-op
	}
	return *this;
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
		item = item->GetTreeParent().get();
		assert(!item || item->IsCacheItem());
	} while (item);
	return false;
}

bool IsDataCurrCompleted(const TreeItem* item)
{
	assert(item);
	assert(item->GetCurrRangeItem() == item);
	assert(item->HasInterest()); // or else result would be volatile

	if (IsDataItem(item))
	{
		auto adi = AsDataItem(item);
		if (!adi->m_DataObject.has_ptr())
			return false;
		if (adi->WasFailed(FailType::Data))
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

	return true;
}

bool IsDataCurrReady(const TreeItem* item)
{
	assert(item);
	assert(item->HasInterest()); // or else result would be volatile

	if (!IsDataCurrCompleted(item))
		return false;

	if (item->m_ItemCount < 0) // still being processed ?
		return false;

	return true;
}

bool IsDataCurrStandby(const TreeItem* item)
{
	assert(item);
	assert(item->GetCurrRangeItem() == item);

	if (IsDataItem(item))
	{
		auto adi = AsDataItem(item);
		if (!adi->m_DataObject.has_ptr())
			return false;
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

bool IsAllDataCurrStandby(const TreeItem* item)
{
	assert(item);

	if (!IsDataCurrStandby(item))
		return false;
	if (item->IsCacheItem())
		for (auto subItem = item->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			if (!IsAllDataCurrStandby(subItem->GetCurrUltimateItem().get()))
				return false;
	return true;
}

bool IsDataReady(const TreeItem* item)
{
	assert(item);
	assert(item->HasInterest()); // or else result would be volatile

	bool result = IsDataCurrReady(item);
	assert(result || item->GetInterestCount());
	return result;
}

bool IsAllInterestedDataReady_impl(const TreeItem* item)
{
	MGD_PRECONDITION(item);
	MGD_PRECONDITION(item->IsCacheItem());
	MGD_PRECONDITION(item == item->GetCurrUltimateItem());

	if (item->GetInterestCount())
		if (!IsDataCurrReady(item))
			return false;

	for (auto subItem = item->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		if (!IsAllInterestedDataReady_impl(subItem->GetCurrUltimateItem().get()))
			return false;

	return true;
}

RTC_CALL bool s_IsDetectingIncInterest;

bool IsAllInterestedCalculatingOrDataReady_impl(const TreeItem* item)
{
	MGD_PRECONDITION(item);
	MGD_PRECONDITION(item->IsCacheItem());

	if (item->GetInterestCount())
	{
		auto ultimateCacheItem = item->GetCurrUltimateItem();
		assert(ultimateCacheItem);
		assert(ultimateCacheItem->IsCacheItem());

		if (IsCalculating(ultimateCacheItem.get()))
			return true;
		if (!IsDataCurrReady(ultimateCacheItem.get()))
		{
			MG_CHECK(!s_IsDetectingIncInterest);
			return false;
		}
	}

	for (auto subItem = item->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		if (!IsAllInterestedDataReady_impl(subItem->GetCurrUltimateItem().get()))
			return false;

	return true;
}

bool IsAllInterestedCalculatingOrDataReady(const TreeItem* item)
{
	assert(IsMetaThread());

	assert(item);
	if (!item->IsCacheItem())
	{
		assert(item->GetInterestCount()); // or else result would be volatile
		return IsCalculatingOrReady(item);
	}

	return IsAllInterestedCalculatingOrDataReady_impl(item);
}

bool CheckAllSubDataReady(const TreeItem* item)
{
	if (!CheckDataReady(item))
		return false;
	if (!item->IsCacheItem())
		return true;
	for (auto walker = item->WalkConstSubTree(nullptr); (walker = item->WalkConstSubTree(walker)); )
		if (!CheckDataReady(walker))
			return false;
	return true;
}

bool IsCalculatingOrReady(const TreeItem* item)
{
	if (IsCalculating(item))
		return true;
	if (IsDataReady(item))
		return true;
	return false;
}


static std::set< shared_tree_ptr<const TreeItem>>  s_ActiveProducerSet;
leveled_critical_section s_ActiveProducerSetMutex(item_level_type(0), ord_level_type::ActiveProducerSet, "ActiveProducerSet");
std::atomic<bool> s_RunTaskActive = false;


shared_tree_ptr<const TreeItem> GetATask()
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

	bool ready = IsDataReady(item->GetCurrUltimateItem().get());
	if (!ready)
	{
		if (IsMultiThreaded2()) {
			leveled_critical_section::scoped_lock lock(s_ActiveProducerSetMutex);

			s_ActiveProducerSet.insert(shared_tree_ptr<const TreeItem>(item, existing_obj{}));
			if (!s_RunTaskActive)
			{
				s_RunTaskActive = true;
GetPortableTaskGroup().run(RunTasks);




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
		item = item->GetTreeParent().get(); // cache items can inherit write rights from parent
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
			return IsDataReady(item);
		}
		std::shared_ptr<OperationContext> producer;
		{
			leveled_critical_section::scoped_lock lock(treeitem_production_task::cs_lockCounterUpdate);
			producer = item->m_Producer.lock();
			if (!producer && item->IsCacheItem())
			{
				for (auto parent = item->GetTreeParent(); parent; parent = parent->GetTreeParent())
				{
					producer = parent->m_Producer.lock();
					if (producer)
						break;
				}
			}
		}
		if (producer)
		{
			auto status = producer->Join();
			return status == task_status::done;
		}
		else
		{
			assert(IsMultiThreaded2());
			DoWorkWhileWaiting(); // max 500 milliseconds or actual work
			SuspendTrigger::MarkProgress(); // Is ti or any other item indeed progressing without dropping off from scope
			if (counter++ == 20) // sporadious wakeup at least every 10 secs to release from mysterious hang
				SuspendTrigger::DoSuspend();
		}
	} while (!SuspendTrigger::MustSuspend());

	assert(SuspendTrigger::DidSuspend()); // POSTCONDITION for MustSuspend returing true

	return false;
}

bool WaitReady(const TreeItem* item)
{
	assert(item);
	assert(item == item->GetCurrRangeItem());
	assert(CheckCalculatingOrReady(item) || item->WasFailed());
	if (IsDataReady(item))
		return true;
	if (!IsCalculatingOrReady(item))
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
		item = item->GetTreeParent().get(); // cache items can inherit write rights from parent
	}	while (item);
	assert(!item || CheckDataReady(item) || item->IsDataReadable() || item->WasFailed());
	return std::shared_ptr<OperationContext>();
}

