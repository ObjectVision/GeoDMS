// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// DataControllers: the calculation cache -- cache entries keyed by their
// LispRef expression, with creation, lookup and interest administration.

#include "DataController.h"

#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/SeverityType.h"
#include "ser/AsString.h"
#include "utl/StrFormat.h"

#include "LispRef.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h" // for IsDataItem/AsDataItem + GetCurrDomainUnit/GetCurrValuesUnit in KeepResultUnitsAlive
#include "AbstrUnit.h"     // complete AbstrUnit for the unit->TreeItem upcast + IsCacheItem in KeepResultUnitsAlive
#include "ItemLocks.h"
#include "LispTreeType.h"
#include "OperationContext.h"

#include <condition_variable>

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
}

// Walk a freshly-created cache RESULT subtree and append owning refs to its cache items' domain/values units
// to the kind-1 holder, so those units (parentless cache results with no other owner once the operator's unit
// locals drop) stay alive while the result is referenced. Config units (IsCacheItem()==false) are tree-owned
// and skipped (owning one would re-form the config-root retain cycle); the root itself is skipped.
static void CollectCacheUnitsToKeepAlive(const TreeItem* root, const TreeItem* item, DcRef& dc)
{
	if (IsDataItem(item))
	{
		const AbstrDataItem* adi = AsDataItem(item);
		if (auto du = adi->GetCurrDomainUnit())
			if (const TreeItem* dut = du.get(); dut != root && dut->IsCacheItem())
				dc.keepAlive(du);
		if (auto vu = adi->GetCurrValuesUnit())
			if (const TreeItem* vut = vu.get(); vut != root && vut->IsCacheItem())
				dc.keepAlive(vu);
	}
	for (const TreeItem* sub = item->_GetFirstSubItem(); sub; sub = sub->GetNextItem())
		CollectCacheUnitsToKeepAlive(root, sub, dc);
}

void TreeItemDualRef::KeepResultUnitsAlive(const TreeItem* root)
{
	CollectCacheUnitsToKeepAlive(root, root, m_Data);
}

// Called from FuncDC_CreateResult right after the operator finished building the result subtree: walk the
// (now complete) cache result and have the kind-1 holder own its cache items' domain/value units, which are
// still alive at this point (they are sub-items of the cache root, or held by the operator's args).
void TreeItemDualRef::CaptureResultUnits()
{
	if (m_Data.kind() == 1)
		if (auto root = m_Data.get())
			KeepResultUnitsAlive(root.get());
}

void TreeItemDualRef::Set(const TreeItem* ti, bool isNew)
{
	assert(ti);
	if (m_Data.get().get() != ti)
	{
		assert(IsMetaThread());
		assert(!m_State.Get(DCF_IsOld|DCF_IsTmp));

		if (isNew)
		{
			if (ti) const_cast<TreeItem*>(ti)->SetIsCacheItem();
			assert(!m_State.Get(DCF_IsOld));
		}
		else
			m_State.Set(DCF_IsOld);


		// Ownership policy (doc/development/std-ptr-migration-plan.md §4, DcRef arms):
		//  - isNew                 -> arm 1 std::shared_ptr<TreeItem>       (DualRef is the primary owner)
		//  - isOld borrowing CACHE -> arm 2 std::shared_ptr<const TreeItem> (co-own; transient cache result)
		//  - isOld borrowing CONFIG-> arm 3 std::weak_ptr<const TreeItem>   (tree owns it; owning here = the
		//    config-root retain cycle / teardown leak -- so keep it NON-owning and .lock()-checked)
		auto setArm = [&]() // std co-ownership via shared_from_this/weak_from_this (ti is a make_shared'd TreeItem)
		{
			if (isNew)
			{
				DcRef::NewResult nr;
				nr.m_Root = std::static_pointer_cast<TreeItem>(const_cast<TreeItem*>(ti)->shared_from_this()); // the cache root result
				m_Data.m_Holder = std::move(nr);
				// The cache result's domain/values units are kept alive by their producer: each UnitCreator site
				// calls resultHolder.KeepAlive(v) on the unit it makes (else it is ownerless -- stored only weakly
				// as m_DomainUnit/m_ValuesUnit), and FuncDC_CreateResult calls CaptureResultUnits() for the rest.
				// (Non-FuncDC NumbDC/StringDC param results have void/default units owned statically by the unit class.)
			}
			else if (ti->IsCacheItem())
			{
				DcRef::OldCacheSubItem o;
				const TreeItem* root = ti;
				while (auto p = root->GetTreeParent()) root = p.get(); // walk up to the cache root
				o.m_Root = root->shared_from_this();
				o.m_SubItem = ti;
				m_Data.m_Holder = std::move(o);
			}
			else
				m_Data.m_Holder = std::weak_ptr<const TreeItem>(ti->weak_from_this());
		};

		if (auto x = GetInterestPtrOrNull())
		{
			if (m_Data)
				DecDataInterestCount();
			m_Data.clear();

			try {
				// StopInterest cannot be asynchronysly be called now as x also holds interest
				setArm();
				IncDataInterestCount();
				assert(GetInterestCount());
			}
			catch (...)
			{
				m_Data.clear();
				throw;
			}
		}
		else
		{
			// IncInterest can only be called in MetaThread no interest can be gained or lost as it is already zero
			setArm();
			assert(!GetInterestCount());
		}
	}
	assert(!ti || GetOld() == ti);
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
	assert(res);
	if (!m_Data)
	{
		assert(!m_State.Get(DCF_IsOld|DCF_IsTmp));
		m_Data.m_Holder = std::weak_ptr<TreeItem>(res->weak_from_this()); // tmp: non-owning borrow (doc §4 arm 4); the calling site keeps res alive
		m_State.Set(DCF_IsTmp);
	}
	assert(GetNew() == res);
}

