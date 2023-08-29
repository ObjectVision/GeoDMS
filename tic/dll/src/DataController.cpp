#include "TicPCH.h"
#pragma hdrstop

#include "DataController.h"

#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/SeverityType.h"
#include "ser/AsString.h"
#include "utl/mySPrintF.h"

#include "LispRef.h"

#include "AbstrCalculator.h"
#include "DataStoreManagerCaller.h"
#include "ItemLocks.h"
#include "LispTreeType.h"
#include "OperationContext.h"

// *****************************************************************************
//										TLispRefTreeItemMap
// *****************************************************************************


/* ISSUES
- Explicit Supplier Items
  Voorbeeld: a: maak en bewaar data in externe storage x; b: result of externe processing van x
- Parents
- Compound Results
- Unit props
- Lookahead locks
  zijn future data request counts die beginnen bij een 
  DMS_TreeItem_Updata (in pre-update fase) of al eerder,
  die geplaatst worden op een DataController, 
  die ze doorplaatst bij z'n args indien deze invalide is.
  na gebruik van het arg (na berekening van de data van this),
  wordt de data weer afgelaagd; bij 0 wordt de data geflushd
  Lookahead locks betreffen interesse in de data, te onderscheiden van managed actors, 
  die interesse in het up-to-date van de gehele item-state betreft (incl. externe storage).
- Read Locks / Write Locks. Moeten tijdelijk zijn (thread-local).
- Flushen of storen.
  gezien de hoge kosten van opslaan, is free-en een optie indien
  de opvraagfrequentie gering of de rekentijd niet al te hoog is en de suppliers beschikbaar.

  Dit is een moeilijke afweging. 
  Vooralsnog moet dit per item geconfigureerd worden.
  Alle virtuele items hebben geen storage
- Invalidation by datachange, this->exprchange, supplier->exprchange, etc.
  Doe in idletime van alle up-to date dingen een state check en update vervolgens alle (zojuist) geinvalideerde items
*/


// *****************************************************************************
// Section:     TreeItemDualRef Implementation
// *****************************************************************************

TreeItemDualRef::TreeItemDualRef()
{}

TreeItemDualRef::~TreeItemDualRef()
{
	Clear();
	dbg_assert(!m_State.Get(DCFD_DataCounted));
//	dms_assert(!m_State.Get(DCF_CacheRootKnown|DCF_DSM_SmallDataKnown));
}

void TreeItemDualRef::Set(const TreeItem* ti, bool isNew)
{
	if (ti && m_Data != ti)
	{
		dms_assert(IsMetaThread());
		dms_assert(!m_State.Get(DCF_IsOld|DCF_IsTmp));

		if (GetInterestCount() && m_Data)
			DecDataInterestCount();

		m_Data = ti;

		if (isNew)
			const_cast<TreeItem*>(ti)->SetIsCacheItem();
		else
			m_State.Set(DCF_IsOld);


		if (GetInterestCount())
			IncDataInterestCount();

		dms_assert(!m_State.Get(DCF_CacheRootKnown));
	}
	dms_assert(!ti || GetOld() == ti);
}

void TreeItemDualRef::SetNew(TreeItem* newTI)
{
	Set(newTI, true);
}

void TreeItemDualRef::SetOld(const TreeItem* oldTI)
{
	Set(oldTI, false);
}

void TreeItemDualRef::SetTmp(TreeItem* res)
{
	dms_assert(res);
	if (!m_Data)
	{
		dms_assert(!m_State.Get(DCF_IsOld|DCF_IsTmp));
		m_Data = res;
		m_State.Set(DCF_IsTmp);
	}
	dms_assert(GetNew() == res);
}

void TreeItemDualRef::Clear()
{
	if (m_Data)
	{
		if (!m_State.Get(DCF_IsTmp))
		{
			if (GetInterestCount())
				DecDataInterestCount();
			if (!m_State.Get(DCF_IsOld))
				const_cast<TreeItem*>(m_Data.get_ptr())->EnableAutoDelete();
		}
		m_Data = nullptr;
	}
	m_State.Clear(DCF_IsOld|DCF_IsTmp);
}

void TreeItemDualRef::DoInvalidate() const
{
	const_cast<TreeItemDualRef*>(this)->Clear();
	dms_assert(!m_Data); // dropped by Clear!
	Actor::DoInvalidate();
}

bool TreeItemDualRef::DoFail(ErrMsgPtr msg, FailType ft) const
{
	if (!Actor::DoFail(msg, ft))
		return false;
	if (IsNew())
		GetNew()->DoFail(msg, ft);
	return true;
}

