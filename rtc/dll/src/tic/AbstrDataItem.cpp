// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <semaphore>

#include "AbstrDataItem.h"

#include "RtcTypelists.h"
#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "dbg/CheckPtr.h"
#include "dbg/DebugContext.h"
#include "dbg/DmsCatch.h"
#include "xct/DmsException.h"
#include "mci/ValueClass.h"
#include "mci/PropDef.h"
#include "ser/FileCreationMode.h"
#include "utl/mySPrintF.h"
#include "utl/scoped_exit.h"
#include "xct/DmsException.h"
#include "xml/XmlOut.h"

#include "LockLevels.h"

#include "TicInterface.h"

#include "AbstrCalculator.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataCheckMode.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataLockContainers.h"
#include "LispTreeType.h"
#include "OperationContext.h" // MemoryLedger_ReleaseRetained
#include "ParallelTiles.h"
#include "TileAccess.h"
#include "TileFunctorImpl.h"
#include "TileLock.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemUtils.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "CopyTreeContext.h"
#include "FreeDataManager.h"
#include "DataStoreManagerCaller.h"

#include "stg/AbstrStorageManager.h"
#include "DataArrayValue.h"

//  -----------------------------------------------------------------------
//  Class  : AbstrDataItem
//  -----------------------------------------------------------------------

AbstrDataItem::AbstrDataItem()
{}

AbstrDataItem::~AbstrDataItem() noexcept
{
	// (was assert(!GetInterestCount())): an item may now be destroyed while consumers still hold non-owning
	// (weak) supplier interest in it -- that interest does not keep it alive and is no-op-decremented once it
	// is gone, so a residual count here is expected.
	// (was assert(!IsOwned()): see SharedBase::~SharedBase -- intrusive count is no longer a liveness gate
	// for std::shared_ptr-managed TreeItems.)

	if (m_StatusFlags.Get(DSF_CachedByStorageManager))
		if (auto sp = GetStorageParent(false))
			if (auto sm = sp->GetStorageManager())
				sm->OnTerminalDataItem(this);

	SetKeepDataState(false);

	// Discard our data BEFORE releasing interest. A dying item's data must be dropped, not spilled to the disk
	// cache (which is what StopInterest -> TryCleanupMem would otherwise attempt); freeing it first also makes
	// that TryCleanupMem a no-op (no m_DataObject left to clean).
	if (m_DataObject)
	{
		garbage_can garbageCan;
		ClearDataObject(garbageCan); // releases m_DataObject (and calls ImLosingIt) so a non-owning functor back-ref can't dangle
	}

	// Release our own residual interest accounting when destroyed while still of interest. Consumer supplier
	// interest is non-owning (weak) now, so it does not keep us alive: m_InterestCount can be > 0 here while
	// StopInterest -- normally run by DecInterestCount on the 1->0 edge -- never fired. That would leak our
	// s_SessionUsageCounter share and the interest we placed on our own suppliers (domain/values units, DC, ...).
	// This MUST run in ~AbstrDataItem: virtual StopInterest must dispatch to AbstrDataItem::StopInterest (which
	// releases domain/values-unit interest); ~TreeItem's vtable can no longer reach it. KeepData was dropped
	// above, so HasInterest() now reflects only GetInterestCount(); zeroing the count first satisfies
	// StopInterest's precondition and makes ~TreeItem's symmetric guard a no-op. Done before the units are reset
	// below so GetAbstrDomainUnit()/GetAbstrValuesUnit() are still resolvable.
	if (GetInterestCount())
	{
		m_InterestCount = 0;
		garbage_can interestGarbage = StopInterest();
	}

	if (!IsEndogenous())
		if (auto du = m_DomainUnit.lock()) du->DelDataItemOut(this); // skip if the unit's owner already dropped it

	m_DomainUnit.reset();
	m_ValuesUnit.reset();

	assert(m_DataLockCount == 0);
}

//----------------------------------------------------------------------
// class  : AbstrDataItem (inline) functions that forward to DataObject
//----------------------------------------------------------------------

auto AbstrDataItem::GetAbstrDomainUnit() const -> const AbstrUnit*
{ 
	if (m_DomainUnit.expired() && IsMetaThread())
		m_DomainUnit = make_shared_tree(FindUnit(m_tDomainUnit, "Domain", nullptr), existing_obj{});
	return m_DomainUnit.lock().get(); // raw non-owning result: the unit is owned by the tree, outlives this call
}

auto AbstrDataItem::GetAbstrValuesUnit() const -> const AbstrUnit*
{
	if (m_ValuesUnit.expired() && IsMetaThread())
	{
		ValueComposition vc = GetValueComposition();
		m_ValuesUnit = make_shared_tree(FindUnit(m_tValuesUnit, "Values", &vc), existing_obj{});
	}
	return m_ValuesUnit.lock().get(); // raw non-owning result: the unit is owned by the tree, outlives this call
}

auto AbstrDataItem::GetDomainUnitOrThrow() const -> SharedUnit
{
	GetAbstrDomainUnit(); // meta-thread re-resolve of an expired/unset weak member (no-op otherwise)
	if (auto du = m_DomainUnit.lock())
		return du;
	throwItemError("Domain unit is no longer available");
}

auto AbstrDataItem::GetValuesUnitOrThrow() const -> SharedUnit
{
	GetAbstrValuesUnit(); // meta-thread re-resolve of an expired/unset weak member (no-op otherwise)
	if (auto vu = m_ValuesUnit.lock())
		return vu;
	throwItemError("Values unit is no longer available");
}

TIC_CALL auto AbstrDataItem::GetNonDefaultDomainUnit() const -> const AbstrUnit*
{
	auto adi = this;
	do {
		auto adu = adi->GetAbstrDomainUnit();
		assert(adu);
		if (!adu->IsDefaultUnit())
			return adu;
		adi = AsDataItem(adi->GetReferredItem()).get();
	} while (adi);
	return GetAbstrDomainUnit();
}

TIC_CALL auto AbstrDataItem::GetNonDefaultValuesUnit() const -> const AbstrUnit*
{
	auto adi = this;
	do {
		auto avu = adi->GetAbstrValuesUnit();
		assert(avu);
		if (!avu->IsDefaultUnit())
			return avu;
		adi = AsDataItem(adi->GetReferredItem()).get();
	} while (adi);
	return GetAbstrValuesUnit();
}


auto AbstrDataItem::GetCurrRefObj() const ->SharedPtr<const AbstrDataObject>
{
	dbg_assert(CheckMetaInfoReadyOrPassor());

	auto ultimateItem = GetCurrUltimateItem();
	MG_CHECK(ultimateItem);
	return debug_cast<const AbstrDataItem*>(ultimateItem.get())->GetCurrDataObj();
}

auto AbstrDataItem::GetRefObj() const -> SharedPtr<const AbstrDataObject>
{
	assert(IsMetaThread());
	MG_SIGNAL_ON_UPDATEMETAINFO

	auto ultimateItem = GetUltimateItem();
	MG_CHECK(ultimateItem);
	return debug_cast<const AbstrDataItem*>(ultimateItem.get())->GetDataObj();
}

