// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
#include "DataStoreManagerCaller.h"
#include "FreeDataManager.h"
#include "ParallelTiles.h"
#include "TicInterface.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemUtils.h"
#include "UnitProcessor.h"
#include "stg/MemoryMappeddataStorageManager.h"

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
	return CheckCalculatingOrReady(au->GetCurrRangeItem());
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

	item->throwItemErrorF("Cannot obtain a %s Lock because there %s %u %s Lock%s on this data",
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
	assert(rhs.m_Item.is_null());
	if (!m_Item)
		return;

	dms_assert(m_Item->m_DataLockCount);
	dms_assert(m_Item->GetInterestCount());
}

DataReadLockAtom::DataReadLockAtom(const AbstrDataItem* item)
	:	m_Item(item)
{
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

DataReadLockAtom::~DataReadLockAtom() noexcept
{
	if (!m_Item) // destruction from stack unwinding from throw in DataReadLock (before point of no return)
		return;
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
		if (m_Item->mc_RefItem || m_Item->m_DataLockCount > 1 || m_Item->PartOfInterest())
		{
			--m_Item->m_DataLockCount;
			return;
		}
	}

	actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("DataReadLockAtom::dtor") sg_ActorLockMap, m_Item.get()); // datalockcount 1->0 or drop of interest is 
	dms_assert(m_Item->m_DataLockCount > 0);
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
		--m_Item->m_DataLockCount;
	}

	if (m_Item->m_DataLockCount == 0 && !m_Item->PartOfInterest())
		m_Item->TryCleanupMem();
}

//----------------------------------------------------------------------
// DataReadLock
//----------------------------------------------------------------------

DataReadLock::DataReadLock(const AbstrDataItem* item)
	:	m_RefPtrLock(item = (item ? AsDataItem(item->GetCurrUltimateItem()):nullptr))
	,	m_DRLA(item)
{
	assert(std::uncaught_exceptions() == 0);

	if (!item)
		return;

	assert(item->m_DataLockCount);
	if (item->WasFailed(FailType::Data))
		item->ThrowFail();
	MG_CHECK(item->m_DataObject);
	
	DBG_START("DataReadLock", "CreateFromItem", MG_DEBUG_DATALOCKS);
	DBG_TRACE(("item = %s", item ? item->GetFullName().c_str() : "<null>"));

	actor_section_lock_map::ScopedLock localDataOpenLock(MG_SOURCE_INFO_CODE("DataReadLock::ctor") sg_ActorLockMap, item);

	*static_cast<SharedPtr<const AbstrDataObject>*>(this) = item->m_DataObject;
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
	assert(std::uncaught_exceptions() == 0);

	DBG_START("DataWriteLock", "CreateFromItem", MG_DEBUG_DATALOCKS);
	DBG_TRACE(("adi = %s", adi ? adi->GetFullName().c_str() : "<null>" ));

	dms_check_not_debugonly; 

	if (!adi)
		return;

	assert((adi->GetTreeParent() == nullptr) or adi->GetTreeParent()->Was(ProgressState::MetaInfo) or adi->GetTreeParent()->WasFailed(FailType::MetaInfo) || IsMainThread());

	actor_section_lock_map::ScopedLock localDataOpenLock(MG_SOURCE_INFO_CODE("DataWriteLockAtom::ctor") sg_ActorLockMap, adi);

	if (adi->m_DataLockCount > 0) // can happen before setting local lock
		DataLockError(adi, "Write");

	bool mustClear = (rwm == dms_rw_mode::write_only_mustzero);
	auto configItem = SharedPtr<const AbstrDataItem>((adi->m_BackRef && IsDataItem(adi->m_BackRef)) ? AsDataItem(adi->m_BackRef) : adi);
	if (!configItem->IsCacheItem())
	{
		if (auto sp = configItem->GetCurrStorageParent(true))
		{
			auto sm = sp->GetStorageManager();
			assert(sm);
			if (auto mmd = dynamic_cast<MmdStorageManager*>(sm))
			{
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

	m_adi = adi;
}

void DataWrite_Unlock(AbstrDataItem* adi)
{
	assert(adi);
	leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
	++adi->m_DataLockCount;
	dms_assert(adi->m_DataLockCount == 0);
}

DataWriteLock::~DataWriteLock()
{
	if (!m_adi)
		return;
	DataWrite_Unlock(m_adi.get());
}

SharedStr incompletedWriteTransactionMsg("Exception occured during generating operation.");

TIC_CALL void DataWriteLock::Commit()
{
	assert(IsLocked());
	auto adi = std::move(m_adi);
	assert(adi);
	assert(!m_adi);

	adi->m_DataObject = std::move(*this); // move from Writable to const
	assert(adi->m_DataObject);
	assert(!get());

#if defined(MG_DEBUG_INTERESTSOURCE_LOGGING)
	if (adi->m_State.Get(actor_flag_set::AFD_PivotElem))
		adi->m_DataObject->md_ActorFlags.Set(actor_flag_set::AFD_PivotElem);
#endif

	if (adi->mc_Calculator)
	{
		adi->SetDC(DataControllerRef{}, nullptr);
		adi->mc_Calculator.reset();
		adi->SetExpr(SharedStr{});
	}
	DataWrite_Unlock(adi.get());
	adi->MarkTS(UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("DataWriteLock::Commit")));
	if (!adi->IsEndogenous())
		adi->GetAbstrDomainUnit()->AddDataItemOut(adi.get());
}

//----------------------------------------------------------------------
// extern "C" interface funcs
//----------------------------------------------------------------------

#include "TicInterface.h"

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