void TreeItemDualRef::Clear()
{
	auto curr = m_Data.get(); // locks the weak arms; null if already expired
	if (curr)
	{
		if (!m_State.Get(DCF_IsTmp))
		{
			if (GetInterestCount())
				DecDataInterestCount();
			if (!m_State.Get(DCF_IsOld))
				const_cast<TreeItem*>(curr.get())->EnableAutoDelete();
		}
	}
	m_Data.clear();       // release the variant arm (owning arms may destroy the result here)
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
		GetNew()->DoFailCaller(msg, ft);
	return true;
}


void TreeItemDualRef::IncDataInterestCount() const
{
	assert(IsMetaThread());
	assert(m_Data);
	dbg_assert(!m_State.Get(DCFD_DataCounted));
	if (auto curr = m_Data.get()) // weak arms can have expired; skipped Inc is balanced by the skipped Dec on the same dead item
		curr->IncInterestCount();
	MG_DEBUGCODE( m_State.Set(DCFD_DataCounted));
}

garbage_can TreeItemDualRef::DecDataInterestCount() const
{
	dbg_assert( m_State.Get(DCFD_DataCounted));
	garbage_can result;
	if (auto curr = m_Data.get())
		result = curr->DecInterestCount();
	MG_DEBUGCODE( m_State.Clear(DCFD_DataCounted));

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

garbage_can TreeItemDualRef::StopInterest() const noexcept
{
	garbage_can garbage;
	if (m_Data && !IsTmp())
		garbage |= DecDataInterestCount();
	garbage |= Actor::StopInterest();
	return garbage;
}

// *****************************************************************************

static thread_local const TreeItemDualRef* s_CurrTreeItemDualRef = nullptr;

TreeItemDualRefContextHandle::TreeItemDualRefContextHandle(const TreeItemDualRef * currRef)
	: ObjectContextHandle(currRef)
	, m_PrevRef(s_CurrTreeItemDualRef)
{
	s_CurrTreeItemDualRef = currRef;
}

TreeItemDualRefContextHandle::~TreeItemDualRefContextHandle()
{
	s_CurrTreeItemDualRef = m_PrevRef;
}

bool TreeItemDualRefContextHandle::HasBackRef()
{
	return s_CurrTreeItemDualRef && s_CurrTreeItemDualRef->HasBackRef();
}

auto TreeItemDualRefContextHandle::GetBackRefStr() ->SharedStr
{
	assert(s_CurrTreeItemDualRef);
	return s_CurrTreeItemDualRef->GetBackRefStr();
}


bool TreeItemDualRefContextHandle::HasItemName()
{
	return s_CurrTreeItemDualRef && s_CurrTreeItemDualRef->HasItemName();
}

auto TreeItemDualRefContextHandle::GetItemNameStr() ->SharedStr
{
	assert(s_CurrTreeItemDualRef);
	return s_CurrTreeItemDualRef->GetItemNameStr();
}

void TreeItemDualRefContextHandle::GenerateDescription()
{
	if (HasItemName())
		SetText(mySSPrintF("while processing result for {}", GetItemNameStr().c_str()));
}

/********** DataControllerContextHandle **********/

void DataControllerContextHandle::GenerateDescription()
{
	SetText(
		mySSPrintF("Called from the DataController for (in sLisp): {}",
			AsString(m_DC->GetLispRef()).c_str()
		)
	);
}
// *****************************************************************************
// Section:     DataController Factory
// *****************************************************************************

#include "MoreDataControllers.h"
#include "LispList.h"

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

			assert(head.IsSymb());

			if (head.GetSymbID() == token::sourceDescr)
			{
				head = keyExpr.Right().Left();
#if defined(MG_DEBUG_LISP_TREE)
				reportF(SeverityTypeID::ST_MinorTrace, "head={}", AsString(head).c_str());
#endif
				assert(head.IsSymb());

				// Is it obvious that keyExpr describes the item that will be found as head.GetSymbID() ? 
				// Yes it is: only expressions that have been generated from the current config get evaluated.
				// The sourceDescr head is passive: it identifies an item without describing a calculation.
				// It once also matched entries in the CalcCache, which was retired with the 8.0 series;
				// within a session it still keys the SymbDC.
				return MakeSharedForNewlyCreatedObject(new SymbDC(keyExpr, head.GetSymbID()));
			}
			const AbstrOperGroup* og = AbstrOperGroup::FindName(head->GetSymbID());
			assert(og->MustCacheResult());
			return MakeSharedForNewlyCreatedObject(new FuncDC(keyExpr, og));
		}
		else if (keyExpr.IsSymb())
			return MakeSharedForNewlyCreatedObject(new SymbDC(keyExpr, keyExpr.GetSymbID()));
		else if (keyExpr.IsStrn())
			return MakeSharedForNewlyCreatedObject(new StringDC(keyExpr));
		else if (keyExpr.IsNumb())
		{
			assert(keyExpr.IsNumb());
			return MakeSharedForNewlyCreatedObject(new NumbDC(keyExpr));
		}
		else
		{
			assert(keyExpr.IsUI64());
			return MakeSharedForNewlyCreatedObject(new UI64DC(keyExpr));
		}
	}
}	// anonymous namespace