Int32 AbstrDataItem::GetDataRefLockCount() const
{
	auto ult = _GetHistoricUltimateItem(this);
	if (!ult)
		return 0; // this is mid-destruction (weak_from_this expired -> empty ultimate, see _GetHistoricUltimateItem); no live data object to have a lock
	return AsDataItem(ult.get())->GetDataObjLockCount();
}

//----------------------------------------------------------------------
// class  : AbstrDataItem (non inline) functions that forward to DataObject
//----------------------------------------------------------------------

auto AbstrDataItem::CreateAbstrValue  () const -> std::unique_ptr<AbstrValue>
{ 
	return GetDynamicObjClass()->GetValuesType()->CreateValue();
}

void AbstrDataItem::ClearDataObject(garbage_can& garbage) const
{
	MG_CHECK(GetDataObjLockCount() == 0);
	MG_CHECK(m_ItemCount == 0);

	MemoryLedger_ReleaseRetained(this); // this is the funnel: the data is going, so unbook it

	if (m_DataObject)
		m_DataObject->ImLosingIt(); // clear any non-owning back-ref into this item (e.g. a tile functor's m_ResultAdi) before (deferred) destruction

	garbage |= std::move(m_DataObject);
	assert(!m_DataObject);

	assert(GetDataObjLockCount() == 0);
}

void AbstrDataItem::XML_DumpData(OutStreamBase* xmlOutStr) const
{ 
	assert(GetInterestCount()); // PRECONDITION, Callers responsibility
	XML_DataBracket dataBracket(*xmlOutStr);
	GetDataObj()->XML_DumpObjData(xmlOutStr, this); 
}

SharedStr AbstrDataItem::GetSignature() const
{
	// GetSignature is a SERIALIZER accessor: its only callers are the DMS config dump
	// (the 'attribute<vu>' tag) and the function-decl writer. It therefore takes the
	// RAW values-unit token — a bare source name re-resolves by up-scope search after
	// the item is written at another depth, whereas a resolved '../'-path would ascend
	// above root on reload (e.g. a body item whose values unit is a function parameter).
	// The user-visible PropValue(item,'ValuesUnit') keeps the resolved path via GetValue.
	return SharedStr(HasVoidDomainGuarantee()	?	"parameter<" :	"attribute<")
		+	s_ValuesUnitPropDefPtr->GetRawValue(this)
		+	">";
}

TokenID AbstrDataItem::GetXmlClassID() const
{
	return DataItemClass::GetStaticClass()->GetID(); // We avoid calling GetDynamicObjClass
}

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

bool AbstrDataItem::DoReadItem(StorageMetaInfoPtr smi)
{
	assert(CheckCalculatingOrReady(GetAbstrDomainUnit()->GetCurrRangeItem().get()));

	auto* sm_ = smi->StorageManager();
	assert(sm_);
	auto sm = MakeSharedFromBorrowedObjectPtr ( dynamic_cast<NonmappableStorageManager*>(sm_) );
	MG_CHECK(sm);
	assert(sm->IsOpen());
	assert(!sm->m_CriticalSection.try_acquire());

	if (!sm->DoesExist(smi->StorageHolder()))
		throwItemErrorF( "Storage {} does not exist", sm->GetNameStr().c_str() );

	try {
		MG_DEBUGCODE(TimeStamp currTS = LastChangeTS(); )

		auto abstract_domain_unit = GetAbstrDomainUnit();
		assert(abstract_domain_unit);

		auto number_of_dimensions = abstract_domain_unit->GetNrDimensions();
		if (number_of_dimensions == 2)
		{
			sm->DoCheckFactorSimilarity(smi);
			sm->DoCheck50PercentExtentOverlap(smi);
		}

		auto tn = abstract_domain_unit->GetNrTiles();
		if (IsMultiThreaded3() && tn > 1 && sm->AllowRandomTileAccess())
		{
			auto readerFarm = std::make_shared<reader_clone_farm>();

			auto tileGenerator = [this, sm, smi, readerFarm](AbstrDataObject* self, tile_id t)
			{
				auto context = TreeItemContextHandle(this, "DoReadItem");
				auto token = readerFarm->acquire();
				auto returnTokenOnExit = make_scoped_exit([&readerFarm, token]() { readerFarm->release(token); });

				auto& readerClonePtr = readerFarm->m_ClonePtrs[token];
				if (!readerClonePtr)
					readerClonePtr = sm->ReaderClone(smi);
				if (auto r = readerClonePtr->StorageManager()->ReadDataItem(smi, self, t); !r)
					r.Throw("Failure during Reading from storage");
			};
			auto rangeDomainUnit = AsUnit(GetAbstrDomainUnit()->GetCurrRangeItem()); assert(rangeDomainUnit);
			auto tileRangeData = rangeDomainUnit->GetTiledRangeData();
			auto rangeValuesUnit = AsUnit(GetAbstrValuesUnit()->GetCurrRangeItem()); assert(rangeValuesUnit);
			MG_CHECK(tileRangeData);
			if (true || sm->EasyRereadTiles())
			{
				visit<typelists::numerics>(rangeValuesUnit.get(), [this, tileRangeData, &tileGenerator]<typename V>(const Unit<V>*valuesUnit) {
					this->m_DataObject = make_unique_LazyTileFunctor<V>(make_shared_tree(this, existing_obj{}), tileRangeData.get(), valuesUnit->m_RangeDataPtr, std::move(tileGenerator)
						MG_DEBUG_ALLOCATOR_SRC(md_FullName + ".AbstrDataItem::DoReadItem of random rereadable tiles")
					).release();
				});
			}
		}
		else
		{
			DataWriteLock readResultHolder(this);
			MG_CHECK(readResultHolder.get_ptr());
			serial_for<tile_id>(0, GetAbstrDomainUnit()->GetNrTiles(),
				[sm, smi, this, &readResultHolder](tile_id t)->void
				{
					auto r = sm->ReadDataItem(smi, readResultHolder.get_ptr(), t);
					if (!r)
						r.Throw("Failure during Reading from storage");
				}
			);
			readResultHolder.Commit();
		}
		dbg_assert(currTS == LastChangeTS());
	}
	catch (const DmsException& x)
	{
		if (!WasFailed(FailType::Data))
			DoFailCaller(x.AsErrMsg(), FailType::Data);
		throw;
	}
	return true;
}

bool AbstrDataItem::DoWriteItem(StorageMetaInfoPtr&& smi) const
{
	assert(CheckDataReady(GetCurrUltimateItem().get()));
	dms_assert(IsMetaThread());

	DataReadLock lockForSave(this);

	auto* sm_ = smi->StorageManager();
	assert(sm_);
	auto* sm = dynamic_cast<NonmappableStorageManager*>(sm_);
	MG_CHECK(sm);


	FencedInterestRetainContext irc("AbstrDataItem::DoWriteItem");
	try {
		std::shared_ptr<const TreeItem> storageHolder = make_shared_tree(smi->StorageHolder(), existing_obj{});
		sm->ExportMetaInfo(storageHolder.get(), this);
		if (!sm->WriteDataItem(std::move(smi)))
			throwItemError("Failure during Writing");
		reportF(MsgCategory::storage_write, SeverityTypeID::ST_MajorTrace, "Writing to {}", sm->GetNameStr().c_str());
	}
	catch (const DmsException& x)
	{
		if (!WasFailed(FailType::Committed))
			DoFailCaller(x.AsErrMsg(), FailType::Committed);
		throw;
	}
	return true;
}

