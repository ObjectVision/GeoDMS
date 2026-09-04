// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"
#include "LockLevels.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// DataReadLock / DataWriteLock: data-access locking of attribute data
// objects, file-data opening and write-commit administration.

#include "DataLocks.h"

#include <memory>

#include "act/ActorLock.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DebugCast.h"
#include "dbg/DmsCatch.h"
#include "mci/ValueClassID.h"
#include "utl/IncrementalLock.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"

#include "AbstrCalculator.h"
#include "DataArray.h"
#include "FreeDataManager.h"
#include "ParallelTiles.h"
#include "TicInterface.h"
#include "TreeItemClass.h"
#include "TreeItemProps.h"
#include "TreeItemContextHandle.h"
#include "TreeItemUtils.h"
#include "UnitProcessor.h"
#include "stg/MemoryMappedDataStorageManager.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_DATALOCKS 0
#endif

bool HasFixedRange(const ValueClass* vc)
{
	if (vc->HasFixedValues())
		return true;
	return vc->GetValueClassID() == ValueClassID::VT_Void;
}

#if defined(MG_DEBUG)

bool CheckCalculatingOrRangeKnown(const AbstrUnit* au)
{
	dms_assert(au);
	if (HasFixedRange(au->GetValueType()))
		return true;
	return CheckCalculatingOrReady(au->GetCurrRangeItem().get());
}

#endif

//----------------------------------------------------------------------
// DataLocks
//----------------------------------------------------------------------

[[noreturn]] void DataLockError(const AbstrDataItem* item, CharPtr lockType)
{
	dms_assert(item && item->GetDataObjLockCount());

	bool hasReadLocks = (item->GetDataObjLockCount() > 0);
	UInt32 nrLocks = hasReadLocks ? item->GetDataObjLockCount() : -item->GetDataObjLockCount();

	item->throwItemErrorF("Cannot obtain a {} Lock because there {} {} {} Lock{} on this data",
		lockType,
		(nrLocks == 1) ? "is" : "are",
		nrLocks,
		hasReadLocks ? "Read" : "Write",
		(nrLocks != 1) ? "s" : "");
}

//----------------------------------------------------------------------
// DataReadLockAtom mf implementation
//----------------------------------------------------------------------

DataReadLockAtom::DataReadLockAtom(DataReadLockAtom&& rhs) noexcept
	:	m_Item(std::move(rhs.m_Item))
{
	assert(rhs.m_Item == nullptr);
	if (!m_Item)
		return;

	dms_assert(m_Item->m_DataLockCount);
	dms_assert(m_Item->GetInterestCount());
}

DataReadLockAtom::DataReadLockAtom(const AbstrDataItem* item)
	:	m_Item(make_shared_tree(item, existing_obj{})) // share the item's REAL control block; a bare m_Item(item) used std::shared_ptr's inherited raw ctor -> a rogue control block whose delete-deleter destroyed the item out from under its other owners/locks
{
	DMS_ENTERS_ITEM(ord_level_type::ItemRegister, dms_exclusive_v);
	if (!item) //  || (item->m_DataLockCount < 0 && !type))
		return;
	if (item->WasFailed(FailType::Data))
		item->ThrowFail();

	dms_assert(!item->InTemplate());

	assert(!IsMultiThreaded2() || IsReadLocked(item) || IsMetaThread());

	assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides MustSuspend

	// From here we grant lock
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
		if (item->m_DataLockCount < 0)
			DataLockError(item, "Read");

		dms_assert(m_Item->m_DataLockCount >= 0);
		if (m_Item->m_DataLockCount >= 1) // n -> n+1 for n>0 is a singleton global critical section, first action
		{
			++m_Item->m_DataLockCount;
			dms_assert(m_Item->m_DataLockCount);
			return;
		}
	}
	assert(CheckDataReady(item));

	actor_section_lock_map::ScopedLock localDataOpenLock(MG_SOURCE_INFO_CODE("DataReadLockAtom::ctor") sg_ActorLockMap, item);

	if (item->m_DataLockCount < 0) // can happen before setting local lock
		DataLockError(item, "Read");

	leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
	++m_Item->m_DataLockCount;
	dms_assert(m_Item->m_DataLockCount);
}

