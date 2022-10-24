//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#include "TicPCH.h"
#pragma hdrstop

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
	dms_assert(!GetInterestCount());
	dms_assert(!GetRefCount());
	SetKeepDataState(false);
	if (m_DataObject)
		CleanupMem(true, 0);

	if (!IsEndogenous())
		if (m_DomainUnit) m_DomainUnit->DelDataItemOut(this);

	m_DomainUnit.reset();
	m_ValuesUnit.reset();

	dms_assert(m_DataLockCount == 0);
}

//----------------------------------------------------------------------
// class  : AbstrDataItem (inline) functions that forward to DataObject
//----------------------------------------------------------------------

inline const AbstrUnit* AbstrDataItem::GetAbstrDomainUnit() const 
{ 
	if (!m_DomainUnit && IsMainThread())
		m_DomainUnit = FindUnit(m_tDomainUnit, "Domain", nullptr);
	return m_DomainUnit;
}
inline const AbstrUnit*  AbstrDataItem::GetAbstrValuesUnit() const 
{ 
	if (!m_ValuesUnit && IsMainThread())
	{
		ValueComposition vc = GetValueComposition();
		m_ValuesUnit = FindUnit(m_tValuesUnit, "Values", &vc);
	}
	return m_ValuesUnit;
}

inline const AbstrDataObject* AbstrDataItem::GetCurrRefObj()      const 
{
	dbg_assert(CheckMetaInfoReadyOrPassor());

	return debug_cast<const AbstrDataItem*>(GetCurrUltimateItem())->GetDataObj();
}

inline const AbstrDataObject* AbstrDataItem::GetRefObj()          const 
{
	dms_assert(IsMainThread());
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
	dms_assert(GetDataObjLockCount() == 0);
#if defined(MG_DEBUG)
//	static TokenID testsID = GetTokenID("tests");
//	if (m_BackRef && m_BackRef->GetID() == testsID)
//		__debugbreak();
#endif
	garbage |= std::move(m_DataObject);
	dms_assert(!m_DataObject);

	dms_assert(GetDataObjLockCount() == 0);
}

/* REMOVE
garbage_t AbstrDataItem::CloseData() const
{
	dms_assert(GetDataObjLockCount() == 0);

	garbage_t garbage;
	garbage |= std::move(m_DataObject);
	dms_assert(!m_DataObject);

	return garbage;
}
*/