void TreeItemDualRef::IncDataInterestCount() const
{
	dbg_assert(!m_State.Get(DCFD_DataCounted));
	m_Data->IncInterestCount();
	MG_DEBUGCODE( m_State.Set(DCFD_DataCounted));
}

garbage_t TreeItemDualRef::DecDataInterestCount() const
{
	dbg_assert( m_State.Get(DCFD_DataCounted));
	auto result = m_Data->DecInterestCount();
	MG_DEBUGCODE( m_State.Clear(DCFD_DataCounted));
/*
	result |= std::move(m_Data);
	m_State.Clear(DCF_IsOld | DCF_IsTmp);
	dms_assert(!m_Data);
*/
	return result;
}

void TreeItemDualRef::StartInterest() const
{
	dms_assert(!GetInterestCount());
	Actor::StartInterest();

	if (m_Data && !IsTmp())
	try {
		IncDataInterestCount();
	}
	catch (...)
	{
		Actor::StopInterest();
		throw;
	}
}

garbage_t TreeItemDualRef::StopInterest() const noexcept
{
	garbage_t garbage;
	if (m_Data && !IsTmp())
		garbage |= DecDataInterestCount();
	garbage |= Actor::StopInterest();
	return garbage;
}

/********** DataControllerContextHandle **********/

void DataControllerContextHandle::GenerateDescription()
{
	SetText(
		mySSPrintF("Called from the DataController for (in sLisp): %s",
			AsString(m_DC->GetLispRef()).c_str()
		)
	);
}
// *****************************************************************************
// Section:     DataController Factory
// *****************************************************************************

#include "MoreDataControllers.h"
#include "LispList.h"
#include "LispTreeType.h"

namespace {

	DataControllerRef CreateDC(LispPtr keyExpr)
	{

#if defined(MG_DEBUG_LISP_TREE)
		reportD(SeverityTypeID::ST_MinorTrace, "===CreateDC===");
		reportD(SeverityTypeID::ST_MinorTrace, AsString(keyExpr).c_str());
		dms_assert(IsExpr(keyExpr));
#endif
		assert(!keyExpr.EndP());
		MG_CHECK(!keyExpr.EndP());

		if (keyExpr.IsList())
		{
			LispPtr head = keyExpr.Left();

			dms_assert(head.IsSymb());

			if (head.GetSymbID() == token::sourceDescr)
			{
				head = keyExpr.Right().Left();
#if defined(MG_DEBUG_LISP_TREE)
				reportF(SeverityTypeID::ST_MinorTrace, "head=%s", AsString(head).c_str());
#endif
				dms_assert(head.IsSymb());

				// Is it obvious that keyExpr describes the item that will be found as head.GetSymbID() ? 
				// Yes it is: only expressions that have been generated from the current config get evaluated.
				// CalcCache entry descriptions are passive which are used to match valid requests for cached data
				return new SymbDC(keyExpr, head.GetSymbID() ); 
			}
			const AbstrOperGroup* og = AbstrOperGroup::FindName(head->GetSymbID());
			dms_assert(og->MustCacheResult());
//			if (og->MustCacheResult())
			return new FuncDC(keyExpr, og);
/* REMOVE
			else if (og->IsTemplateCall())
				return new TemplDC(keyExpr, og);
			else
				return new CompoundDC(keyExpr, og);
*/
		}
		else if (keyExpr.IsSymb())
			return new SymbDC(keyExpr, keyExpr.GetSymbID());
		else if (keyExpr.IsStrn())
			return new StringDC(keyExpr);
		else if (keyExpr.IsNumb())
		{
			dms_assert(keyExpr.IsNumb());
			return new NumbDC(keyExpr);
		}
		else
		{
			assert(keyExpr.IsUI64());
			return new UI64DC(keyExpr);
		}
	}
}	// anonymous namespace

// *****************************************************************************
// Section:     DataController Implementation
// *****************************************************************************

#include "SessionData.h"
static std::mutex sd_DataControllerMapCriticalSeciton;
static std::condition_variable sd_DataControllerMapCriticalSectionWasRevisited;

//inline DataControllerMap& CurrDcMap() { return SessionData::Curr()->GetDcMap(); }

DataController::DataController(LispPtr keyExpr)
	:	m_Key(keyExpr)
#if defined(MG_DEBUG_DCDATA)
	,	md_sKeyExpr(AsFLispSharedStr(keyExpr, FormattingFlags::ThousandSeparator))
#endif
{}