void DataReadLockAtom::release() noexcept
{
	DMS_ENTERS_ITEM(ord_level_type::ItemRegister, dms_exclusive_v);
	if (!m_Item) // destruction from stack unwinding from throw in DataReadLock (before point of no return)
		return;
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
		if (!m_Item->mc_RefItem.expired() || m_Item->m_DataLockCount > 1 || m_Item->PartOfInterest())
		{
			--m_Item->m_DataLockCount;
			return;
		}
	}

	actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("DataReadLockAtom::release") sg_ActorLockMap, m_Item.get()); // datalockcount 1->0 or drop of interest is
	dms_assert(m_Item->m_DataLockCount > 0);
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
		--m_Item->m_DataLockCount;
	}

	if (m_Item->m_DataLockCount == 0 && !m_Item->PartOfInterest())
		m_Item->TryCleanupMem();
}

DataReadLockAtom::~DataReadLockAtom() noexcept
{
	release();
}

DataReadLockAtom& DataReadLockAtom::operator =(DataReadLockAtom&& rhs) noexcept
{
	if (this != &rhs)
	{
		release();                       // release the count we currently hold (a defaulted reset-move would leak it)
		m_Item = std::move(rhs.m_Item);  // adopt rhs's item; rhs becomes empty so its dtor is a no-op
	}
	return *this;
}

//----------------------------------------------------------------------
// DataReadLock
//----------------------------------------------------------------------

DataReadLock::DataReadLock(const AbstrDataItem* item)
	:	m_KeepItemAlive(make_shared_tree(item = (item ? AsDataItem(item->GetCurrUltimateItem()).get():nullptr), existing_obj{})) // owns the ultimate item; outlives both count-locks (see DataLocks.h)
	,	m_RefPtrLock(item)
	,	m_DRLA(item)
{
	DMS_ENTERS_ITEM(ord_level_type::ItemRegister, dms_exclusive_v);
	assert(std::uncaught_exceptions() == 0);

	if (!item)
		return;

	assert(item->m_DataLockCount);
	if (item->WasFailed(FailType::Data))
		item->ThrowFail();
	MG_CHECK(item->m_DataObject);
	
	DBG_START("DataReadLock", "CreateFromItem", MG_DEBUG_DATALOCKS);
	DBG_TRACE(("item = {}", item ? item->GetFullName().c_str() : "<null>"));

	actor_section_lock_map::ScopedLock localDataOpenLock(MG_SOURCE_INFO_CODE("DataReadLock::ctor") sg_ActorLockMap, item);

	*static_cast<SharedPtr<const AbstrDataObject>*>(this) = item->m_DataObject;
}

DataReadLock& DataReadLock::operator =(DataReadLock&& rhs) noexcept
{
	if (this != &rhs)
	{
		// Release what we currently hold BEFORE overwriting (a defaulted reset-move would leak the read-lock
		// members). Release the count-bearing members first -- m_DRLA (m_DataLockCount) and m_RefPtrLock
		// (m_ItemCount + s_SessionUsageCounter share) -- and only then drop the m_KeepItemAlive owner, so the
		// item is never freed while a count is still held (the counts-before-owner order of ~DataReadLock; see
		// the member-ordering note in DataLocks.h). Each of these member assignments is itself release-first.
		m_DRLA          = DataReadLockAtom();
		m_RefPtrLock    = ItemReadLock();
		m_KeepItemAlive = {};

		// Adopt rhs (the base move-assign drops our old data-object ref; rhs is emptied so its dtor is a no-op).
		SharedPtr<const AbstrDataObject>::operator=(std::move(static_cast<SharedPtr<const AbstrDataObject>&>(rhs)));
		m_KeepItemAlive = std::move(rhs.m_KeepItemAlive);
		m_RefPtrLock    = std::move(rhs.m_RefPtrLock);
		m_DRLA          = std::move(rhs.m_DRLA);
	}
	return *this;
}

//----------------------------------------------------------------------
// PreparedDataReadLock
//----------------------------------------------------------------------

void Update(const AbstrDataItem* adi)
{
	if (adi)
	{
		adi->UpdateMetaInfo();
		adi->UpdateDC();
#if defined(MG_DEBUG)
		auto refItem = adi->GetCurrUltimateItem();
		assert(refItem == adi->GetUltimateItem());
		assert(refItem == adi->GetCurrRangeItem());
#endif
		adi->GetAbstrValuesUnit()->PrepareData();
		if (!adi->PrepareDataUsage(DrlType::Certain))
			adi->ThrowFail();
	}
}

PreparedDataReadLock::PreparedDataReadLock(const AbstrDataItem* adi, CharPtr blockingAction)
	: SuspendTrigger::FencedBlocker(blockingAction)
	, DataReadLock((Update(adi), adi))
{}

