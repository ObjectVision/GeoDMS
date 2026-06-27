#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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

void TreeItemDualRef::Set(const TreeItem* ti, bool isNew)
{
	assert(ti);
	if (m_Data.get() != ti)
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


		// Ownership policy (see TreeItemDualref.h, m_Data / m_OwnedData):
		//  - isNew: a freshly created parentless refcount-0 cache result (e.g. CreateCacheDataItem /
		//    CreateCacheRoot); this DualRef is its primary owner, so adopt it.
		//  - isOld borrowing a CACHE item: owned transiently by its producing DC, so co-own it (borrow).
		//  - isOld borrowing a CONFIG item: the config tree owns it; owning it here would form a retain
		//    cycle up to the config root (the teardown leak), so keep m_Data non-owning (m_OwnedData empty).
		bool ownIt = isNew || ti->IsCacheItem();
		SharedTreeItem ownerHolder;
		if (ownIt)
			ownerHolder = isNew
				? MakeSharedForNewlyCreatedObject( ti )
				: MakeSharedFromBorrowedObjectPtr( ti );

		if (auto x = GetInterestPtrOrNull())
		{
			if (m_Data)
				DecDataInterestCount();
			m_Data = nullptr;
			m_OwnedData = nullptr;

			try {
				// StopInterest cannot be asynchronysly be called now as x also holds interest
				m_OwnedData = std::move(ownerHolder);
				m_Data = ti;
				IncDataInterestCount();
				assert(GetInterestCount());
			}
			catch (...)
			{
				m_Data = nullptr;
				m_OwnedData = nullptr;
				throw;
			}
		}
		else
		{
			// IncInterest can only be called in MetaThread no interest can be gained or lost as it is already zero
			m_OwnedData = std::move(ownerHolder);
			m_Data = ti;
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
		m_OwnedData = MakeSharedFromBorrowedObjectPtr( res ); // tmp: co-own the existing (owned) holder, as before
		m_Data = res;
		m_State.Set(DCF_IsTmp);
	}
	assert(GetNew() == res);
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
				const_cast<TreeItem*>(m_Data.get())->EnableAutoDelete();
		}
		m_Data = nullptr;       // drop the non-owning current pointer first
		m_OwnedData = nullptr;  // then release ownership (new/tmp/cache) -> may destroy the result
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
		GetNew()->DoFailCaller(msg, ft);
	return true;
}


void TreeItemDualRef::IncDataInterestCount() const
{
	assert(IsMetaThread());
	assert(m_Data);
	dbg_assert(!m_State.Get(DCFD_DataCounted));
	m_Data->IncInterestCount();
	MG_DEBUGCODE( m_State.Set(DCFD_DataCounted));
}