DataController::~DataController()
{
	dms_assert(GetInterestCount() == 0);
	dms_assert(!IsNew() || m_Data->GetInterestCount() == 0 || (m_Data->GetRefCount() > 1));

	std::lock_guard dcLock(sd_DataControllerMapCriticalSeciton);

	SessionData::GetDcMap().erase(m_Key);

	sd_DataControllerMapCriticalSectionWasRevisited.notify_all();
}

DataControllerRef
GetDataControllerImpl(LispPtr keyExpr, bool mayCreate)
{
	MG_CHECK(IsMainThread());

	if (keyExpr.EndP())
		return {};

	DataControllerMap& dcMap = SessionData::GetDcMap();
	DataControllerMap::iterator dcPtrLoc;
	{
		auto dcLock = std::unique_lock(sd_DataControllerMapCriticalSeciton);

	retry:
		dcPtrLoc = dcMap.lower_bound(keyExpr);
		if (dcPtrLoc != dcMap.end() && dcPtrLoc->first == keyExpr)
		{
			if (!dcPtrLoc->second->IsOwned()) // destruction is pending, wait for it and retry
			{
				sd_DataControllerMapCriticalSectionWasRevisited.wait(dcLock);
				goto retry;
			}
			return dcPtrLoc->second;
		}
		if (!mayCreate)
			return nullptr;
	}
	// we now have uqiue access to dcPtrLoc, as this is only called from one thread and keyExpr cannot be self-referential.
#if defined(MG_DEBUG_LISP_TREE)
	reportD(SeverityTypeID::ST_MinorTrace, "===GetDataController===");
	reportD(SeverityTypeID::ST_MinorTrace, AsString(keyExpr).c_str());
#endif
	assert(!keyExpr.EndP()); // entry condition

	auto dcRef = CreateDC(keyExpr);
	assert(dcRef->GetLispRef() == keyExpr);

	std::lock_guard scopedcLock(sd_DataControllerMapCriticalSeciton);
	dcMap.insert(dcPtrLoc, DataControllerMap::value_type(keyExpr, dcRef));
	return dcRef;
}

DataControllerRef GetOrCreateDataController(LispPtr keyExpr)
{
	return GetDataControllerImpl(keyExpr, true);
}

DataControllerRef GetExistingDataController(LispPtr keyExpr)
{
	return GetDataControllerImpl(keyExpr, false);
}

#include "DataLocks.h"

auto DataController::CalcResult(Explain::Context* context) const -> FutureData
{
	FutureData resultHolder(this);
	dms_assert(GetInterestCount());

	MakeResult();
	assert(resultHolder);
	return resultHolder;
}

auto DataController::CalcResultWithValuesUnits() const -> FutureData// TODO G8: REMOVE
{
	dms_assert(IsMetaThread());

	if (WasFailed(FR_Data))
		return nullptr;

	auto result = CalcResult();
	if (!result)
	{
		dms_assert(WasFailed(FR_Data) || SuspendTrigger::DidSuspend());
		return nullptr;
	}
/*
	dms_assert(CheckCalculatingOrReady(result->GetCurrRangeItem()) || result->WasFailed(FR_Data));
	if (!UpdateValuesUnits(this, m_Data.get_ptr(), useTree))
		return nullptr;

	if (result->WasFailed(FR_Data))
	{
		Fail(result.get_ptr());
		return nullptr;
	}
	*/
	dms_assert(!WasFailed(FR_MetaInfo));
	return result;
}

FutureData DataController::CalcCertainResult()  const
{
	SuspendTrigger::SilentBlocker lock;
	return CalcResult();
}

SharedStr DataController::GetSourceName() const
{
	auto keyStr = AsFLispSharedStr(m_Key, FormattingFlags::None);
	return mySSPrintF("%s: %s"
		,	keyStr.c_str()
		,	GetClsName().c_str()
	);
}

const Class* DataController::GetResultCls () const
{
	SuspendTrigger::FencedBlocker block;

	auto result = MakeResult();
	if (WasFailed(FR_MetaInfo))
		ThrowFail();
	dms_assert(result);
	return result->GetDynamicObjClass();
}

ActorVisitState DataController::DoUpdate(ProgressState ps)
{
	return AVS_Ready;
}

bool DataController::IsCalculating() const
{
	return m_Data && ::IsCalculating(m_Data->GetCurrRangeItem());
}

void DataController::DoInvalidate () const
{
	TreeItemDualRef::DoInvalidate();

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

#include "mci/Class.h"

IMPL_RTTI_CLASS(DataController);