// *****************************************************************************
// auxiliary contructs
// *****************************************************************************

/********** DataControllerMap **********/

using DataControllerMap = std::map<DataController::DataControllerKey, const DataController*>;

static DataControllerMap s_DcMap;
static std::mutex sd_DataControllerMapCriticalSeciton;
static std::condition_variable sd_DataControllerMapCriticalSectionWasRevisited;

// *****************************************************************************
// Section:     DataController Implementation
// *****************************************************************************

DataController::DataController(LispPtr keyExpr)
	:	m_Key(keyExpr)
#if defined(MG_DEBUG_DCDATA)
	,	md_sKeyExpr(AsFLispSharedStr(keyExpr, FormattingFlags::ThousandSeparator))
#endif
{}

DataController::~DataController()
{
	dms_assert(GetInterestCount() == 0);
	dms_assert(!IsNew() || m_Data.get()->GetInterestCount() == 0 || (m_Data.use_count() > 1)); // std owner-count replaces intrusive GetRefCount

	std::lock_guard dcLock(sd_DataControllerMapCriticalSeciton);

	s_DcMap.erase(m_Key);

	sd_DataControllerMapCriticalSectionWasRevisited.notify_all();
}

DataControllerRef
GetDataControllerImpl(LispPtr keyExpr, bool mayCreate)
{
	MG_CHECK(IsMetaThread() || !mayCreate);

	if (keyExpr.EndP())
		return {};

	DataControllerMap::iterator dcPtrLoc;
	{
		auto dcLock = std::unique_lock(sd_DataControllerMapCriticalSeciton);

		while (true) {
			dcPtrLoc = s_DcMap.lower_bound(keyExpr);
			if (dcPtrLoc == s_DcMap.end() || dcPtrLoc->first != keyExpr)
				break;
			auto result = MakeSharedFromWeakPtrInsideSync(dcPtrLoc->second);;
			if (result)
				return result;
			if (!mayCreate)
				return {};
			sd_DataControllerMapCriticalSectionWasRevisited.wait(dcLock);
		}
		if (!mayCreate)
			return {};
	}
	// we now have uqiue access to dcPtrLoc, as this is only called from one thread and keyExpr cannot be self-referential.
#if defined(MG_DEBUG_LISP_TREE)
	reportD(SeverityTypeID::ST_MinorTrace, "===GetDataController===");
	reportD(SeverityTypeID::ST_MinorTrace, AsString(keyExpr).c_str());
#endif
	assert(!keyExpr.EndP()); // entry condition
	assert(mayCreate);
	assert(IsMetaThread());

	auto dcRef = CreateDC(keyExpr);
	assert(dcRef->GetLispRef() == keyExpr);

	std::lock_guard scopedcLock(sd_DataControllerMapCriticalSeciton);
	s_DcMap.insert(dcPtrLoc, DataControllerMap::value_type(keyExpr, dcRef.get()));
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

auto DataController::CallCalcResult(std::shared_ptr<Explain::Context> context) const -> FutureData
{
	FutureData resultHolder(DataControllerRef(this));
	assert(GetInterestCount());
	assert(!SuspendTrigger::DidSuspend());

	MakeResult();
	assert(!SuspendTrigger::DidSuspend());

	auto resultItem = this->GetOld();
	if (resultItem && resultItem->mc_DC)
	{
		resultHolder = resultItem->mc_DC->CallCalcResult(context);
		assert(!SuspendTrigger::DidSuspend());

	}

	assert(resultHolder);
	return resultHolder;
}

auto DataController::CalcResultWithValuesUnits() const -> FutureData// TODO G8: REMOVE
{
	dms_assert(IsMetaThread());

	if (WasFailed(FailType::Data))
		return nullptr;

	auto result = CallCalcResult();
	if (!result)
	{
		dms_assert(WasFailed(FailType::Data) || SuspendTrigger::DidSuspend());
		return nullptr;
	}
/*
	dms_assert(CheckCalculatingOrReady(result->GetCurrRangeItem()) || result->WasFailed(FailType::Data));
	if (!UpdateValuesUnits(this, m_Data.get_ptr(), useTree))
		return nullptr;

	if (result->WasFailed(FailType::Data))
	{
		Fail(result.get_ptr());
		return nullptr;
	}
	*/
	assert(!WasFailed(FailType::MetaInfo));
	return result;
}

FutureData DataController::CalcCertainResult()  const
{
	SuspendTrigger::SilentBlocker lock("DataController::CalcCertainResult()");
	return CallCalcResult();
}

SharedStr DataController::GetSourceName() const
{
	auto keyStr = AsFLispSharedStr(m_Key, FormattingFlags::None);
	return mySSPrintF("{}: {}"
		,	keyStr.c_str()
		,	GetClsName().c_str()
	);
}

const Class* DataController::GetResultCls () const
{
	SuspendTrigger::FencedBlocker block("DataController::GetResultCls()");

	auto result = MakeResult();
	if (WasFailed(FailType::MetaInfo))
		ThrowFail();
	dms_assert(result);
	return result->GetDynamicObjClass();
}

ActorVisitState DataController::DoUpdate()
{
	return AVS_Ready;
}

bool DataController::IsCalculating() const
{
	auto curr = GetCurr();
	if (!curr)
		return false;
	auto rangeItem = curr->GetCurrRangeItem();
	return rangeItem && ::IsCalculating(rangeItem.get());
}

void DataController::DoInvalidate () const
{
	TreeItemDualRef::DoInvalidate();

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

#include "mci/Class.h"

IMPL_RTTI_CLASS(DataController);
