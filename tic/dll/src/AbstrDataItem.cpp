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
#include "ParallelTiles.h"
#include "TileAccess.h"
#include "TileFunctorImpl.h"
#include "TileLock.h"
#include "TreeItemClass.h"
#include "TreeItemUtils.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "CopyTreeContext.h"
#include "FreeDataManager.h"
#include "DataStoreManagerCaller.h"

#include "stg/AbstrStorageManager.h"

//  -----------------------------------------------------------------------
//  Class  : AbstrDataItem
//  -----------------------------------------------------------------------

AbstrDataItem::AbstrDataItem()
{}

AbstrDataItem::~AbstrDataItem() noexcept
{
	assert(!GetInterestCount());
	assert(!IsOwned());
	SetKeepDataState(false);
	if (m_DataObject)
		CleanupMem(true, 0);

	if (!IsEndogenous())
		if (m_DomainUnit) m_DomainUnit->DelDataItemOut(this);

	m_DomainUnit.reset();
	m_ValuesUnit.reset();

	assert(m_DataLockCount == 0);
}

//----------------------------------------------------------------------
// class  : AbstrDataItem (inline) functions that forward to DataObject
//----------------------------------------------------------------------

auto AbstrDataItem::GetAbstrDomainUnit() const -> const AbstrUnit*
{ 
	if (!m_DomainUnit && IsMetaThread())
		m_DomainUnit = FindUnit(m_tDomainUnit, "Domain", nullptr);
	return m_DomainUnit;
}

auto AbstrDataItem::GetAbstrValuesUnit() const -> const AbstrUnit*
{ 
	if (!m_ValuesUnit && IsMetaThread())
	{
		ValueComposition vc = GetValueComposition();
		m_ValuesUnit = FindUnit(m_tValuesUnit, "Values", &vc);
	}
	return m_ValuesUnit;
}

TIC_CALL auto AbstrDataItem::GetNonDefaultDomainUnit() const -> const AbstrUnit*
{
	auto adi = this;
	do {
		auto adu = adi->GetAbstrDomainUnit();
		assert(adu);
		if (!adu->IsDefaultUnit())
			return adu;
		adi = AsDataItem(adi->GetReferredItem());
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
		adi = AsDataItem(adi->GetReferredItem());
	} while (adi);
	return GetAbstrValuesUnit();
}


inline const AbstrDataObject* AbstrDataItem::GetCurrRefObj()      const 
{
	dbg_assert(CheckMetaInfoReadyOrPassor());

	return debug_cast<const AbstrDataItem*>(GetCurrUltimateItem())->GetDataObj();
}

inline const AbstrDataObject* AbstrDataItem::GetRefObj()          const 
{
	assert(IsMetaThread());
	MG_SIGNAL_ON_UPDATEMETAINFO

	return debug_cast<const AbstrDataItem*>(GetUltimateItem())->GetDataObj(); 
}

Int32 AbstrDataItem::GetDataRefLockCount() const 
{ 
	return AsDataItem(_GetHistoricUltimateItem(this))->GetDataObjLockCount();
}

//----------------------------------------------------------------------
// class  : AbstrDataItem (non inline) functions that forward to DataObject
//----------------------------------------------------------------------

AbstrValue* AbstrDataItem::CreateAbstrValue  () const 
{ 
	return GetDynamicObjClass()->GetValuesType()->CreateValue();
}

void AbstrDataItem::ClearData(garbage_t& garbage) const
{
	assert(GetDataObjLockCount() == 0);
#if defined(MG_DEBUG)
//	static TokenID testsID = GetTokenID("tests");
//	if (m_BackRef && m_BackRef->GetID() == testsID)
//		__debugbreak();
#endif
	garbage |= std::move(m_DataObject);
	assert(!m_DataObject);

	assert(GetDataObjLockCount() == 0);
}

/* REMOVE
garbage_t AbstrDataItem::CloseData() const
{
	assert(GetDataObjLockCount() == 0);

	garbage_t garbage;
	garbage |= std::move(m_DataObject);
	assert(!m_DataObject);

	return garbage;
}
*/