auto CreateFileData(AbstrDataItem* adi, const SharedObj* abstrValuesRangeData, SharedStr filename, bool mustClear)
-> std::unique_ptr<AbstrDataObject>
{
	bool isPersistent = adi->IsCacheItem() && MustStorePersistent(adi);
	bool isTmp = !isPersistent;

	assert(!filename.empty());
	return CreateFileTileArray(adi, abstrValuesRangeData
		,	mustClear ? dms_rw_mode::write_only_mustzero : dms_rw_mode::write_only_all
		,	filename, isTmp
	);
}

auto OpenFileData(const AbstrDataItem* adi, const SharedObj* abstrValuesRangeData, SharedStr filenameBase)
-> std::unique_ptr<const AbstrDataObject>
{
	return CreateFileTileArray(adi, abstrValuesRangeData, dms_rw_mode::read_only, filenameBase, false);
}

//const AbstrTileRangeData* domain, 
//----------------------------------------------------------------------
// DataWriteLock
//----------------------------------------------------------------------

DataWriteLock::DataWriteLock(AbstrDataItem* adi, dms_rw_mode rwm, const SharedObj* abstrValuesRangeData) // was lockTile 
{
	DMS_ENTERS_ITEM(ord_level_type::ItemRegister, dms_exclusive_v);
	assert(std::uncaught_exceptions() == 0);

	DBG_START("DataWriteLock", "CreateFromItem", MG_DEBUG_DATALOCKS);
	DBG_TRACE(("adi = {}", adi ? adi->GetFullName().c_str() : "<null>" ));

	dms_check_not_debugonly; 

	if (!adi)
		return;

	assert((adi->GetTreeParent() == nullptr) or adi->GetTreeParent()->Was(ProgressState::MetaInfo) or adi->GetTreeParent()->WasFailed(FailType::MetaInfo) || IsMetaThread());

	actor_section_lock_map::ScopedLock localDataOpenLock(MG_SOURCE_INFO_CODE("DataWriteLockAtom::ctor") sg_ActorLockMap, adi);

	if (adi->m_DataLockCount > 0) // can happen before setting local lock
		DataLockError(adi, "Write");

	bool mustClear = (rwm == dms_rw_mode::write_only_mustzero);
	auto backRef = adi->m_BackRef.lock();
	auto configItem = make_shared_tree((backRef && IsDataItem(backRef.get())) ? AsDataItem(backRef.get()) : adi, no_zombies{}); // no_zombies: empty (not bad_weak_ptr) if the item is mid-destruction, e.g. during the storage-write teardown drain
	if (configItem && !configItem->IsCacheItem())
	{
		if (auto sp = configItem->GetCurrStorageParent(true))
		{
			auto sm = sp->GetStorageManager();
			assert(sm);
			if (auto mmd = dynamic_cast<MmdStorageManager*>(sm))
			{
				// #1179: an IntegrityCheck on a stored sub-item derails the write session -- the
				// data file is produced here, through this lock, but OpenForWrite never runs, so no
				// 0Dictionary.dms is written and the whole storage reads back empty. Refuse loudly
				// instead: restrictions belong on the storage holder, the common ancestor, whose
				// IntegrityCheck guards all its sub-items since #1180.
				for (SharedTreeItem guarded = configItem; guarded && guarded != sp; guarded = guarded->GetTreeParent())
					if (integrityCheckPropDefPtr->HasNonDefaultValue(guarded.get()))
						guarded->throwItemErrorF(
							"IntegrityCheck on an item stored in MMD storage {} is not supported: "
							"it would produce a storage without 0Dictionary.dms (issue #1179). "
							"Configure the restriction on the storage holder {} instead; "
							"its IntegrityCheck guards all its sub-items."
							, sm->GetNameStr(), sp->GetFullName());

				auto fsn = sm->GetNameStr();
				auto rn = configItem->GetRelativeName(sp.get());
				if (rn.empty())
				{
					rn = "@main";
				}

				auto fn = DelimitedConcat(fsn, rn);
				reset(CreateFileData(adi, abstrValuesRangeData, fn, mustClear).release()); // , !adi->IsPersistent(), true); // calls OpenFileData
				goto afterReset;
			}
		}
	}

	reset(CreateAbstrHeapTileFunctor(adi, abstrValuesRangeData, mustClear MG_DEBUG_ALLOCATOR_SRC("DataWriteLock")).release() );
/*
	if (abstrValuesRangeData)
	{
		MG_CHECK(adi->GetValueComposition() == ValueComposition::Single);

		visit<typelists::ranged_unit_objects>(adi->GetAbstrValuesUnit(), [this, abstrValuesRangeData]<typename T>(const Unit<T>*)
		{
			auto tileFunctor = dynamic_cast<DataArray<T>*>(this->get_ptr()); // ValueComposition ?
			assert(tileFunctor);
			if (tileFunctor)
				tileFunctor->InitValueRangeData(dynamic_cast<const range_or_void_data<T>*>(abstrValuesRangeData));
		});
	}
*/	

afterReset:

	dms_assert(get());
	if (rwm == dms_rw_mode::read_write)
		CopyData(adi->GetRefObj().get(), get());

	leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
	dms_assert(!adi->m_DataLockCount);
	--adi->m_DataLockCount;
	dms_assert(adi->m_DataLockCount < 0);

	m_adi = make_shared_tree(adi, existing_obj{});
}