garbage_can TreeItemDualRef::DecDataInterestCount() const
{
	dbg_assert( m_State.Get(DCFD_DataCounted));
	auto result = m_Data->DecInterestCount();
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


void TreeItemDualRefContextHandle::GenerateDescription()
{
	if (HasBackRef())
		SetText(mySSPrintF("while processing result for %s", GetBackRefStr().c_str()));
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

			assert(head.IsSymb());

			if (head.GetSymbID() == token::sourceDescr)
			{
				head = keyExpr.Right().Left();
#if defined(MG_DEBUG_LISP_TREE)
				reportF(SeverityTypeID::ST_MinorTrace, "head=%s", AsString(head).c_str());
#endif
				assert(head.IsSymb());

				// Is it obvious that keyExpr describes the item that will be found as head.GetSymbID() ? 
				// Yes it is: only expressions that have been generated from the current config get evaluated.
				// CalcCache entry descriptions are passive which are used to match valid requests for cached data
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

// TEMP teardown-trace instrumentation (REMOVE after diagnosis): log s_DcMap size at teardown boundaries.
void DBG_DumpDcMapSize(const char* where)
{
	size_t sz;
	{
		std::lock_guard lk(sd_DataControllerMapCriticalSeciton);
		sz = s_DcMap.size();
	}
	if (FILE* f = fopen("C:\\dev\\GeoDMS_2026\\dcmap_trace.txt", "a"))
	{
		fprintf(f, "%-32s s_DcMap.size = %llu\n", where, (unsigned long long)sz);
		fclose(f);
	}
}

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
	dms_assert(!IsNew() || m_Data->GetInterestCount() == 0 || (m_Data->GetRefCount() > 1));

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
#include "AbstrUnit.h" // TEMP: for du/vu base conversion in DBG_DumpDcDetails

// TEMP TEST/FIX (teardown-race diagnosis): break result<->m_DataObject self-cycles for the cache results
// still registered in s_DcMap. A result's data object (Future/Lazy tile functor) owns its own result item
// via m_ResultAdi, so the result is self-pinned until its data is dropped. Parentless cache results are not
// reached by the config-root teardown sweep, so drop their data here. Hold every result alive across the
// reset loop so a cascade-free mid-loop cannot dangle a sibling, then release for a deterministic collapse.
void DBG_DropCacheResultData()
{
	std::vector<SharedTreeItem> results;
	{
		std::lock_guard lk(sd_DataControllerMapCriticalSeciton);
		results.reserve(s_DcMap.size());
		for (auto& kv : s_DcMap)
			if (auto* dc = kv.second)
				if (auto* res = dc->GetOld())
					results.emplace_back(res, existing_obj{});
	}
	for (auto& res : results)
		if (IsDataItem(res.get()))
		{
			auto* adi = AsDataItem(res.get());
			if (adi->m_DataObject)
				adi->m_DataObject.reset(); // mutable member; drops the functor -> releases its m_ResultAdi back-ref
		}
}

const TreeItem* g_DBG_ConfigRoot = nullptr; // TEMP: set by the EnableAutoDelete probe so the dump can flag the root.

// TEMP: dump per-DC holding structure + supplier graph to identify what pins the leaked DataControllers.
void DBG_DumpDcDetails(const char* where)
{
	std::vector<const DataController*> dcs;
	{
		std::lock_guard lk(sd_DataControllerMapCriticalSeciton);
		for (auto& kv : s_DcMap)
			dcs.push_back(kv.second);
	}
	std::map<const DataController*, int> idx;
	for (int i = 0; i < (int)dcs.size(); ++i) idx[dcs[i]] = i;
	auto idxOf = [&](const DataController* p) { auto it = idx.find(p); return it != idx.end() ? it->second : -1; };

	// map each leaked DC's result item -> its DC index, so we can tell whether an attribute's domain/values
	// unit is itself one of the leaked results (an intra-graph unit<->attribute ownership cycle).
	std::map<const TreeItem*, int> resIdx;
	for (int i = 0; i < (int)dcs.size(); ++i)
		if (auto* r = dcs[i]->GetOld())
			resIdx[r] = i;
	auto resIdxOf = [&](const TreeItem* p) { auto it = resIdx.find(p); return it != resIdx.end() ? it->second : -1; };

	if (FILE* f = fopen("C:\\dev\\GeoDMS_2026\\dcmap_detail.txt", "a"))
	{
		fprintf(f, "=== %s: %llu DCs ===\n", where, (unsigned long long)dcs.size());
		for (int i = 0; i < (int)dcs.size(); ++i)
		{
			auto* dc = dcs[i];
			const TreeItem* res = dc->GetOld();
			auto dcRc  = dc->GetRefCount();
			int  dcIC  = (int)dc->GetInterestCount();
			bool isNew = dc->IsNew();
			bool isOld = dc->IsOld();
			unsigned resRc = res ? (unsigned)res->GetRefCount() : 0;
			bool resIsRoot = (res && res == g_DBG_ConfigRoot);
			int  resCache = res ? (int)res->IsCacheItem() : -1;
			fprintf(f, "[%d] dcRc=%u dcIC=%d isNew=%d isOld=%d resRc=%u resCache=%d%s", i, (unsigned)dcRc, dcIC, (int)isNew, (int)isOld, resRc, resCache, resIsRoot ? " RES==ROOT" : "");
			if (res)
				fprintf(f, " res='%s'", res->GetSourceName().c_str());
			if (res && IsDataItem(res))
			{
				auto* adi = AsDataItem(res);
				const TreeItem* du = adi->GetAbstrDomainUnit();
				const TreeItem* vu = adi->GetAbstrValuesUnit();
				fprintf(f, " du=r%d(rc=%u) vu=r%d(rc=%u)",
					resIdxOf(du), du ? (unsigned)du->GetRefCount() : 0u,
					resIdxOf(vu), vu ? (unsigned)vu->GetRefCount() : 0u);
			}
			fprintf(f, " suppliers:");
			if (auto* fdc = dynamic_cast<const FuncDC*>(dc))
			{
				for (DcRefListElem* e = fdc->GetArgList(); e; e = e->m_Next.get())
					fprintf(f, " a%d", idxOf(e->m_DC.get()));
				for (auto& os : fdc->DBG_GetOtherSuppliers())
					fprintf(f, " o%d", idxOf(os.get()));
			}
			fprintf(f, "\n");
		}
		fclose(f);
	}
}

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
	return mySSPrintF("%s: %s"
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
	return m_Data && ::IsCalculating(m_Data->GetCurrRangeItem());
}

void DataController::DoInvalidate () const
{
	TreeItemDualRef::DoInvalidate();

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

#include "mci/Class.h"

IMPL_RTTI_CLASS(DataController);