void AbstrDataItem::XML_DumpData(OutStreamBase* xmlOutStr) const
{ 
	assert(GetInterestCount()); // PRECONDITION, Callers responsibility
	XML_DataBracket dataBracket(*xmlOutStr);
	GetDataObj()->XML_DumpObjData(xmlOutStr, this); 
}

SharedStr AbstrDataItem::GetSignature() const
{
	return SharedStr(HasVoidDomainGuarantee()	?	"parameter<" :	"attribute<")
		+	s_ValuesUnitPropDefPtr->GetValue(this)
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
	std::vector<NonmappableStorageManagerRef> m_ClonePtrs;
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
	assert(CheckCalculatingOrReady(GetAbstrDomainUnit()->GetCurrRangeItem()));

	auto* sm = smi->StorageManager();
	assert(sm);

	if (!sm->DoesExist(smi->StorageHolder()))
		throwItemErrorF( "Storage %s does not exist", sm->GetNameStr().c_str() );

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

			auto sharedSm = MakeShared(sm);
			auto tileGenerator = [this, sharedSm, smi, readerFarm](AbstrDataObject* self, tile_id t)
			{
				auto token = readerFarm->acquire();
				auto returnTokenOnExit = make_scoped_exit([&readerFarm, token]() { readerFarm->release(token); });

				auto& readerClonePtr = readerFarm->m_ClonePtrs[token];
				if (!readerClonePtr)
					readerClonePtr = sharedSm->ReaderClone(*smi);
				if (!readerClonePtr->ReadDataItem(smi, self, t))
					this->throwItemError("Failure during Reading from storage");
			};
			auto rangeDomainUnit = AsUnit(GetAbstrDomainUnit()->GetCurrRangeItem()); assert(rangeDomainUnit);
			auto tileRangeData = rangeDomainUnit->GetTiledRangeData();
			auto rangeValuesUnit = AsUnit(GetAbstrValuesUnit()->GetCurrRangeItem()); assert(rangeValuesUnit);
			MG_CHECK(tileRangeData);
			if (true || sm->EasyRereadTiles())
			{
				visit<typelists::numerics>(rangeValuesUnit, [this, tileRangeData, &tileGenerator]<typename V>(const Unit<V>*valuesUnit) {
					this->m_DataObject = make_unique_LazyTileFunctor<V>(this, tileRangeData, valuesUnit->m_RangeDataPtr, std::move(tileGenerator)
						MG_DEBUG_ALLOCATOR_SRC("AbstrDataItem::DoReadItem of random rereadable tiles")
					).release();
				});
			}
		}
		else
		{
			DataWriteLock readResultHolder(this);
			assert(readResultHolder.get_ptr());

			serial_for<tile_id>(0, GetAbstrDomainUnit()->GetNrTiles(),
				[sm, smi, this, &readResultHolder](tile_id t)->void
				{
					if (!sm->ReadDataItem(smi, readResultHolder.get_ptr(), t))
						throwItemError("Failure during Reading from storage");
				}
			);
			readResultHolder.Commit();
		}
		dbg_assert(currTS == LastChangeTS());
	}
	catch (const DmsException& x)
	{
		if (!WasFailed(FR_Data))
			DoFail(x.AsErrMsg(), FR_Data);
		throw;
	}
	return true;
}

bool AbstrDataItem::DoWriteItem(StorageMetaInfoPtr&& smi) const
{
	assert(CheckDataReady(GetCurrUltimateItem()));

	DataReadLock lockForSave(this);

	auto sm = smi->StorageManager();
	FencedInterestRetainContext irc;
	try {
		SharedPtr<const TreeItem> storageHolder = smi->StorageHolder();
		sm->ExportMetaInfo(storageHolder, this);
		if (sm->WriteDataItem(std::move(smi)))
		{
			reportF(MsgCategory::storage_write, SeverityTypeID::ST_MajorTrace, "%s IS STORED IN %s",
				GetSourceName().c_str()
				, sm->GetNameStr().c_str()
			);
		}
		else
			throwItemError("Failure during Writing");
		
	}
	catch (const DmsException& x)
	{
		if (!WasFailed(FR_Committed))
			DoFail(x.AsErrMsg(), FR_Committed);
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

	assert((m_tDomainUnit == domainUnit) || !IsDefined(m_tDomainUnit) || !domainUnit); // only called once?
	assert((m_tValuesUnit == valuesUnit) || !IsDefined(m_tValuesUnit) || !valuesUnit); // only called once?
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

		throwDmsErrF("No ValueType for %s composition of %s values", vcStr.c_str(), vtSingleStr.c_str());
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
	while (refItem = refAsDi->GetReferredItem())
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
			reportF(SeverityTypeID::ST_Warning, "%s: DomainUnification of categorical calculation result: %s"
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
		res->m_DomainUnit = GetAbstrDomainUnit();
		res->m_ValuesUnit = GetAbstrValuesUnit();
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
			CatchFail(FR_MetaInfo);
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
			CatchFail(FR_MetaInfo);
		}
	}
}