void AbstrDataItem::XML_DumpData(OutStreamBase* xmlOutStr) const
{ 
	dms_assert(GetInterestCount()); // PRECONDITION, Callers responsibility
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

bool AbstrDataItem::DoReadItem(StorageMetaInfo* smi)
{
	dms_assert(CheckCalculatingOrReady(GetAbstrDomainUnit()->GetCurrRangeItem()));

	AbstrStorageManager* sm = smi->StorageManager();
	dms_assert(sm);

	if (!sm->DoesExist(smi->StorageHolder()))
		throwItemErrorF( "Storage %s does not exist", sm->GetNameStr().c_str() );

	DataWriteLock readResultHolder(this);

	try {
		MG_DEBUGCODE(TimeStamp currTS = LastChangeTS(); )

		serial_for<tile_id>(0, GetAbstrDomainUnit()->GetNrTiles(),
			[sm, smi, this, &readResultHolder](tile_id t)->void
			{
				if (! sm->ReadDataItem(*smi, readResultHolder.get_ptr(), t))
					throwItemError("Failure during Reading from storage");
			}
		);
		dbg_assert(currTS == LastChangeTS() );
	}
	catch (const DmsException& x)
	{
		if (!WasFailed(FR_Data))
			DoFail(x.AsErrMsg(), FR_Data);
		throw;
	}
	readResultHolder.Commit();
	return true;
}

bool AbstrDataItem::DoWriteItem(StorageMetaInfoPtr&& smi) const
{
	dms_assert(CheckDataReady(GetCurrUltimateItem()));

	DataReadLock lockForSave(this);

	AbstrStorageManager* sm = smi->StorageManager();
	reportF(SeverityTypeID::ST_MajorTrace, "%s IS STORED IN %s",
		GetSourceName().c_str()
	,	sm->GetNameStr().c_str()
	);

	FencedInterestRetainContext irc;
	try {
		SharedPtr<const TreeItem> storageHolder = smi->StorageHolder();
		if (!sm->WriteDataItem(std::move(smi)))
			throwItemError("Failure during Writing");
		sm->ExportMetaInfo(storageHolder, this);
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

	dms_assert((m_tDomainUnit == domainUnit) || !IsDefined(m_tDomainUnit) || !domainUnit); // only called once?
	dms_assert((m_tValuesUnit == valuesUnit) || !IsDefined(m_tValuesUnit) || !valuesUnit); // only called once?
//	dms_assert(!m_DataObject || (!valuesUnit && !domainUnit));             // and before it resulted in further construction

	m_tDomainUnit = domainUnit;
	m_tValuesUnit = valuesUnit;

	m_StatusFlags.SetValueComposition(vc);
}

const DataItemClass* AbstrDataItem::GetDynamicObjClass() const
{
	auto avu = GetAbstrValuesUnit();
	auto vc = GetValueComposition();
	auto vt = avu->GetUnitClass()->GetValueType(vc);
	auto dic = DataItemClass::FindCertain(vt, this);
	return dic;
}

const Class* AbstrDataItem::GetCurrentObjClass() const
{
	return HasDataObj()
		?	GetDataObj()->GetDynamicClass()
		:	GetDynamicClass();
}

void AbstrDataItem::Unify(const TreeItem* refItem) const
{
	const AbstrDataItem* refAsDi = AsDataItem(refItem);
	GetAbstrDomainUnit()->UnifyDomain(refAsDi->GetAbstrDomainUnit(), UM_Throw);
	while (refItem = refAsDi->GetReferredItem())
	{
		refAsDi->Unify(refItem);
		refAsDi = AsDataItem(refItem);
	}
	GetAbstrValuesUnit()->UnifyValues(refAsDi->GetAbstrValuesUnit(), UnifyMode(UM_AllowDefaultLeft|UM_Throw));
}

/* REMOVE
LispRef AbstrDataItem::GetKeyExprImpl() const
{
	auto result = TreeItem::GetKeyExprImpl();
	if (!result.EndP())
		return result;
	return {};
}
*/

void AbstrDataItem::CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const
{
	TreeItem::CopyProps(result, copyContext);

	auto res = debug_cast<AbstrDataItem*>(result);

	// only copy unitnames when not defined
	if (copyContext.MustCopyExpr() || !IsDefined(res->m_tDomainUnit))
	{
		try {
			auto adu = GetAbstrDomainUnit();
			res->m_tDomainUnit = copyContext.GetAbsOrRelUnitID(adu, this, res);
		}
		catch (...)
		{
			CatchFail(FR_MetaInfo);
			res->m_tDomainUnit = m_tDomainUnit;
		}
	}
	if (copyContext.MustCopyExpr() || !IsDefined(res->m_tValuesUnit))
	{
		try {
			auto avu = GetAbstrValuesUnit();
			res->m_tValuesUnit = copyContext.GetAbsOrRelUnitID(avu, this, res);
		}
		catch (...)
		{
			CatchFail(FR_MetaInfo);
			res->m_tValuesUnit = m_tValuesUnit;
		}
	}
	res->m_StatusFlags.SetValueComposition(GetValueComposition());
}

ValueComposition AbstrDataItem::GetValueComposition() const
{
	ValueComposition vc = m_StatusFlags.GetValueComposition();
	dms_assert(vc != ValueComposition::Unknown);
	return vc;
}

void AbstrDataItem::LoadBlobStream (const InpStreamBuff* f)
{
	
//	dms_assert(IsMainThread());
	dms_assert(m_State.GetProgress() >= PS_MetaInfo || IsPassor());
	dms_assert(GetCurrDataObj());
	dms_assert(!m_DataLockCount);
//	dms_assert(IsSdKnown());

	const AbstrUnit* adu = GetAbstrDomainUnit();
	dms_assert(adu && adu->GetInterestCount());
	const AbstrUnit* adr = AsUnit(adu->GetCurrRangeItem());
	dms_assert(adr && adr->GetInterestCount());
	dms_assert(CheckDataReady(adr));

	dms_assert(CheckCalculatingOrReady(adr));

	dms_assert(IsReadLocked(this) || !IsMultiThreaded2());
	DataWriteLock lock(const_cast<AbstrDataItem*>(this));
	//	dms_assert(m_DataLockCount < 0);
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
	dms_assert(GetCurrDataObj());
	dms_assert(!GetAbstrDomainUnit()->IsCurrTiled());

	BinaryOutStream out(f);
	for (tile_id t = 0, e = GetAbstrDomainUnit()->GetNrTiles(); t != e; ++t)
		GetCurrDataObj()->DoWriteData(out, t);
}

bool AbstrDataItem::CheckResultItem(const TreeItem* refItem) const
{
	dms_assert(refItem);
	if (!base_type::CheckResultItem(refItem))
		return false;
	const AbstrDataItem* adi = AsDataItem(refItem);

	SharedStr resultMsg;
	CharPtr issueStr;
	{
		auto mydu = GetAbstrDomainUnit(); mydu->UpdateMetaInfo();
		auto refdu = adi->GetAbstrDomainUnit(); refdu->UpdateMetaInfo();
		if (!mydu->UnifyDomain(refdu, UnifyMode::UM_AllowDefaultLeft, &resultMsg))
		{
			issueStr = "Domain";
			goto failResultMsg;
		}
	}
	dbg_assert(m_LastGetStateTS == refItem->m_LastGetStateTS || refItem->IsPassor());
	{
		auto myvu = GetAbstrValuesUnit(); myvu->UpdateMetaInfo();
		auto refvu = adi->GetAbstrValuesUnit(); refvu->UpdateMetaInfo();
		if (GetAbstrValuesUnit()->UnifyValues(refvu, UM_AllowDefault, &resultMsg))
			return true;
		issueStr = "ValuesUnit ";
	}

failResultMsg:
	auto msg = mySSPrintF("%s is incompatible with the result of the calculation '%s'\nbecause %s"
	,	issueStr
	,	AsFLispSharedStr(GetAsLispRef(GetCurrMetaInfo({})))
	,	resultMsg
	);
	Fail(msg, FR_Determine);
	return false;
}

const AbstrUnit* AbstrDataItem::FindUnit(TokenID t, CharPtr role, ValueComposition* vcPtr) const
{
	dms_assert(GetTreeParent());
	if (t == TokenID::GetUndefinedID())
		ThrowFail(mySSPrintF("Undefined %s unit", role), FR_MetaInfo);
	const AbstrUnit* result = UnitClass::GetUnitOrDefault(GetTreeParent(), t, vcPtr);
	if (!result)
	{
		auto msg = mySSPrintF("Cannot find %s unit %s", role, GetTokenStr(t));
		ThrowFail(msg, FR_MetaInfo);
	}
	return result;
}

void AbstrDataItem::InitDataItem(const AbstrUnit* du, const AbstrUnit* vu, const DataItemClass* dic)
{
	dms_assert( m_StatusFlags.GetValueComposition() != ValueComposition::Unknown );
	m_DomainUnit = du;
	m_ValuesUnit = vu;
}

const AbstrDataObject* AbstrDataItem::GetDataObj() const
{
//	dms_assert(m_DataLockCount > 0);
	dms_assert(m_DataObject);
/* REMOVE
	if (!m_DataObject)
	{
		dms_assert((GetTreeParent() == nullptr) or GetTreeParent()->Was(PS_MetaInfo) or GetTreeParent()->WasFailed(FR_MetaInfo));

		MG_CHECK2(false, "TODO G8");

		if (GetTSF(TSF_DSM_SdKnown))
			m_DataObject = DataStoreManager::Curr()->LoadBlob(this, false);
		if (IsFnKnown())
			m_DataObject = DataStoreManager::Curr()->CreateFileData(const_cast<AbstrDataItem*>(this), dms_rw_mode::read_only); // , !item->IsPersistent(), true); // calls OpenFileData -> FileTileArray

		MG_CHECK(m_DataObject);
	}
*/
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
	dms_assert(GetInterestCount()==0);

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
	dms_assert(GetInterestCount() == 0);

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
	dms_assert(!GetTSF(DSF_ValuesChecked)); // PRECONDITION
	const AbstrDataObject* ado = GetDataObj();
	DataCheckMode dcm = ado->DoGetCheckMode();

	dms_assert(!GetTSF(DSF_ValuesChecked)); // NO CONCURRENCY
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
	dms_assert(adi);
	dms_assert(CheckDataReady(adi));

	dms_assert(adi->GetDataObjLockCount() > 0 );

	if (!adi->GetTSF(DSF_ValuesChecked))
	{
		dbg_assert(IsMultiThreaded2() || !gd_nrActiveLoops);
		dms_assert(IsMainThread() || IsMultiThreaded2());
		if (IsMultiThreaded2())
		{
			data_flags_lock_map::ScopedLock localLock(MG_SOURCE_INFO_CODE("AbstrDataItem::GetRawCheckMode") sg_DataFlagsLockMap, adi);
			if (!adi->GetTSF(DSF_ValuesChecked))
				adi->GetRawCheckModeImpl();
		}
		else
		{
			dms_assert(IsMainThread());
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
	dms_assert(adi);
	dms_assert(CheckDataReady(adi));

	dms_assert(adi->GetDataObjLockCount() > 0);

	dbg_assert(IsMultiThreaded2() || !gd_nrActiveLoops);
	dms_assert(IsMainThread() || IsMultiThreaded2());
	if (IsMultiThreaded2())
	{
		data_flags_lock_map::ScopedLock localLock(MG_SOURCE_INFO_CODE("AbstrDataItem::GetRawCheckMode") sg_DataFlagsLockMap, adi);
		return adi->DetermineRawCheckModeImpl();
	}
	else
	{
		dms_assert(IsMainThread());
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
		dms_assert(t == no_tile || t == 0);
		return GetCheckMode();
	}
}

bool AbstrDataItem::HasVoidDomainGuarantee() const
{
	return GetAbstrDomainUnit()->IsKindOf( Unit<Void>::GetStaticClass() );
}

void AbstrDataItem::OnDomainUnitRangeChange(const DomainChangeInfo* info)
{
//	MG_CHECK2(false, "NYI: Copy Data into newly formed DataArray");
	if (mc_Calculator ? mc_Calculator->IsDataBlock() : m_DataObject)
	{
		// is info->oldRangeData nog "actief" ? "actief" <-> Actor <-> TimeStamp of land change <-!-> Value Bases Calculation <-> declarative modelling
		try {
			dms_assert(!GetDataObjLockCount());
			auto oldDataObject = GetDataObj();
			
			DataWriteLock lock(this); // calls CreateAbstrHeapTileFunctor(); is dan nu ineens info->newDataRange "actief" ?
			CopyData(oldDataObject, lock.get(), info); // can I reuse tiles ?
			lock.Commit();
			dms_assert(!mc_Calculator); // DataWriteLock::Commit() destroyed DataBlockTask
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
	dms_assert(!GetTSF(TSF_DataInMem));

	dms_assert(!GetDataObjLockCount());
	dms_assert(!PartOfInterest());

	if (GetDataRefLockCount())
		return false;

	if (!m_DataObject)
		return false;

	dms_assert(!PartOfInterestOrKeep());

	if (m_DataObject->IsMemoryObject() && m_DataObject->IsSmallerThan(KEEPMEM_MAX_NR_BYTES)) // TODO G8: Consider leaning on CleanupMem; is the same if applied twice ?
		return true;

	bool hasSource = !HasCurrConfigData();
//	dms_assert(!hasSource || IsCacheItem() || GetCurrStorageParent(false) || mc_Calculator)
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

	dms_assert(m_DataObject);
	// Drop Composite from root when Out Of Interest
	garbage_t garbageCan;
	if (hasSourceOrExit && !GetKeepDataState())
		garbageCan |= DropValue(); // calls ClearData

	return garbageCan;
}

//	override virtuals of Actor
ActorVisitState AbstrDataItem::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (Test(svf, SupplierVisitFlag::DomainValues)) // already done by StartInterest
	{
		if (visitor.Visit(GetAbstrDomainUnit()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
		if (visitor.Visit(GetAbstrValuesUnit()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
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
		dms_assert(IsDefined(item->m_tDomainUnit));
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
		dms_assert(IsDefined(item->m_tValuesUnit));
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
		return UnitName(item->GetAbstrDomainUnit());
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
		return UnitName(item->GetAbstrValuesUnit());
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

/* REMOVE
TIC_CALL void DMS_CONV DMS_DataItem_CopyData(AbstrDataItem* dest, const AbstrDataItem* source)
{
	DMS_CALL_BEGIN

		CheckPtr(dest,   AbstrDataItem::GetStaticClass(), "DMS_DataItem_CopyData");
		CheckPtr(source, AbstrDataItem::GetStaticClass(), "DMS_DataItem_CopyData");


		source->GetAbstrDomainUnit()->UnifyDomain(dest->GetAbstrDomainUnit(), UnifyMode(UM_Throw| UM_AllowAllEqualCount));
		source->GetAbstrValuesUnit()->UnifyValues(dest->GetAbstrValuesUnit(), UnifyMode(UM_Throw));

		ExternalVectorOutStreamBuff::VectorType dataBuff;
		PreparedDataReadLock sourceLock(source);
		DataWriteLock        destLock(dest);
		for (tile_id t = source->GetAbstrDomainUnit()->GetNrTiles(); t--; )
		{
			dataBuff.clear();
			{
				ExternalVectorOutStreamBuff streamToBuff(dataBuff);
				BinaryOutStream os(&streamToBuff);
				source->GetRefObj()->DoWriteData(os, t);
			}
			{
				MemoInpStreamBuff streamFromBuff(begin_ptr(dataBuff), end_ptr(dataBuff));
				BinaryInpStream is(&streamFromBuff);
				destLock->DoReadData(is, t);
			}
		}
		destLock.Commit();
	DMS_CALL_END
}
REMOVE */
TIC_CALL void DMS_CONV Table_Dump(OutStreamBuff* out, const ConstAbstrDataItemPtr* dataItemArray, const ConstAbstrDataItemPtr* dataItemArrayEnd, const SizeT* recNos, const SizeT* recNoEnd)
{
	SizeT nrDataItems = dataItemArrayEnd - dataItemArray;
	if (!nrDataItems)
		return;

	const AbstrUnit* domain = dataItemArray[0]->GetAbstrDomainUnit();
	for (const ConstAbstrDataItemPtr* dataItemIter = dataItemArray + 1; dataItemIter != dataItemArrayEnd; ++dataItemIter)
		domain->UnifyDomain((*dataItemIter)->GetAbstrDomainUnit(), UM_Throw);

	std::vector<SharedDataItemInterestPtr> keepInterest; keepInterest.reserve(nrDataItems);
	for (const ConstAbstrDataItemPtr* dataItemIter = dataItemArray; dataItemIter != dataItemArrayEnd; ++dataItemIter)
		keepInterest.emplace_back(*dataItemIter);

	DataReadLockContainer readLocks; readLocks.reserve(nrDataItems);
	for (const ConstAbstrDataItemPtr* dataItemIter = dataItemArray; dataItemIter != dataItemArrayEnd; ++dataItemIter)
		readLocks.push_back(PreparedDataReadLock(*dataItemIter));

	std::vector<OwningPtr<const AbstrReadableTileData>> tileLocks; keepInterest.reserve(nrDataItems);
	for (const auto& drl : readLocks)
		tileLocks.emplace_back(drl.GetRefObj()->CreateReadableTileData(no_tile));

	FormattedOutStream fout(out, FormattingFlags::None);
	for (const ConstAbstrDataItemPtr* dataItemIter = dataItemArray; dataItemIter != dataItemArrayEnd; ++dataItemIter)
	{
		if (dataItemIter != dataItemArray)
			out->WriteByte(';');
		SharedStr themeDisplName = GetDisplayNameWithinContext(*dataItemIter, true, [dataItemArray, dataItemArrayEnd]() mutable -> const AbstrDataItem*
			{
				if (dataItemArray == dataItemArrayEnd)
					return nullptr;
				return *dataItemArray++;
			}
		);
		DoubleQuote(fout, themeDisplName);
	}
	out->WriteByte('\n');

	SizeT nrRows = recNos ? (recNoEnd - recNos) : domain->GetCount();

	SizeT nrCols = tileLocks.size();
	for (SizeT i = 0; i != nrRows; ++i) {
		SizeT recNo = (recNos) ? *recNos++ : i;

		for (SizeT j = 0; j != nrCols; ++j) {
			if (j)
				out->WriteByte(';');
			tileLocks[j]->WriteFormattedValue(fout, recNo);
		}
		out->WriteByte('\n');
	}
}

TIC_CALL void DMS_CONV DMS_Table_Dump(OutStreamBuff* out, UInt32 nrDataItems, const ConstAbstrDataItemPtr* dataItemArray)
{
	DMS_CALL_BEGIN

		Table_Dump(out, dataItemArray, dataItemArray + nrDataItems, nullptr, nullptr);

	DMS_CALL_END
}

TIC_CALL const Class* DMS_CONV DMS_AbstrDataItem_GetStaticClass()
{
	return AbstrDataItem::GetStaticClass();
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

		dms_assert(focusItem->GetInterestCount());

		const TreeItem* ti = dynamic_cast<const TreeItem*>(focusItem);

		auto donePtr = done.find(focusItem);
		bool wasDone = (donePtr != done.end() && *donePtr == focusItem);

#if defined(MG_DEBUG_DCDATA)
		auto dc = dynamic_cast<const DataController*>(focusItem);
		reportF(SeverityTypeID::ST_MinorTrace, "%x LVL %d IC %d KD %s SI %s; %s %s %s: %s", focusItem,  level,
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
/*
		const AbstrCalculator* ac = dynamic_cast<const AbstrCalculator*>(focusItem);
		if (ac)
		{
			if (ac->m_DcRef)
				ReportTree(done, ac->m_DcRef, "DC_REF");
		}
*/
		if (ti)
		{
//			return;  // DEBUG

			if (ti->IsCacheItem())
				ReportTree(done, ti->GetTreeParent(), level, "PARENT");
			ReportTree(done, ti->mc_RefItem, level, "REF_ITEM");
			ReportTree(done, ti->mc_DC, level, "CALC");
//			ReportTree(done, ti->mc_IntegrityChecker, "ICHECK");
	

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
		dms_assert(focusItem);
		dms_assert(focusItem->GetInterestCount());

		if (focusItem->DoesHaveSupplInterest() && s_SupplTreeInterest)
		{
			auto supplPtr = s_SupplTreeInterest->find(focusItem);
			if (supplPtr != s_SupplTreeInterest->end())
			{
				dms_assert(supplPtr->first == focusItem);
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
		dms_assert(asPtr != as.end());
		dms_assert(asPtr->first == a);
		dms_assert(asPtr->second > 0);
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