void AbstrDataItem::InitAbstrDataItem(TokenID domainUnit, TokenID valuesUnit, ValueComposition vc)
{
#if defined(MG_DEBUG)
		CharPtr
			debug_currDomainUnitStr = m_tDomainUnit.GetStr().c_str(),
			debug_currValuesUnitStr = m_tValuesUnit.GetStr().c_str(),
			debug_newDomainUnitStr = domainUnit.GetStr().c_str(),
			debug_newValuesUnitStr = valuesUnit.GetStr().c_str();
#endif

//	assert((m_tDomainUnit == domainUnit) || !IsDefined(m_tDomainUnit) || !domainUnit); // only called once?
//	assert((m_tValuesUnit == valuesUnit) || !IsDefined(m_tValuesUnit) || !valuesUnit); // only called once?
//	assert(!m_DataObject || (!valuesUnit && !domainUnit));             // and before it resulted in further construction

	m_tDomainUnit = domainUnit;
	m_tValuesUnit = valuesUnit;

	m_StatusFlags.SetValueComposition(vc);
}

const DataItemClass* AbstrDataItem::GetDynamicObjClass() const
{
	auto avu = GetAbstrValuesUnit();
	assert(avu);
	auto vc = GetValueComposition();
	auto au = avu->GetUnitClass();
	assert(au);

	auto vt = au->GetValueType(vc);

	if (!vt)
	{
		assert(vc != ValueComposition::Single);
		auto vcStr = GetValueCompositionID(vc).AsSharedStr();
		auto vtSingle = au->GetValueType(ValueComposition::Single);
		assert(vtSingle);
		auto vtSingleStr = vtSingle->GetID().AsSharedStr();

		throwDmsErrF("No ValueType for {} composition of {} values", vcStr.c_str(), vtSingleStr.c_str());
	}
	MG_CHECK(vt);
	auto dic = DataItemClass::FindCertain(vt, this);
	return dic;
}

const Class* AbstrDataItem::GetCurrentObjClass() const
{
	return HasDataObj()
		?	GetDataObj()->GetDynamicClass()
		:	GetDynamicClass();
}

void AbstrDataItem::Unify(const TreeItem* refItem, CharPtr leftRole, CharPtr rightRole) const
{
	const AbstrDataItem* refAsDi = AsDataItem(refItem);
	GetAbstrDomainUnit()->UnifyDomain(refAsDi->GetAbstrDomainUnit(), leftRole, rightRole, UM_Throw);
	while ((refItem = refAsDi->GetReferredItem().get()))
	{
		Unify(refItem, leftRole, rightRole);
		refAsDi = AsDataItem(refItem);
	}
	GetAbstrValuesUnit()->UnifyValues(refAsDi->GetAbstrValuesUnit(), leftRole, rightRole, UnifyMode(UM_AllowDefaultLeft|UM_Throw));

/*
	if (refAsDi->GetTSF(TSF_Categorical))
	{
		SharedStr resultMsg;
		if (!GetAbstrValuesUnit()->UnifyDomain(refAsDi->GetAbstrValuesUnit(), UnifyMode(UM_AllowDefaultLeft), &resultMsg))
			reportF(SeverityTypeID::ST_Warning, "{}: DomainUnification of categorical calculation result: {}"
			,	GetFullName()
			,	resultMsg
			);
	}
*/

}

void AbstrDataItem::CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const
{
	TreeItem::CopyProps(result, copyContext);

	auto res = debug_cast<AbstrDataItem*>(result);
	
	res->m_StatusFlags.SetValueComposition(GetValueComposition());
	if (copyContext.InFenceOperator())
	{
		res->m_DomainUnit = make_shared_tree(GetAbstrDomainUnit(), existing_obj{});
		res->m_ValuesUnit = make_shared_tree(GetAbstrValuesUnit(), existing_obj{});
		return;
	}

	// only copy unitnames when not defined
	if (copyContext.MustCopyExpr() || !IsDefined(res->m_tDomainUnit))
	{
		res->m_tDomainUnit = m_tDomainUnit;
		try {
			auto adu = GetAbstrDomainUnit();
			if (adu)
				res->m_tDomainUnit = copyContext.GetAbsOrRelUnitID(adu, this, res);
		}
		catch (...)
		{
			CatchFail(FailType::MetaInfo);
		}
	}
	if (copyContext.MustCopyExpr() || !IsDefined(res->m_tValuesUnit))
	{
		res->m_tValuesUnit = m_tValuesUnit;
		try {
			auto avu = GetAbstrValuesUnit();
			if (avu)
				res->m_tValuesUnit = copyContext.GetAbsOrRelUnitID(avu, this, res);
		}
		catch (...)
		{
			CatchFail(FailType::MetaInfo);
		}
	}
}

ValueComposition AbstrDataItem::GetValueComposition() const
{
	ValueComposition vc = m_StatusFlags.GetValueComposition();
	assert(vc != ValueComposition::Unknown);
	return vc;
}

void AbstrDataItem::SetValueComposition(ValueComposition vc)
{
	assert(vc != ValueComposition::Unknown);
	m_StatusFlags.SetValueComposition(vc);
}

void AbstrDataItem::LoadBlobStream (const InpStreamBuff* f)
{
	
//	assert(IsMetaThread());
	assert(m_State.GetProgress() >= ProgressState::MetaInfo || IsPassor());
	assert(GetCurrDataObj());
	assert(!m_DataLockCount);
//	assert(IsSdKnown());

	const AbstrUnit* adu = GetAbstrDomainUnit();
	assert(adu && adu->GetInterestCount());
	const AbstrUnit* adr = AsUnit(adu->GetCurrRangeItem()).get();
	assert(adr && adr->GetInterestCount());
	assert(CheckDataReady(adr));

	assert(CheckCalculatingOrReady(adr));

	assert(IsReadLocked(this) || !IsMultiThreaded2());

	BinaryInpStream ar(f);

	DataWriteLock writeHandle(this);

	for (tile_id t = 0, e = GetAbstrDomainUnit()->GetNrTiles(); t != e; ++t)
		writeHandle->DoReadData(ar, t);

	writeHandle.Commit();
}

void AbstrDataItem::StoreBlobStream(OutStreamBuff* f) const
{
	assert(GetCurrDataObj());
	assert(!GetAbstrDomainUnit()->IsCurrTiled());

	BinaryOutStream out(f);
	for (tile_id t = 0, e = GetAbstrDomainUnit()->GetNrTiles(); t != e; ++t)
		GetCurrDataObj()->DoWriteData(out, t);
}