ValueComposition AbstrDataItem::GetValueComposition() const
{
	ValueComposition vc = m_StatusFlags.GetValueComposition();
	assert(vc != ValueComposition::Unknown);
	return vc;
}

void AbstrDataItem::LoadBlobStream (const InpStreamBuff* f)
{
	
//	assert(IsMetaThread());
	assert(m_State.GetProgress() >= PS_MetaInfo || IsPassor());
	assert(GetCurrDataObj());
	assert(!m_DataLockCount);
//	assert(IsSdKnown());

	const AbstrUnit* adu = GetAbstrDomainUnit();
	assert(adu && adu->GetInterestCount());
	const AbstrUnit* adr = AsUnit(adu->GetCurrRangeItem());
	assert(adr && adr->GetInterestCount());
	assert(CheckDataReady(adr));

	assert(CheckCalculatingOrReady(adr));

	assert(IsReadLocked(this) || !IsMultiThreaded2());
	DataWriteLock lock(const_cast<AbstrDataItem*>(this));
	//	assert(m_DataLockCount < 0);
	BinaryInpStream ar(f);

//	auto adu = GetAbstrDomainUnit()->GetTiledRangeData();
//	auto avu = GetAbstrValuesUnit()->GetTiledRangeData();
//	auto vc = GetValueComposition();
	DataWriteLock writeHandle(this);

	AbstrDataObject* ado = const_cast<AbstrDataObject*>(GetCurrDataObj());
	for (tile_id t = 0, e = GetAbstrDomainUnit()->GetNrTiles(); t != e; ++t)
		writeHandle->DoReadData(ar, t);

	lock.Commit();
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
	dbg_assert(m_LastGetStateTS == refItem->m_LastGetStateTS || refItem->IsPassor());
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
	Fail(errMsgStr, FR_Determine);
	return false;
}

const AbstrUnit* AbstrDataItem::FindUnit(TokenID t, CharPtr role, ValueComposition* vcPtr) const
{
	assert(GetTreeParent());
	if (t == TokenID::GetUndefinedID())
		ThrowFail(mySSPrintF("Undefined %s unit", role), FR_MetaInfo);
	const AbstrUnit* result = UnitClass::GetUnitOrDefault(GetTreeParent(), t, vcPtr);
	if (!result && !InTemplate())
	{
		auto msg = mySSPrintF("Cannot find %s unit %s", role, GetTokenStr(t));
		ThrowFail(msg, FR_MetaInfo);
	}
	return result;
}

void AbstrDataItem::InitDataItem(const AbstrUnit* du, const AbstrUnit* vu, const DataItemClass* dic)
{
	assert( m_StatusFlags.GetValueComposition() != ValueComposition::Unknown );
	m_DomainUnit = du;
	m_ValuesUnit = vu;
}