void DataWrite_Unlock(AbstrDataItem* adi)
{
	DMS_ENTERS(ord_level_type::CountSection, dms_exclusive_v);
	assert(adi);
	leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
	++adi->m_DataLockCount;
	dms_assert(adi->m_DataLockCount == 0);
}

void DataWriteLock::release() noexcept
{
	if (!m_adi)
		return;
	DataWrite_Unlock(m_adi.get());
}

DataWriteLock::~DataWriteLock()
{
	release();
}

DataWriteLock& DataWriteLock::operator =(DataWriteLock&& rhs) noexcept
{
	if (this != &rhs)
	{
		release();                                                                              // release our current write lock (a defaulted reset-move of m_adi would leak it)
		SharedPtr<AbstrDataObject>::operator=(std::move(static_cast<SharedPtr<AbstrDataObject>&>(rhs))); // move the data-object ptr (base)
		m_adi = std::move(rhs.m_adi);                                                            // adopt rhs's item; rhs.m_adi -> empty
	}
	return *this;
}

SharedStr incompletedWriteTransactionMsg("Exception occured during generating operation.");

TIC_CALL void DataWriteLock::Commit()
{
	MG_CHECK(IsLocked()); // committing an empty lock (null/expired item at construction) is a caller bug: throw, don't defer a null deref
	auto adi = std::move(m_adi);
	MG_CHECK(adi);
	assert(!m_adi);

	adi->m_DataObject = std::move(*this); // move from Writable to const
	assert(adi->m_DataObject);
	assert(!get());

#if defined(MG_DEBUG_INTERESTSOURCE_LOGGING)
	if (adi->m_State.Get(actor_flag_set::AFD_PivotElem))
		adi->m_DataObject->md_ActorFlags.Set(actor_flag_set::AFD_PivotElem);
#endif

	if (adi->GetCalculatorMember())
	{
		adi->SetDC(DataControllerRef{}, nullptr);
		adi->ResetCalculatorMember();
		adi->SetExpr(SharedStr{});
	}
	DataWrite_Unlock(adi.get());
	adi->MarkTS(UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("DataWriteLock::Commit")));
	if (!adi->IsEndogenous())
		adi->GetDomainUnitOrThrow()->AddDataItemOut(adi.get());

	// Every commit of a variable-width result publishes its measured bytes-per-row (no-op for
	// fixed-width types). This is what carries real sequence widths across links the inheritance
	// guard rightly refuses -- value-type conversions (mm integer points -> float points) and
	// operators that build new geometry (geos_*) -- because the NEXT consumer re-estimates after
	// its suppliers completed, i.e. after this line ran for them (SS8.1.19/8.1.21). The tiles were
	// just written, so the measurement walks resident data.
	PublishMeasuredElementWidth(adi.get());
}

//----------------------------------------------------------------------
// extern "C" interface funcs
//----------------------------------------------------------------------

TIC_CALL DataReadLock*  DMS_CONV DMS_DataReadLock_Create(const AbstrDataItem* self, bool mustUpdateCertain)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrDataItem::GetStaticClass(), "DMS_DataReadLock_Create");

		if (self->PrepareDataUsage(mustUpdateCertain ? DrlType::Certain : DrlType::Suspendible))
		{
			auto result = std::make_unique<DataReadLock>(self);
			if (result->IsLocked())
				return result.release();
		}
	DMS_CALL_END
	return nullptr;
}

TIC_CALL void DMS_CONV DMS_DataReadLock_Release(DataReadLock* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(0, "DMS_DataReadLock_Release");
		delete self;

	DMS_CALL_END
}