bool AbstrDataItem::CheckResultItem(const TreeItem* refItem) const
{
	assert(refItem);
	if (!base_type::CheckResultItem(refItem))
		return false;
	const AbstrDataItem* adi = AsDataItem(refItem);

	SharedStr errMsgStr;
	{
		auto mydu = GetAbstrDomainUnit(); mydu->UpdateMetaInfo();
		auto refdu = adi->GetAbstrDomainUnit(); refdu->UpdateMetaInfo();
		if (!mydu->UnifyDomain(refdu, "the specified Domain", "the domain of the results of the calculation", UnifyMode::UM_AllowDefaultLeft, &errMsgStr))
			goto failResultMsg;
	}
	dbg_assert(m_LastGetStateTS >= refItem->m_LastGetStateTS || refItem->IsPassor());
	{
		auto myvu = GetAbstrValuesUnit(); myvu->UpdateMetaInfo();
		auto refvu = adi->GetAbstrValuesUnit(); refvu->UpdateMetaInfo();
		bool myvuIsCategorical = myvu->GetTSF(TSF_Categorical);
		CharPtr myvuTypeStr = myvuIsCategorical
			? "the specified categorical ValuesUnit"
			: "the specified noncategorical ValuesUnit";

		if (!myvu->UnifyValues(refvu, myvuTypeStr, "the values unit of the calculation results", UnifyMode::UM_AllowDefaultLeft, &errMsgStr))
			goto failResultMsg;

		if (adi->GetTSF(TSF_Categorical))
		{
			if (!myvu->UnifyDomain(refvu, myvuTypeStr, "the categorical calculation results", UnifyMode::UM_AllowDefaultLeft, &errMsgStr))
				goto failResultMsg;
			SetTSF(TSF_Categorical);
		}
		else if (myvuIsCategorical)
		{
			if (!myvu->UnifyDomain(refvu, myvuTypeStr, "the noncategorical calculation results", UnifyMode::UM_AllowDefaultRight, &errMsgStr))
				goto failResultMsg;
			SetTSF(TSF_Categorical);
		}
		return true;
	}
failResultMsg:
	Fail(errMsgStr, FailType::Determine);
	return false;
}

const AbstrUnit* AbstrDataItem::FindUnit(TokenID t, CharPtr role, ValueComposition* vcPtr) const
{
	assert(GetTreeParent());
	if (t == TokenID::GetUndefinedID())
		ThrowFail(mySSPrintF("Undefined {} unit", role), FailType::MetaInfo);
	const AbstrUnit* result = UnitClass::GetUnitOrDefault(GetTreeParent().get(), t, vcPtr);
	if (!result && !InTemplate())
	{
		auto msg = mySSPrintF("Cannot find {} unit {}", role, GetTokenStr(t));
		ThrowFail(msg, FailType::MetaInfo);
	}
	return result;
}

void AbstrDataItem::InitDataItem(const AbstrUnit* du, const AbstrUnit* vu, const DataItemClass* dic)
{
	assert( m_StatusFlags.GetValueComposition() != ValueComposition::Unknown );
	m_DomainUnit = make_shared_tree(du, existing_obj{});
	m_ValuesUnit = make_shared_tree(vu, existing_obj{});
}

auto AbstrDataItem::GetDataObj() const -> SharedPtr<const AbstrDataObject>
{
	auto dataObj = m_DataObject;
	assert(dataObj);
	return dataObj;
}

auto AbstrDataItem::GetCurrDataObj() const -> SharedPtr<const AbstrDataObject>
{ 
	return m_DataObject; 
}

const Object* AbstrDataItem::_GetAs(const Class* cls) const
{
	const Object* res = TreeItem::_GetAs(cls);

	return (res)
		?	res
		:	GetDataObj()->GetAs(cls);
}

//	Override Actor
void AbstrDataItem::StartInterest() const
{
	assert(GetInterestCount()==0);

	InterestPtr<const TreeItem*>
		domainHolder = GetAbstrDomainUnit()
	,	valuesHolder = GetAbstrValuesUnit()
	;

	TreeItem::StartInterest();

	// nothrow from here, release the InterestHolder without releaseing the interest
	domainHolder.release();
	valuesHolder.release();
}

garbage_can AbstrDataItem::StopInterest() const noexcept
{
	assert(GetInterestCount() == 0);

	garbage_can garbage;
	garbage |= OptionalInterestDec( GetAbstrDomainUnit() );
	garbage |= OptionalInterestDec( GetAbstrValuesUnit() );

	garbage |= TreeItem::StopInterest();
	return garbage;
}


SharedStr AbstrDataItem::GetDescr() const
{
	SharedStr descr = TreeItem::GetDescr();
	if (descr.empty() && !InTemplate())
	{
		try {
			if (GetAbstrValuesUnit())
				descr = GetAbstrValuesUnit()->GetDescr();
		}
		catch (...) {}
	}
	return descr;
}

bool AbstrDataItem::HasUndefinedValues() const // REMOVE, XXX TRY TO REPLACE BY DIRECT APPL OF GetCheckType
{
	return GetRawCheckMode() & DCM_CheckDefined;
}

void AbstrDataItem::GetRawCheckModeImpl() const
{
	assert(!GetTSF(DSF_ValuesChecked)); // PRECONDITION
	const AbstrDataObject* ado = GetDataObj().get();
	DataCheckMode dcm = ado->DoGetCheckMode();

	assert(!GetTSF(DSF_ValuesChecked)); // NO CONCURRENCY
	m_StatusFlags.SetDataCheckMode(dcm);
}

DataCheckMode AbstrDataItem::DetermineRawCheckModeImpl() const
{
	const AbstrDataObject* ado = GetDataObj().get();
	return ado->DoDetermineCheckMode();
}

using data_flags_lock_map = cs_lock_map<const AbstrDataItem*>;
data_flags_lock_map sg_DataFlagsLockMap("DataItemFlags");

DataCheckMode AbstrDataItem::GetRawCheckMode() const
{
	dbg_assert(CheckMetaInfoReadyOrPassor());
	MG_LOCKER_NO_UPDATEMETAINFO

	auto ultimateItem = GetCurrUltimateItem();
	MG_CHECK(ultimateItem);
	const AbstrDataItem* adi = debug_cast<const AbstrDataItem*>(ultimateItem.get());
	assert(adi);
	assert(CheckDataReady(adi));

	assert(adi->GetDataObjLockCount() > 0 );

	if (!adi->GetTSF(DSF_ValuesChecked))
	{
		assert(IsMetaThread() || IsMultiThreaded1or2());
		if (IsMultiThreaded1or2())
		{
			data_flags_lock_map::ScopedLock localLock(MG_SOURCE_INFO_CODE("AbstrDataItem::GetRawCheckMode") sg_DataFlagsLockMap, adi);
			if (!adi->GetTSF(DSF_ValuesChecked))
				adi->GetRawCheckModeImpl();
		}
		else
		{
			assert(IsMetaThread());
			adi->GetRawCheckModeImpl();
		}
	}
	return adi->m_StatusFlags.GetDataCheckMode();
}

DataCheckMode AbstrDataItem::DetermineRawCheckMode() const
{
	dbg_assert(CheckMetaInfoReadyOrPassor());
	MG_LOCKER_NO_UPDATEMETAINFO

	auto ultimateItem = GetCurrUltimateItem();
	MG_CHECK(ultimateItem);
	const AbstrDataItem* adi = debug_cast<const AbstrDataItem*>(ultimateItem.get());
	assert(adi);
	assert(CheckDataReady(adi));

	assert(adi->GetDataObjLockCount() > 0);

	assert(IsMetaThread() || IsMultiThreaded1or2());
	if (IsMultiThreaded1or2())
	{
		data_flags_lock_map::ScopedLock localLock(MG_SOURCE_INFO_CODE("AbstrDataItem::GetRawCheckMode") sg_DataFlagsLockMap, adi);
		return adi->DetermineRawCheckModeImpl();
	}
	else
	{
		assert(IsMetaThread());
		return adi->DetermineRawCheckModeImpl();
	}
}

