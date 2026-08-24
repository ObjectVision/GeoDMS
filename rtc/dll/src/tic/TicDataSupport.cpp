// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// Data-layer support satellites of tic, merged from five small TUs (2026-08):
// DataLockContainers, FreeDataManager, SupplCache, DataStoreManager(Caller)
// and AbstrDataObject.

// ==== DataLockContainers ====

#include "DataLockContainers.h"

//----------------------------------------------------------------------
// class  : DataReadLockContainer
//----------------------------------------------------------------------

bool DataReadLockContainer::Add(const AbstrDataItem* adi, DrlType drlType)
{
	if	(!adi)
		return true;

	assert(drlType != DrlType::Certain || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility
	assert((drlType != DrlType::Suspendible) || !SuspendTrigger::DidSuspend()); // should have been acted upon

	if (!adi->PrepareDataUsage(drlType))
		return false;

	SharedTreeItem adiCurrItem = make_shared_tree(adi->GetCurrUltimateItem().get(), existing_obj{});
	assert(adiCurrItem->GetInterestCount());
	if (!WaitForReadyOrSuspendTrigger(adiCurrItem.get()))
		return false;

	assert(!SuspendTrigger::DidSuspend()); // PRECONDITION for DataReadLock, POSTCONDITION for WaitForReadyOrSuspendTrigger

	DataReadLock newLock(adi);
	if (newLock.IsLocked())
		push_back(std::move(newLock));
	assert(!newLock.IsLocked()); // ownership transferred?
	return true;
}

//----------------------------------------------------------------------
// class  : DataWriteLockContainer
//----------------------------------------------------------------------

void DataWriteLockContainer::Add(AbstrDataItem* adi, dms_rw_mode rwm)
{
	m_Locks.push_back( DataWriteLock(adi, rwm)	);
}

void DataWriteLockContainer::Commit()
{
	for(auto i = m_Locks.begin(), e = m_Locks.end(); i!=e; ++i)
		i->Commit();
}



// ==== FreeDataManager ====

#include "FreeDataManager.h"

#include "DbgInterface.h"
#include "utl/Environment.h"
#include "utl/Registry.h"
#include "vt/Couple.h"
#include "mci/ValueClass.h"
#include "utl/IncrementalLock.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataItemClass.h"

// ====================== public Static functions

bool MustStorePersistent(const TreeItem* ti)
{
	if (ti->GetFreeDataState())
		return false;
	if (ti->IsPassor())
		return !ti->m_BackRef.expired();
	else
		return !ti->HasConfigData();
}




// ==== SupplCache ====

#include "SupplCache.h"
#include "TreeItemProps.h"

#include "act/UpdateMark.h"
#include "set/VectorFunc.h"

#include <algorithm>

#include "AbstrCalculator.h"

//----------------------------------------------------------------------
// class  : SupplCache
//----------------------------------------------------------------------

SupplCache::SupplCache()
	:	m_NrConfigured(0)
	,	m_IsDirty   (false)        // default: no suppliers
{}

void SupplCache::Reset()
{
	m_SupplArray.reset();
	m_IsDirty = true;
}

void SupplCache::SetSupplString(WeakStr val)
{
	if (!m_IsDirty)
		Reset();
	m_strConfigured = val;
}

void SupplCache::InitAt(const TreeItem* fencedSource)
{
	m_NrConfigured = 1;
	m_IsDirty = false;
	m_SupplArray.reset(new ActorCRef[1]);
	m_SupplArray[0] = make_shared_tree(fencedSource, existing_obj{});
}

void SupplCache::InitAt(const ActorCRef* first, const ActorCRef* last)
{
	m_NrConfigured = last - first;
	m_IsDirty = false;
	m_SupplArray.reset( new ActorCRef[m_NrConfigured] );
	std::copy(first, last, m_SupplArray.get());
}

void SupplCache::BuildSet(const TreeItem* context) const
{
	dms_assert(context);
	dms_assert(m_IsDirty);

	SharedStr strConfigured = TreeItemPropertyValue(context, explicitSupplPropDefPtr);

	CharPtr
		iBegin = strConfigured.begin(), 
		iEnd   = strConfigured.send();

	m_NrConfigured = 0;

	while (true)
	{
		CharPtr
			iFirstEnd = std::find(iBegin, iEnd, ';');
		if (iFirstEnd != iBegin)
		{
			++m_NrConfigured;
			dms_assert(m_NrConfigured);
		}
		if (iFirstEnd == iEnd)
			break;
		iBegin = iFirstEnd + 1;
	}
	iBegin = strConfigured.begin();

	ActorCRefArray newSupplArray(new ActorCRef[m_NrConfigured]);
	if (!context->GetTreeParent())
		context->throwItemError("ExplicitSuppliers property cannot be set on root items");
	context = context->GetTreeParent().get();
	UInt32 i=0;
	while (true)
	{
		CharPtr iFirstEnd = std::find(iBegin, iEnd, ';');
		if (iFirstEnd != iBegin)
		{
			CharPtrRange explicitSupplierName(iBegin, iFirstEnd);
			Trim(explicitSupplierName);
			auto suppl = context->ResolveItemPath(explicitSupplierName);
			if (!suppl)
				context->throwItemErrorF("ExplicitSupplier {} not found", SingleQuote(explicitSupplierName.first, explicitSupplierName.second));

			assert(i<m_NrConfigured);
			newSupplArray[i++] = make_shared_tree(suppl.get(), existing_obj{}); // owning std borrow of the already-tree-owned supplier TreeItem
		}
		if (iFirstEnd == iEnd)
			break;
		iBegin = iFirstEnd + 1;
	}
	assert(i == m_NrConfigured);
	newSupplArray.swap(m_SupplArray);

	m_IsDirty = false;
}


// ==== session cancellation ====

#include "act/SupplierVisitFlag.h"
#include "dbg/DmsCatch.h"
#include "sym/Token.h"


#include "OperationContext.h"
#include "SessionData.h"

#include "LispRef.h"

[[noreturn]] void CancelOrThrow(const TreeItem* item)
{
	if (CancelableFrame::CurrActive())
		throw task_canceled{}; // assume it was cancelled due to outdated suppliers

	if (item)
		throwPreconditionFailed(item->GetConfigFileName().c_str(), item->GetConfigFileLineNr(), "CancelOrThrow requested without CancelableFrame");
	throwCheckFailed(MG_POS, "CancelOrThrow requested without CancelableFrame and without Item");
}

void CancelIfOutOfInterest(const TreeItem* item)
{
	if (IsMetaThread() && !CancelableFrame::CurrActive())
		return;

	CancelableFrame::CurrActiveCancelIfNoInterestOrForced(SessionData::IsCurrCancelling());

	if (CancelableFrame::CurrActiveCanceled() && !std::uncaught_exceptions())
	{
		CancelOrThrow(item);
	}
}


// ===================================================== usage of m_SupplierLevels;

#include "act/ActorVisitor.h"
#include "StateChangeNotification.h"

 // ==== code analysis support: TreeItem_SetAnalysisSource
#include "TicInterface.h"

static std::shared_ptr<const TreeItem>      s_SourceItem;
static std::map<const Actor*, supplier_level> s_SupplierLevels;

supplier_level operator & (supplier_level lhs, supplier_level rhs) { return supplier_level(UInt32(lhs) & UInt32(rhs)); }
supplier_level operator | (supplier_level lhs, supplier_level rhs) { return supplier_level(UInt32(lhs) | UInt32(rhs)); }

static void ProcessDeletion(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode)
{
	return;
	if (notificationCode == NC_Deleting)
	{
		assert(self != s_SourceItem.get()); // s_SourceItem is reference counted
		s_SupplierLevels.erase(self); // supplier level register is not reference counted.
	}
}

bool MarkSources(const Actor* a, supplier_level level)
{
	assert(a);
	SharedTreeItem ti = make_shared_tree(dynamic_cast<const TreeItem*>(a), existing_obj{}); // block a from deletion when in process
	if (a->IsPassor())
		if (!ti || ti->IsCacheItem())
			return false;

	if (s_SupplierLevels.empty())
		DMS_RegisterStateChangeNotification(ProcessDeletion, nullptr);

	supplier_level& currLevel = s_SupplierLevels[a];

	bool hasSource = (a == s_SourceItem.get());

	if ((currLevel & supplier_level::usage_flags) < level) // Source bit is also already determined and irrelevant for the decision to search for next level.
	{
		currLevel = level; // if Source bit was set, it will be set again.

		if (ti)
			NotifyStateChange(ti.get(), NC2_InterestChange);

		if (level == supplier_level::calc)
		{
			a->VisitSuppliers(SupplierVisitFlag::CalcAll, MakeDerivedProcVisitor([&hasSource](const Actor* s) { hasSource |= MarkSources(s, supplier_level::calc); }));
			a->VisitSuppliers(SupplierVisitFlag::MetaAll, MakeDerivedProcVisitor([&hasSource](const Actor* s) { hasSource |= MarkSources(s, supplier_level::meta); }));
		}
		else
		{
			assert(currLevel == supplier_level::meta);
			a->VisitSuppliers(SupplierVisitFlag::All, MakeDerivedProcVisitor([&hasSource](const Actor* s) { hasSource |= MarkSources(s, supplier_level::meta); }));
		}
		if (hasSource)
			currLevel = currLevel | supplier_level::uses_source_flag;
	}
	else if ((currLevel & supplier_level::uses_source_flag) == supplier_level::uses_source_flag)
		hasSource = true;
	return hasSource;
}


TIC_CALL void TreeItem_SetAnalysisTarget(const TreeItem * ti, bool mustClean)
{
	assert(IsMetaThread());
	if (mustClean)
	{
//	TODO: issue: registered suppliers may alredy be destroyed (and locations even be reused !). We need std::weak_ptr here.
//		for (auto& supplierRecord: dsm->m_SupplierLevels)
//			if (auto ti = dynamic_cast<const TreeItem*>(supplierRecord.first))
//				NotifyStateChange(ti, NC2_InterestChange);
		DMS_ReleaseStateChangeNotification(ProcessDeletion, nullptr);
		s_SupplierLevels.clear();
	}
	if (!ti)
		return;
	MarkSources(ti, supplier_level::calc);
}

TIC_CALL void TreeItem_SetAnalysisSource(const TreeItem * ti)
{
	DMS_CALL_BEGIN

		s_SourceItem = make_shared_tree(ti, existing_obj{});
		TreeItem_SetAnalysisTarget(ti, true); // sends a refresh at cleaning

	DMS_CALL_END
}

TIC_CALL supplier_level TreeItem_GetSupplierLevel(const TreeItem * ti)
{
	assert(IsMetaThread());
	auto iter = s_SupplierLevels.find(ti);
	if (iter != s_SupplierLevels.end())
		return iter->second;
	return supplier_level::none;
}



// ==== AbstrDataObject ====

#include "AbstrDataObject.h"
#include "TiledRangeData.h"

#include "geom/Range.h"
#include "mci/ValueWrap.h"
#include "mci/PropDef.h"
#include "mci/ValueClass.h"
#include "mem/tiledata.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "TileChannel.h"
#include "TreeItemClass.h"

//  -----------------------------------------------------------------------
//  Class  : AbstrDataObject
//  -----------------------------------------------------------------------

SharedPtr<const AbstrTileRangeData> AbstrDataObject::GetTiledRangeData() const
{ 
	return m_TileRangeData;
}

const DataItemClass* AbstrDataObject::GetDataItemClass() const
{
	const DataItemClass* rtc = debug_cast<const DataItemClass*>(GetDynamicClass());
	dms_assert(rtc);
	return rtc;
}

const ValueClass* AbstrDataObject::GetValuesType() const
{
	return GetDataItemClass()->GetValuesType();
}

//----------------------------------------------------------------------
// CopyData
//----------------------------------------------------------------------

// TODO G8: use info->changePos.

void CopyData(const AbstrDataObject* oldDataO, AbstrDataObject* newDataO, const DomainChangeInfo* info)
{
	auto oldDataSize = oldDataO->GetTiledRangeData()->GetElemCount();
	auto newDataSize = newDataO->GetTiledRangeData()->GetElemCount();
	auto copySize = std::min(oldDataSize, newDataSize);
	visit<typelists::value_elements>(oldDataO->GetValueClass(),
		[oldDataO, newDataO, info, newDataSize, copySize]<typename V>(const V*)
		{
			auto restSize = copySize;
			auto oldData = const_array_cast<V>(oldDataO);
			auto writeChannel = tile_write_channel<V>(mutable_array_checkedcast<V>(newDataO));
			auto tn = oldDataO->GetTiledRangeData()->GetNrTiles();
			for (tile_id t = 0; t != tn; ++t)
			{
				auto tileData = oldData->GetTile(t);
				auto currWriteSize = std::min(restSize, tileData.size());
				writeChannel.Write(tileData.begin(), tileData.begin()+currWriteSize);
				restSize -= currWriteSize;
			}
			writeChannel.WriteZeroes(newDataSize - copySize);
			dms_assert(writeChannel.IsEndOfChannel());
		}
	);
}


//----------------------------------------------------------------------
// Tile support
//----------------------------------------------------------------------


tile_loc AbstrDataObject::GetTiledLocation(row_id idx) const
{
	return GetTiledRangeData()->GetTiledLocation(idx);
}

tile_loc AbstrDataObject::GetTileDataLocation(datarow_id idx) const
{
	return GetTiledRangeData()->GetTileDataLocation(idx);
}

std::mutex s_DaFailReasonMutex;

auto AbstrDataObject::GetFailReason() const -> ErrMsgPtr
{
	auto lock = std::lock_guard(s_DaFailReasonMutex);
	return m_FailReason;
}

void AbstrDataObject::SetFailReason(ErrMsgPtr err)
{
	auto lock = std::lock_guard(s_DaFailReasonMutex);
	if (!m_FailReason)
		m_FailReason = err;
}

void AbstrDataObject::CheckFailure() const
{
	auto fr = GetFailReason(); // get it or not atomically
	if (fr)
		throw DmsException(fr);
}

//----------------------------------------------------------------------
// Illegal Abstracts
//----------------------------------------------------------------------

void AbstrDataObject::illegalNumericOperation() const
{
	throwErrorF("DataObject", "illegal numeric operation called on DataItem<{}>", GetValuesType()->GetName().c_str());
}

void AbstrDataObject::illegalGeometricOperation() const
{
	throwErrorF("DataObject", "illegal geometric operation called on DataItem<{}>", GetValuesType()->GetName().c_str());
}

// Support for numerics (optional)
Float64 AbstrDataObject::GetValueAsFloat64(SizeT  index) const                { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsFloat64(SizeT  index, Float64 val)         { illegalNumericOperation(); }
SizeT   AbstrDataObject::FindPosOfFloat64 (Float64 val, SizeT startPos) const { illegalNumericOperation(); }
Int32   AbstrDataObject::GetValueAsInt32(SizeT index) const                   { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsInt32(SizeT index, Int32 val)              { illegalNumericOperation(); }
UInt32  AbstrDataObject::GetValueAsUInt32(SizeT index) const                  { illegalNumericOperation(); }
SizeT   AbstrDataObject::GetValueAsSizeT(SizeT index) const                   { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsSizeT(SizeT index, SizeT val)              { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsDiffT(SizeT index, DiffT val)              { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsSizeT(SizeT index, SizeT val, tile_id t)   { illegalNumericOperation(); }
UInt8   AbstrDataObject::GetValueAsUInt8 (SizeT index) const                  { illegalNumericOperation(); }
void    AbstrDataObject::SetValueAsUInt32(SizeT index, UInt32 val)            { illegalNumericOperation(); }
SizeT  AbstrDataObject::FindPosOfSizeT(SizeT val, SizeT startPos) const       { illegalNumericOperation(); }
Float64 AbstrDataObject::GetSumAsFloat64() const                              { illegalNumericOperation(); }

// Support for numeric arrays (optional)
SizeT AbstrDataObject::GetValuesAsFloat64Array(tile_loc tl, SizeT len, Float64* data) const { illegalNumericOperation(); }
SizeT AbstrDataObject::GetValuesAsUInt32Array (tile_loc tl, SizeT len, UInt32* data) const { illegalNumericOperation(); }
SizeT AbstrDataObject::GetValuesAsInt32Array  (tile_loc tl, SizeT len, Int32* data) const { illegalNumericOperation(); }
SizeT AbstrDataObject::GetValuesAsUInt8Array  (tile_loc tl, SizeT len, UInt8* data) const { illegalNumericOperation(); }

void AbstrDataObject::SetValuesAsFloat64Array(tile_loc tl, SizeT len, const Float64* data) { illegalNumericOperation(); }
void AbstrDataObject::SetValuesAsInt32Array  (tile_loc tl, SizeT len, const Int32*   data) { illegalNumericOperation(); }
void AbstrDataObject::SetValuesAsUInt8Array  (tile_loc tl, SizeT len, const UInt8*   data) { illegalNumericOperation(); }

void AbstrDataObject::FillWithFloat64Values  (tile_loc tl, SizeT len, Float64 fillValue) { illegalNumericOperation(); }
void AbstrDataObject::FillWithUInt32Values   (tile_loc tl, SizeT len, UInt32  fillValue) { illegalNumericOperation(); }
void AbstrDataObject::FillWithInt32Values    (tile_loc tl, SizeT len, Int32   fillValue) { illegalNumericOperation(); }
void AbstrDataObject::FillWithUInt8Values    (tile_loc tl, SizeT len, UInt8   fillValue) { illegalNumericOperation(); }


// Support for Geometrics (optional)
DRect AbstrDataObject::GetActualRangeAsDRect(bool checkForNulls) const    { illegalGeometricOperation(); }

// Support for GeometricPoints (optional)
DPoint  AbstrDataObject::GetValueAsDPoint(SizeT index) const              { illegalGeometricOperation(); }
void    AbstrDataObject::SetValueAsDPoint(SizeT index, const DPoint& val) { illegalGeometricOperation(); }

// Support for GeometricSequences (optional)
void AbstrDataObject::GetValueAsDPoints(SizeT index, std::vector<DPoint>& dpoints) const { illegalGeometricOperation(); }

//----------------------------------------------------------------------
// Serialization and rtti
//----------------------------------------------------------------------


IMPL_CLASS(AbstrDataObject, nullptr)


//----------------------------------------------------------------------
// FutureTileArray
//----------------------------------------------------------------------

#include "FutureTileArray.h"

TIC_CALL auto GetAbstrFutureTileArray(const AbstrDataObject* ado) -> abstr_future_tile_array
{
	assert(ado); // PRECONDITION
	auto tn = ado->GetTiledRangeData()->GetNrTiles();
	auto result = abstr_future_tile_array(tn, ValueConstruct_tag() MG_DEBUG_ALLOCATOR_SRC("GetAbstrFutureTileArray"));
	for (tile_id t = 0; t != tn; ++t)
		result[t] = ado->GetFutureAbstrTile(t);
	return result;
}