const AbstrDataObject* AbstrDataItem::GetDataObj() const
{
	auto dataObj = m_DataObject;
	assert(dataObj);

//	if (!dataObj)
//		throwItemError("No DataObj");
//	assert(m_DataLockCount > 0);
//	assert(m_DataObject);
/* REMOVE
	if (!m_DataObject)
	{
		assert((GetTreeParent() == nullptr) or GetTreeParent()->Was(PS_MetaInfo) or GetTreeParent()->WasFailed(FR_MetaInfo));

		MG_CHECK2(false, "TODO G8");

		if (GetTSF(TSF_DSM_SdKnown))
			m_DataObject = DataStoreManager::Curr()->LoadBlob(this, false);
		if (IsFnKnown())
			m_DataObject = DataStoreManager::Curr()->CreateFileData(const_cast<AbstrDataItem*>(this), dms_rw_mode::read_only); // , !item->IsPersistent(), true); // calls OpenFileData -> FileTileArray

		MG_CHECK(m_DataObject);
	}
*/
	return dataObj;
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

garbage_t AbstrDataItem::StopInterest() const noexcept
{
	assert(GetInterestCount() == 0);

	garbage_t garbage;
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
	const AbstrDataObject* ado = GetDataObj();
	DataCheckMode dcm = ado->DoGetCheckMode();

	assert(!GetTSF(DSF_ValuesChecked)); // NO CONCURRENCY
	m_StatusFlags.SetDataCheckMode(dcm);
}

DataCheckMode AbstrDataItem::DetermineRawCheckModeImpl() const
{
	const AbstrDataObject* ado = GetDataObj();
	return ado->DoDetermineCheckMode();
}

typedef cs_lock_map<const AbstrDataItem*> data_flags_lock_map;
data_flags_lock_map sg_DataFlagsLockMap("DataItemFlags");

DataCheckMode AbstrDataItem::GetRawCheckMode() const
{
	dbg_assert(CheckMetaInfoReadyOrPassor());
	MG_LOCKER_NO_UPDATEMETAINFO

	const AbstrDataItem* adi = debug_cast<const AbstrDataItem*>(GetCurrUltimateItem()); 
	assert(adi);
	assert(CheckDataReady(adi));

	assert(adi->GetDataObjLockCount() > 0 );

	if (!adi->GetTSF(DSF_ValuesChecked))
	{
		dbg_assert(IsMultiThreaded2() || !gd_nrActiveLoops);
		assert(IsMetaThread() || IsMultiThreaded2());
		if (IsMultiThreaded2())
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
	
	const AbstrDataItem* adi = debug_cast<const AbstrDataItem*>(GetCurrUltimateItem());
	assert(adi);
	assert(CheckDataReady(adi));

	assert(adi->GetDataObjLockCount() > 0);

	dbg_assert(IsMultiThreaded2() || !gd_nrActiveLoops);
	assert(IsMetaThread() || IsMultiThreaded2());
	if (IsMultiThreaded2())
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
	{
		this->throwItemError("Invalid domain reference.");
	}
	return adu->IsKindOf( Unit<Void>::GetStaticClass() );
}

void AbstrDataItem::OnDomainUnitRangeChange(const DomainChangeInfo* info)
{
//	MG_CHECK2(false, "NYI: Copy Data into newly formed DataArray");
	if (mc_Calculator ? mc_Calculator->IsDataBlock() : m_DataObject)
	{
		// is info->oldRangeData nog "actief" ? "actief" <-> Actor <-> TimeStamp of land change <-!-> Value Bases Calculation <-> declarative modelling
		try {
			assert(!GetDataObjLockCount());
			auto oldDataObject = GetDataObj();
			
			DataWriteLock lock(this); // calls CreateAbstrHeapTileFunctor(); is dan nu ineens info->newDataRange "actief" ?
			CopyData(oldDataObject, lock.get(), info); // can I reuse tiles ?
			lock.Commit();
			assert(!mc_Calculator); // DataWriteLock::Commit() destroyed DataBlockTask
		}
		catch (DmsException& x)
		{
			DoFail(x.AsErrMsg(), FR_Data);
		}
	}
}

// called when InterestCount drops to 0 or KeepData went to false 
bool AbstrDataItem::TryCleanupMemImpl(garbage_t& garbageCan) const
{
	MG_LOCKER_NO_UPDATEMETAINFO

	// copied from TreeItem::TryCleanupMemImpl, TODO G8: Reorder logic and avoid double code
	if (PartOfInterestOrKeep())
		return false;

	if (m_ItemCount < 0)
		return false;

	//ClearTSF(TSF_DataInMem);
	assert(!GetTSF(TSF_DataInMem));

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
// TODO G8: Consider merging with ClearData 
garbage_t AbstrDataItem::CleanupMem(bool hasSourceOrExit, std::size_t minNrBytes) noexcept
{
	MG_LOCKER_NO_UPDATEMETAINFO

	assert(m_DataObject);
	// Drop Composite from root when Out Of Interest
	garbage_t garbageCan;
	if (hasSourceOrExit && !GetKeepDataState())
		garbageCan |= DropValue(); // calls ClearData

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
		assert(IsDefined(item->m_tDomainUnit));
		if (item->m_tDomainUnit)
			return SharedStr(item->m_tDomainUnit);

		return  item->GetAbstrDomainUnit()->GetScriptName(item);
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
		assert(IsDefined(item->m_tValuesUnit));
		if (item->m_tValuesUnit)
			return SharedStr(item->m_tValuesUnit);

		return item->GetAbstrValuesUnit()->GetScriptName(item);
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
		readLocks.push_back(PreparedDataReadLock(columnSpecIter->m_DataItem));
		if (columnSpecIter->m_RelativeDisplay)
			columnSpecIter->m_ColumnTotal = columnSpecIter->m_DataItem->GetRefObj()->GetSumAsFloat64();
	}
	std::vector<OwningPtr<const AbstrReadableTileData>> tileLocks; tileLocks.reserve(nrDataItems);
	for (const auto& drl : readLocks)
		tileLocks.emplace_back(drl.GetRefObj()->CreateReadableTileData(no_tile));

	FormattedOutStream fout(out, FormattingFlags::None);
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
		adi = AsDataItem(adi->GetCurrRefItem());
		if (!adi)
			return nullptr;
	}
}

//----------------------------------------------------------------------
// Building blocks for LazyTileFunctor heristics
//----------------------------------------------------------------------

inline UInt32 ElementWeight(const AbstrDataItem* adi)
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

UInt32 LTF_ElementWeight(const AbstrDataItem* adi)
{
	return 0;
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
		reportF(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "%x LVL %d IC %d KD %s SI %s; %s %s %s: %s", focusItem,  level,
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
				ReportTree(done, tidr->m_Data, level, "CACHEDATA");
#endif
		}
		if (ti)
		{

			if (ti->IsCacheItem())
				ReportTree(done, ti->GetTreeParent(), level, "PARENT");
			ReportTree(done, ti->mc_RefItem, level, "REF_ITEM");
			ReportTree(done, ti->mc_DC, level, "CALC");
	

			if (IsDataItem(focusItem))
			{
				SharedStr name( focusItem->GetFullName().c_str());
				CDebugContextHandle debugContext2("ReportTreeUnits", name.c_str(), true);
				const AbstrDataItem* adi = AsDataItem(focusItem);
				if (adi->HasDataObj())
				{
					ReportTree(done, adi->GetAbstrDomainUnit(), level, "DOMAIN");
					ReportTree(done, adi->GetAbstrValuesUnit(), level, "VALUES");
				}
			}
		}

		if (focusItem->DoesHaveSupplInterest() && s_SupplTreeInterest)
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
				ReduceInterest(interestRoots, tidr->m_Data);
			return;
		}

		const TreeItem* ti = dynamic_cast<const TreeItem*>(focusItem);
		if (!ti)
			return;
//		if (ti->IsCacheItem())
		ReduceInterest(interestRoots, ti->GetTreeParent());
		ReduceInterest(interestRoots, ti->mc_RefItem);
		ReduceInterest(interestRoots, ti->mc_DC);
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

		reportF(SeverityTypeID::ST_MajorTrace, "#Items with interest: %d", DemandManagement::sd_InterestSet.size());
		reportF(SeverityTypeID::ST_MajorTrace, "sum #Interest:        %d", totalInterestCount);
		reportF(SeverityTypeID::ST_MajorTrace, "#reduced interest:    %d", reducedInterest);
		reportF(SeverityTypeID::ST_MajorTrace, "sum reduced #Interest:%d", reducedInterestCount);

/* Too much, leave it for now
* 
		ActorSet done;
		for (const auto& ii: interestRoots)
			if (ii.second)
				ReportTree(done, ii.first, 0,  "ROOT");
		DMS_TRACE(("%d done", done.size()));

*/
	}
};

InterestReporter sd_InterestSetReporter;


#endif // defined(MG_DEBUG_INTERESTSOURCE)