DataCheckMode AbstrDataItem::GetCheckMode() const
{
	DataCheckMode dcm = GetRawCheckMode();
	GetCurrRefObj()->DoSimplifyCheckMode(dcm);
	return dcm;
}

DataCheckMode AbstrDataItem::DetermineActualCheckMode() const
{
	DataCheckMode dcm = DetermineRawCheckMode();
	GetCurrRefObj()->DoSimplifyCheckMode(dcm);
	return dcm;
}

DataCheckMode AbstrDataItem::GetTiledCheckMode(tile_id t) const
{
	if (t != no_tile && GetAbstrValuesUnit()->IsCurrTiled())
	{
		if (GetAbstrValuesUnit()->ContainsUndefined(t))
			return DataCheckMode( GetCheckMode() | DCM_CheckRange );
		return DCM_CheckRange;
	}
	else
	{
		assert(t == no_tile || t == 0);
		return GetCheckMode();
	}
}

bool AbstrDataItem::HasVoidDomainGuarantee() const
{
	auto adu = GetAbstrDomainUnit();
	if (!adu)
		// An unresolved domain reference gives NO void guarantee: this only happens for an
		// in-template item whose declared domain is a generic type-variable (a concrete item
		// with a missing domain already throws in FindUnit); an instantiation may bind it to a
		// non-void domain, so it must not be reported as a guaranteed-void (parameter) domain.
		// (Data operators never reach this — they run on concrete, instantiated items.)
		return false;
	return adu->IsKindOf( Unit<Void>::GetStaticClass() );
}

void AbstrDataItem::OnDomainUnitRangeChange(const DomainChangeInfo* info)
{
//	MG_CHECK2(false, "NYI: Copy Data into newly formed DataArray");
	if (GetCalculatorMember() ? GetCalculatorMember()->IsDataBlock() : bool(m_DataObject))
	{
		// is info->oldRangeData nog "actief" ? "actief" <-> Actor <-> TimeStamp of land change <-!-> Value Bases Calculation <-> declarative modelling
		try {
			assert(!GetDataObjLockCount());
			auto oldDataObject = GetDataObj();
			
			DataWriteLock lock(this); // calls CreateAbstrHeapTileFunctor(); is dan nu ineens info->newDataRange "actief" ?
			CopyData(oldDataObject.get(), lock.get(), info); // can I reuse tiles ?
			lock.Commit();
			assert(!GetCalculatorMember()); // DataWriteLock::Commit() destroyed DataBlockTask
		}
		catch (DmsException& x)
		{
			DoFailCaller(x.AsErrMsg(), FailType::Data);
		}
	}
}

// called when InterestCount drops to 0 or KeepData went to false 
bool AbstrDataItem::TryCleanupMemImpl(garbage_can& garbageCan) const
{
	MG_LOCKER_NO_UPDATEMETAINFO

	// copied from TreeItem::TryCleanupMemImpl, TODO G8: Reorder logic and avoid double code
	if (PartOfInterestOrKeep())
		return false;

	if (m_ItemCount < 0)
		return false;

	assert(!GetDataObjLockCount());
	assert(!PartOfInterest());

	if (GetDataRefLockCount())
		return false;

	if (!m_DataObject)
		return false;

	assert(!PartOfInterestOrKeep());

	if (m_DataObject->IsMemoryObject() && m_DataObject->IsSmallerThan(KEEPMEM_MAX_NR_BYTES)) // TODO G8: Consider leaning on CleanupMem; is the same if applied twice ?
		return true;

	bool hasSource = !HasCurrConfigData();
//	assert(!hasSource || IsCacheItem() || GetCurrStorageParent(false) || mc_Calculator)
	garbageCan |= const_cast<AbstrDataItem*>(this)->CleanupMem(hasSource, KEEPMEM_MAX_NR_BYTES+1);

	// copied from TreeItem::TryCleanupMemImpl, TODO G8: Reorder logic and avoid double code
	if (IsCacheItem())
		for (const TreeItem* subTI = _GetFirstSubItem(); subTI; subTI = subTI->GetNextItem())
			subTI->TryCleanupMemImpl(garbageCan);

	return true;
}

// called when InterestCount drops to 0 or KeepData went to false 
// TODO G8: Consider merging with ClearDataObject
garbage_can AbstrDataItem::CleanupMem(bool hasSourceOrExit, std::size_t minNrBytes) noexcept
{
	MG_LOCKER_NO_UPDATEMETAINFO

	assert(m_DataObject);
	// Drop Composite from root when Out Of Interest
	garbage_can garbageCan;
	if (hasSourceOrExit && !GetKeepDataState())
		garbageCan |= DropValue(); // calls ClearDataObject

	return garbageCan;
}

bool FindAndVisitUnit(const AbstrDataItem* adi, TokenID t, SupplierVisitFlag svf, const ActorVisitor& visitor)
{
	const ValueClass* vc = ValueClass::FindByScriptName(t);
	if (vc)
		return true;
	auto context = adi->GetTreeParent();
	if (!context)
		return true;

	SharedStr itemRefStr(t.AsStrRange());
	return context->FindAndVisitItem(itemRefStr, svf, visitor).has_value();
}

//	override virtuals of Actor
ActorVisitState AbstrDataItem::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (!InTemplate())
	{

		if (Test(svf, SupplierVisitFlag::DomainValues)) // already done by StartInterest
		{
			if (visitor.Visit(GetAbstrDomainUnit()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
			if (visitor.Visit(GetAbstrValuesUnit()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
		}
		if (Test(svf, SupplierVisitFlag::ImplSuppliers))
		{
			if (m_tDomainUnit) if (!FindAndVisitUnit(this, m_tDomainUnit, svf, visitor)) return AVS_SuspendedOrFailed;
			if (m_tValuesUnit) if (!FindAndVisitUnit(this, m_tValuesUnit, svf, visitor)) return AVS_SuspendedOrFailed;
		}
	}
	return TreeItem::VisitSuppliers(svf, visitor);
}

ActorVisitState AbstrDataItem::VisitLabelAttr(const ActorVisitor& visitor, SharedDataItemInterestPtr& labelLock) const
{
	if (visitor(this) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	return GetAbstrDomainUnit()->VisitLabelAttr(visitor, labelLock);
}

//----------------------------------------------------------------------
// Serialization and rtti
//----------------------------------------------------------------------

IMPL_DYNC_TREEITEMCLASS(AbstrDataItem, "AbstrDataItem")

//----------------------------------------------------------------------
// PropDefs for DataItems
//----------------------------------------------------------------------

struct DomainUnitPropDef : ReadOnlyPropDef<AbstrDataItem, SharedStr>
{
	DomainUnitPropDef()
		:	ReadOnlyPropDef<AbstrDataItem, SharedStr>("DomainUnit", set_mode::construction, xml_mode::attribute)
	{}

	// implement PropDef get/set virtuals
	auto GetValue(const AbstrDataItem* item) const-> SharedStr override
	{
		// The PROPERTY value (what PropValue(item,'DomainUnit') returns) is the
		// RESOLVED script name — documented public behavior, relative path since 18.x.
		auto adu = item->GetAbstrDomainUnit();
		if (adu)
			return adu->GetScriptName(item);
		assert(IsDefined(item->m_tDomainUnit));
		return SharedStr(item->m_tDomainUnit);
	}

	// The DUMP value (OutStreamBase::DumpPropList writes GetRawValue): prefer a
	// bare-name SOURCE token over a resolved *relative path*. A bare name is
	// up-scope searched, so it re-resolves after the item is copied to another
	// depth (a function/template body instantiated at the call site); a '../'-style
	// path would ascend above root there ("FollowDots: relative pathname ascended
	// above root" on reload of the dumped config).
	//
	// This split is the 2026-07-29 regression fix: fc15f892 put the bare-name
	// preference in GetValue, which silently changed the user-visible
	// PropValue(item,'DomainUnit'/'ValuesUnit') from the documented relative path
	// ('../meter') to the bare name ('meter') and broke the tst Operator suite
	// (Miscellaneous/PropValue asserts '../meter' since 18.x). Dump fidelity and
	// property semantics are different questions, so they now use different hooks.
	auto GetRawValue(const AbstrDataItem* item) const -> SharedStr override
	{
		auto resolved = GetValue(item);
		if (IsDefined(item->m_tDomainUnit))
		{
			SharedStr src(item->m_tDomainUnit);
			if (!src.empty() && !strchr(src.c_str(), '/') && strchr(resolved.c_str(), '/'))
				return src;
		}
		return resolved;
	}
	bool HasNonDefaultValue(const Object* self) const override
	{
		return !debug_cast<const AbstrDataItem*>(self)->HasVoidDomainGuarantee();
	}
};

struct ValuesUnitPropDef : ReadOnlyPropDef<AbstrDataItem, SharedStr>
{
	ValuesUnitPropDef()
		:	ReadOnlyPropDef<AbstrDataItem, SharedStr>("ValuesUnit", set_mode::construction, xml_mode::signature)
	{}

	// implement PropDef get/set virtuals
	auto GetValue(const AbstrDataItem* item) const -> SharedStr override
	{
		// see DomainUnitPropDef::GetValue: the PROPERTY stays RESOLVED (public API)
		auto avu = item->GetAbstrValuesUnit();
		if (avu)
			return avu->GetScriptName(item);
		assert(IsDefined(item->m_tValuesUnit));
		return SharedStr(item->m_tValuesUnit);
	}

	// see DomainUnitPropDef::GetRawValue: the DUMP prefers the re-instantiation-safe
	// bare source token
	auto GetRawValue(const AbstrDataItem* item) const -> SharedStr override
	{
		auto resolved = GetValue(item);
		if (IsDefined(item->m_tValuesUnit))
		{
			SharedStr src(item->m_tValuesUnit);
			if (!src.empty() && !strchr(src.c_str(), '/') && strchr(resolved.c_str(), '/'))
				return src;
		}
		return resolved;
	}
};

TokenID UnitName(const AbstrUnit* unit)
{
	if (unit->IsDefaultUnit())
		return unit->GetValueType()->GetID();
	else
		return TokenID(unit->GetFullName());
}

struct DomainUnitFullNamePropDef : ReadOnlyPropDef<AbstrDataItem, TokenID>
{
	DomainUnitFullNamePropDef()
		: ReadOnlyPropDef<AbstrDataItem, TokenID>("DomainUnit_FullName")
	{}

	// implement PropDef get/set virtuals
	TokenID GetValue(const AbstrDataItem* item) const override
	{
		auto adu = item->GetAbstrDomainUnit();
		if (!adu)
			return TokenID::GetUndefinedID();

		return UnitName(adu);
	}
};

struct ValuesUnitFullNamePropDef : ReadOnlyPropDef<AbstrDataItem, TokenID>
{
	ValuesUnitFullNamePropDef()
		: ReadOnlyPropDef<AbstrDataItem, TokenID>("ValuesUnit_FullName")
	{}

	// implement PropDef get/set virtuals
	TokenID GetValue(const AbstrDataItem* item) const override
	{
		auto avu = item->GetAbstrValuesUnit();
		if (!avu)
			return TokenID::GetUndefinedID();
		return UnitName(avu);
	}
};


struct ValueCompositionDataPropDef : ReadOnlyPropDef<AbstrDataItem, TokenID> 
{
	ValueCompositionDataPropDef()
		:	ReadOnlyPropDef<AbstrDataItem, TokenID>("ValueComposition", set_mode::construction, xml_mode::attribute)
	{}

	// implement PropDef get/set virtuals
	TokenID GetValue(const AbstrDataItem* item) const override
	{ 
		return GetValueCompositionID( item->GetValueComposition() );
	}
	bool HasNonDefaultValue(const Object* self) const override
	{
		return debug_cast<const AbstrDataItem*>(self)->GetValueComposition() != ValueComposition::Single;
	}

};

//----------------------------------------------------------------------
// PropDefPtrs for AbstrDataItems
//----------------------------------------------------------------------

DomainUnitPropDef staticDomainUnitPropDef;
ValuesUnitPropDef staticValuesUnitPropDef;
ValueCompositionDataPropDef staticDiValueCompositionPropDef;

PropDef<AbstrDataItem, SharedStr>* s_DomainUnitPropDefPtr = &staticDomainUnitPropDef;
PropDef<AbstrDataItem, SharedStr>* s_ValuesUnitPropDefPtr = &staticValuesUnitPropDef;


DomainUnitFullNamePropDef staticDomainUnitFullNamePropDef;
ValuesUnitFullNamePropDef staticValuesUnitFullNamePropDef;

//----------------------------------------------------------------------
// C style Interface functions for class id retrieval
//----------------------------------------------------------------------

#include "TicInterface.h"

TIC_CALL const Class* DMS_CONV DMS_AbstrParam_GetStaticClass()
{
	return AbstrParam::GetStaticClass();
}

TIC_CALL const AbstrUnit* DMS_CONV DMS_Param_GetValueUnit(const AbstrParam* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrParam::GetStaticClass(), "DMS_Param_GetValueUnit");
		return self->GetAbstrValuesUnit();

	DMS_CALL_END
	return 0;
}

TIC_CALL const AbstrUnit* DMS_CONV DMS_DataItem_GetValuesUnit(const AbstrDataItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrDataItem::GetStaticClass(), "DMS_DataItem_GetValuesUnit");
		return self->GetAbstrValuesUnit();

	DMS_CALL_END
	return 0;
}

TIC_CALL const AbstrUnit*  DMS_CONV DMS_DataItem_GetDomainUnit(const AbstrDataItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrDataItem::GetStaticClass(), "DMS_DataItem_GetDomainUnit");
		return self->GetAbstrDomainUnit();

	DMS_CALL_END
	return 0;
}

TIC_CALL ValueComposition  DMS_CONV DMS_DataItem_GetValueComposition(const AbstrDataItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrDataItem::GetStaticClass(), "DMS_DataItem_GetDomainUnit");
		return self->GetValueComposition();

	DMS_CALL_END
	return ValueComposition::Single;
}

TIC_CALL UInt32 DMS_CONV DMS_AbstrDataItem_GetNrFeatures(const AbstrDataItem* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrDataItem::GetStaticClass(), "DMS_AbstrDataItem_GetNrFeatures");
		return self->GetAbstrDomainUnit()->GetCount();

	DMS_CALL_END
	return 0;
}

TIC_CALL void DMS_CONV Table_Dump(OutStreamBuff* out, const TableColumnSpec* columnSpecPtr, const TableColumnSpec* columnSpecEnd, const SizeT* recNos, const SizeT* recNoEnd)
{
	SizeT nrDataItems = columnSpecEnd - columnSpecPtr;
	if (!nrDataItems)
		return;

	const AbstrUnit* domain = columnSpecPtr[0].m_DataItem->GetAbstrDomainUnit();
	for (auto columnSpecIter = columnSpecPtr + 1; columnSpecIter != columnSpecEnd; ++columnSpecIter)
		domain->UnifyDomain(columnSpecIter->m_DataItem->GetAbstrDomainUnit(), "Domain of the first column", "Domain of a following column", UM_Throw);

	DataReadLockContainer readLocks; readLocks.reserve(nrDataItems);
	for (auto columnSpecIter = columnSpecPtr; columnSpecIter != columnSpecEnd; ++columnSpecIter)
	{
		readLocks.push_back(PreparedDataReadLock(columnSpecIter->m_DataItem, "@Table_Dump"));
		if (columnSpecIter->m_RelativeDisplay)
			columnSpecIter->m_ColumnTotal = columnSpecIter->m_DataItem->GetRefObj()->GetSumAsFloat64();
	}
	std::vector<std::unique_ptr<const AbstrReadableTileData>> tileLocks; tileLocks.reserve(nrDataItems);
	for (const auto& drl : readLocks)
		tileLocks.emplace_back(drl.GetRefObj()->CreateReadableTileData(no_tile));

	FormattedOutStream fout(out, FormattingFlags::None);
	if (nrDataItems > 1 || !recNos || recNoEnd - recNos != 1)
	{
		// write header
		for (auto columnSpecIter = columnSpecPtr; columnSpecIter != columnSpecEnd; ++columnSpecIter)
		{
			if (columnSpecIter != columnSpecPtr)
				out->WriteByte(';');
			if (columnSpecIter->m_ColumnName)
			{
				auto columnStr = columnSpecIter->m_ColumnName.AsStrRange();
				DoubleQuote(fout, columnStr.m_CharPtrRange.first, columnStr.m_CharPtrRange.second);
			}
			else
			{
				SharedStr themeDisplName = GetDisplayNameWithinContext(columnSpecIter->m_DataItem, true, [columnSpecPtr, columnSpecEnd]() mutable -> const AbstrDataItem*
					{
						if (columnSpecPtr == columnSpecEnd)
							return nullptr;
						const AbstrDataItem* dataItem = columnSpecPtr->m_DataItem;
						++columnSpecPtr;
						return dataItem;
					}
				);
				DoubleQuote(fout, themeDisplName);
			}
		}
		out->WriteByte('\n');
	}

	SizeT nrRows = recNos ? (recNoEnd - recNos) : domain->GetCount();
	SizeT nrCols = tileLocks.size();
	for (SizeT i = 0; i != nrRows; ++i) {
		SizeT recNo = (recNos) ? *recNos++ : i;

		for (SizeT j = 0; j != nrCols; ++j) {
			if (j)
				out->WriteByte(';');
			if (columnSpecPtr[j].m_RelativeDisplay)
				fout << (100.0 * tileLocks[j]->GetAsFloat64(recNo) / columnSpecPtr[j].m_ColumnTotal);
			else
				tileLocks[j]->WriteFormattedValue(fout, recNo);
		}
		out->WriteByte('\n');
	}
}

TIC_CALL void DMS_CONV DMS_Table_Dump(OutStreamBuff* out, UInt32 nrDataItems, const ConstAbstrDataItemPtr* dataItemArray)
{
	DMS_CALL_BEGIN

		std::vector<TableColumnSpec> columnSpecs;
		columnSpecs.reserve(nrDataItems);
		while (nrDataItems--)
		{
			auto& currDataItemSpec = columnSpecs.emplace_back();
			auto dataItem = *dataItemArray++;
			currDataItemSpec.m_DataItem = dataItem;
//			currDataItemSpec.m_ColumnName = dataItem->GetID(); let Table_Dump fill this in
		}
		Table_Dump(out, begin_ptr(columnSpecs), end_ptr(columnSpecs), nullptr, nullptr);

	DMS_CALL_END
}

TIC_CALL const Class* DMS_CONV DMS_AbstrDataItem_GetStaticClass()
{
	return AbstrDataItem::GetStaticClass();
}

// *****************************************************************************
// Section:    AbstrValuesUnit interface function
// *****************************************************************************

const AbstrUnit* AbstrValuesUnit(const AbstrDataItem* adi)
{
	assert(adi);
	while (true)
	{
		auto au = adi->GetAbstrValuesUnit();
		if (!au->IsDefaultUnit())
			return au;
		adi = AsDataItem(adi->GetCurrRefItem()).get();
		if (!adi)
			return nullptr;
	}
}

//----------------------------------------------------------------------
// Building blocks for LazyTileFunctor heristics
//----------------------------------------------------------------------

UInt32 ElementWeight(const AbstrDataItem* adi)
{
	if (adi->HasVoidDomainGuarantee())
		return 0;
	auto bitSize = adi->GetAbstrValuesUnit()->GetValueType()->GetBitSize(); // bool => 1; UInt32 => 32; DPoint == 128
	if (!bitSize)
		return 256; // string weight
	if (adi->GetValueComposition() != ValueComposition::Single)
		return bitSize * 32; // Sequence<UInt8> -> 256 too
	return  bitSize;
}

// Assumed per-row volume of variable-width elements, matching the guesses ElementWeight has
// always made. A declared SizeUpperbound (schedule-with-lookahead.md §4.5) supersedes them.
const SizeT ASSUMED_STRING_BYTES = 32;
const SizeT ASSUMED_SEQ_LENGTH   = 32;

SizeT EstimateDataBytes(const AbstrDataItem* adi, SizeT nrElements)
{
	if (adi->HasVoidDomainGuarantee())
		return 0;
	auto bitSize = SizeT(adi->GetAbstrValuesUnit()->GetValueType()->GetBitSize());
	if (!bitSize)
		return nrElements * (ASSUMED_STRING_BYTES + sizeof(SizeT)); // chars plus a sequence index entry
	if (adi->GetValueComposition() != ValueComposition::Single)
		return nrElements * (((bitSize * ASSUMED_SEQ_LENGTH) >> 3) + sizeof(SizeT));
	return (nrElements * bitSize + 7) >> 3; // sub-byte elements are bit-packed
}

//----------------------------------------------------------------------
//	InterestCount management
//----------------------------------------------------------------------

#if defined(MG_DEBUG_INTERESTSOURCE)

#include "act/SupplInterest.h"
#include "dbg/DebugReporter.h"
#include "dbg/DebugContext.h"
#include "mci/Class.h"

inline CharPtr YesNo(bool v) { return v  ? "Yes" : "No"; }

struct InterestReporter : DebugReporter
{
	using ActorSet = DemandManagement::ActorSet;
	using ActorMap = std::map<ActorSet::value_type, interest_count_t>;

	static void ReportTree(ActorSet& done, const Actor* focusItem, UInt32 level, CharPtr role)
	{
		if (!focusItem)
			return;
		assert(focusItem);

		assert(focusItem->GetInterestCount());

		const TreeItem* ti = dynamic_cast<const TreeItem*>(focusItem);

		auto donePtr = done.find(focusItem);
		bool wasDone = (donePtr != done.end() && *donePtr == focusItem);

#if defined(MG_DEBUG_DCDATA)
		auto dc = dynamic_cast<const DataController*>(focusItem);
		reportF(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "{} LVL {} IC {} KD {} SI {}; {} {} {}: {}", focusItem,  level,
			focusItem->GetInterestCount(),
			YesNo(ti ? ti->GetKeepDataState() : false),
			YesNo(focusItem->DoesHaveSupplInterest()),
			wasDone ? "[Rep]" : "[New]",
			role, 
			focusItem->GetDynamicClass()->GetName().c_str(),
			dc ? dc->md_sKeyExpr : focusItem->GetFullName()
		);
#endif
		if (wasDone)
			return;

		++level;
		done.insert(donePtr, focusItem);

		const TreeItemDualRef* tidr = dynamic_cast<const TreeItemDualRef*>(focusItem);
		if (tidr)
		{
#if defined(MG_DEBUG_DCDATA)
			if (tidr->m_State.Get(DCFD_DataCounted))
				ReportTree(done, tidr->m_Data.get().get(), level, "CACHEDATA");
#endif
		}
		if (ti)
		{

			if (ti->IsCacheItem())
				ReportTree(done, ti->GetTreeParent().get(), level, "PARENT");
			ReportTree(done, ti->mc_RefItem.lock().get(), level, "REF_ITEM");
			ReportTree(done, ti->mc_DC.get(), level, "CALC");
	

			if (IsDataItem(ti))
			{
				SharedStr name( ti->GetFullName().c_str() MG_DEBUG_ALLOCATOR_SRC("ReportTree"));
				CDebugContextHandle debugContext2("ReportTreeUnits", name.c_str(), true);
				const AbstrDataItem* adi = AsDataItem(ti);
				if (adi->HasDataObj())
				{
					ReportTree(done, adi->GetAbstrDomainUnit(), level, "DOMAIN");
					ReportTree(done, adi->GetAbstrValuesUnit(), level, "VALUES");
				}
			}
		}

		assert(focusItem);
		if (focusItem && focusItem->DoesHaveSupplInterest() && s_SupplTreeInterest)
		{
			auto supplPtr = s_SupplTreeInterest->find(focusItem);
			if (supplPtr != s_SupplTreeInterest->end() && supplPtr->first == focusItem)
			{
				SupplInterestListElem* suppl = supplPtr->second;
				while (suppl)
				{
					ReportTree(done, suppl->m_Value.get_ptr(), level, "SUPPL");
					suppl = suppl->m_NextPtr;
				}
			}
		}
	}
	static void TrimSuppliers(ActorMap& interestRoots, const Actor* focusItem)
	{
		assert(focusItem);
		assert(focusItem->GetInterestCount());

		if (focusItem->DoesHaveSupplInterest() && s_SupplTreeInterest)
		{
			auto supplPtr = s_SupplTreeInterest->find(focusItem);
			if (supplPtr != s_SupplTreeInterest->end())
			{
				assert(supplPtr->first == focusItem);
				for (SupplInterestListElem* suppl = supplPtr->second; suppl; suppl = suppl->m_NextPtr)
					ReduceInterest(interestRoots, suppl->m_Value.get_ptr());
			}
		}
		const TreeItemDualRef* tidr = dynamic_cast<const TreeItemDualRef*>(focusItem);
		if (tidr)
		{
			if (tidr->m_State.Get(DCFD_DataCounted))
				ReduceInterest(interestRoots, tidr->m_Data.get().get());
			return;
		}

		const TreeItem* ti = dynamic_cast<const TreeItem*>(focusItem);
		if (!ti)
			return;
//		if (ti->IsCacheItem())
		ReduceInterest(interestRoots, ti->GetTreeParent().get());
		ReduceInterest(interestRoots, ti->mc_RefItem.lock().get());
		ReduceInterest(interestRoots, ti->mc_DC.get());
//		ReduceInterest(interestRoots, ti->mc_IntegrityChecker);

		if (IsDataItem(ti))
		{
			const AbstrDataItem* adi = AsDataItem(ti);
			if (adi->HasDataObj())
			{
				ReduceInterest(interestRoots, adi->GetAbstrDomainUnit());
				ReduceInterest(interestRoots, adi->GetAbstrValuesUnit());
			}
		}

	}

	static void ReduceInterest(ActorMap& as, const Actor* a)
	{
		if (!a)
			return;
		auto asPtr = as.find(a);
		assert(asPtr != as.end());
		assert(asPtr->first == a);
		assert(asPtr->second > 0);
		asPtr->second--;
/* REMOVE
		if (!--asPtr->second)
		{
			as.erase(a);
			TrimSuppliers(as, a);
		}
*/
	}

	void Report() const override
	{
#if defined(MG_DEBUG_LOCKLEVEL)
		LevelCheckBlocker blockChecks;
#endif
		leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
		leveled_critical_section::scoped_lock lock(DemandManagement::sd_UpdatingInterestSet);

		UInt32 totalInterestCount = 0;
		ActorMap interestRoots; 
		for (const auto& ii : DemandManagement::sd_InterestSet)
		{
			auto interestCount = ii->GetInterestCount();
			interestRoots[ii] = interestCount;
			totalInterestCount += interestCount;
		}


		for (auto i = DemandManagement::sd_InterestSet.begin(); i != DemandManagement::sd_InterestSet.end(); ++i)
			TrimSuppliers(interestRoots, *i);

		SharedStr nrRootsStr = AsString(interestRoots.size());
		DBG_START("InterestReporter", nrRootsStr.c_str(), true);

		UInt32 reducedInterest =0, reducedInterestCount = 0; 
		for (const auto& ii : interestRoots) 
			if (ii.second)
			{
				reducedInterest++;
				reducedInterestCount += ii.second;
			}

		reportF(SeverityTypeID::ST_MajorTrace, "#Items with interest: {}", DemandManagement::sd_InterestSet.size());
		reportF(SeverityTypeID::ST_MajorTrace, "sum #Interest:        {}", totalInterestCount);
		reportF(SeverityTypeID::ST_MajorTrace, "#reduced interest:    {}", reducedInterest);
		reportF(SeverityTypeID::ST_MajorTrace, "sum reduced #Interest:{}", reducedInterestCount);

/* Too much, leave it for now
* 
		ActorSet done;
		for (const auto& ii: interestRoots)
			if (ii.second)
				ReportTree(done, ii.first, 0,  "ROOT");
		DMS_TRACE(("{} done", done.size()));

*/
	}
};

InterestReporter sd_InterestSetReporter;


#endif // defined(MG_DEBUG_INTERESTSOURCE)
