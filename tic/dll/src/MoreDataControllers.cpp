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

#include "MoreDataControllers.h"

#include "act/ActorSet.h"
#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/DebugContext.h"
#include "dbg/SeverityType.h"
#include "geo/StringArray.h"
#include "ser/FileStreamBuff.h"
#include "utl/swap.h"
#include "xct/DmsException.h"

#include "LockLevels.h"

#include "AbstrDataItem.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataStoreManagerCaller.h"
#include "DC_Ptr.h"
#include "FreeDataManager.h"
#include "ItemLocks.h"
#include "LispTreeType.h"
#include "Operator.h"
#include "OperationContext.ipp"
#include "Param.h"
#include "SessionData.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#if defined(MG_DEBUG_DCDATA)
#define MG_DEBUG_FUNCDC false
#endif
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
// ArgRefs
// *****************************************************************************


const TreeItem* GetItem(const ArgRef& ar)
{
	if (ar.index() == 0)
	{
		const FutureData& fd = std::get<0>(ar);
		if (!fd)
			return nullptr;
		return fd->GetOld();
	}
	return std::get<1>(ar).get_ptr();
}

const Actor* GetStatusActor(const ArgRef& ar)
{
	if (ar.index() == 0)
	{
		return std::get<0>(ar).get_ptr();
	}
	return std::get<1>(ar).get_ptr();
}

ArgSeqType GetItems(const ArgRefs& ar)
{
	ArgSeqType result; result.reserve(ar.size());
	for (const auto& a : ar)
		result.emplace_back(GetItem(a));
	return result;
}

const AbstrDataItem* AsDataItem(const ArgRef& ar)
{
	return AsDataItem(GetItem(ar)); 
}

// *****************************************************************************
// Section:     FuncDC implementation
// *****************************************************************************

FuncDC::FuncDC(LispPtr keyExpr,	const AbstrOperGroup* og)
	:	DataController(keyExpr)
	,	m_OperatorGroup(og)
{
	dms_assert(og && (og->MustCacheResult() || !og->CanResultToConfigItem()));

	DBG_START("FuncDC", "ctor", false);
	DBG_TRACE(("keyExpr = %s", AsFLispSharedStr(keyExpr).c_str()));

	if (og->IsObsolete())
		reportF(SeverityTypeID::ST_Warning, "obsolete operator %s used: %s", og->GetName(), og->GetObsoleteMsg());

	dms_assert(GetLispRef().IsRealList());    // no EndP allowed
	dms_assert(GetLispRef().Left().IsSymb()); // operator or calculation scheme call

	if (og->IsTransient())
		m_State.Set(DCF_CanChange);
//	if (og->HasTemplArg())
//		m_State.Set(DCF_IsTmp);

	// for each subexpr in keyExpr do add arg
	OwningPtr<DcRefListElem>* nextArgPtr = &m_Args;
	for (LispPtr tailPtr = keyExpr.Right(); !tailPtr.EndP(); tailPtr = tailPtr.Right()) 
	{
		DBG_TRACE(("arg = %s", AsFLispSharedStr(tailPtr->Left()).c_str()));
		DcRefListElem* dcRef = new DcRefListElem;
		nextArgPtr->assign(dcRef);
		dcRef->m_DC = GetOrCreateDataController(tailPtr->Left());
		if (dcRef->m_DC->m_State.Get(DCF_CanChange))
			m_State.Set(DCF_CanChange);
		nextArgPtr = &(dcRef->m_Next);
	}
}

FuncDC::~FuncDC()
{
	CancelOperContext();
//	m_Data.Clear();
	dms_assert(!GetInterestCount());
	dms_assert(!m_State.Get(actor_flag_set::AF_SupplInterest));
}

leveled_critical_section cs_OperContextAccess(item_level_type(0), ord_level_type::OperContextAccess, "FuncDC.OperContext");

std::shared_ptr<OperationContext> FuncDC::GetOperContext() const
{
	leveled_critical_section::scoped_lock ocaLock(cs_OperContextAccess);
	return m_OperContext;
}

std::shared_ptr<OperationContext> FuncDC::ResetOperContextImpl() const
{
	leveled_critical_section::scoped_lock ocaLock(cs_OperContextAccess);

	dms_assert(
		(!m_InterestCount) 
	||	!IsNew() 
	||	CheckDataReady(GetNew())
	||	GetNew()->WasFailed(FR_Data) 
	||	DSM::IsCancelling()
	||	m_State.GetProgress() == PS_None // Just invalidated.
	);

	auto operContext = std::move(m_OperContext);
	if (operContext) operContext->m_FuncDC.reset();
	dms_assert(!m_OperContext);
	return operContext;
}

garbage_t FuncDC::ResetOperContextImplAndStopSupplInterest() const
{
	auto res = StopSupplInterest();
//	dms_assert(!DoesHaveSupplInterest());
	res |= ResetOperContextImpl();
	return res;
}

void FuncDC::CancelOperContext() const
{
	auto operContext = ResetOperContextImplAndStopSupplInterest();
}

bool FuncDC::IsCalculating() const
{
	if (!IsNew())
		return base_type::IsCalculating();

	leveled_critical_section::scoped_lock ocaLock(cs_OperContextAccess);
	return m_OperContext && m_OperContext->IsScheduled();
}

void FuncDC::DoInvalidate() const
{
	m_State.Clear(DCF_CalcStarted);
	m_Operator.reset();
	CancelOperContext();
	dms_assert(!IsCalculating());

/*
	if (m_Data && m_Data->IsCacheRoot())
		for (TreeItem* cacheItem = GetNew(); cacheItem = GetNew()->WalkCurrSubTree(cacheItem); )
		{
			dms_assert(cacheItem != GetNew());
			dms_assert(cacheItem->m_LastChangeTS <= m_LastChangeTS);
//			cacheItem->SetDcKnown();
			dms_assert(cacheItem->mc_RefItem == nullptr);

			auto dc = DSM::Curr()->GetSubItemDC(this, cacheItem, false);
			if (!dc)
				continue;
			dc->InvalidateAt(m_LastChangeTS);
		}
*/
	base_type::DoInvalidate();

	dms_assert(!m_Data);										 // dropped by base_type::DoInvalidate
	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());	 // set by base_type::DoInvalidate
	dms_assert(!IsCalculating());
	dms_assert(!m_State.Get(DCF_CalcStarted));
}

garbage_t FuncDC::StopInterest () const noexcept
{ 
	auto garbage = ResetOperContextImplAndStopSupplInterest();
	garbage |= DataController::StopInterest(); 
//	garbage |= m_Data->TryCleanupMem();
	m_State.Clear(DCF_CalcStarted);
	return garbage;
}

oper_arg_policy FuncDC::GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const
{
	dms_assert(firstArgValue == nullptr || *firstArgValue == char(0));

	if (m_OperatorGroup->IsTemplateCall())
		return oper_arg_policy(m_OperatorGroup->GetArgPolicy(argNr, firstArgValue));

	const Operator* op = GetOperator();
	dms_assert(op);
	return op->GetArgPolicy(argNr, firstArgValue);
}

// CalcCacheStorage
/*  REMOVE
class CalcCacheStorageManager : public AbstrStorageManager
{
	SharedStr m_FilenameBase;
	bool ReadDataItem(const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override
	{
		MG_CHECK2(false, "NYI");
	}
};
*/

// Postcondition of CalcResult(true):
//		Null is returned OR calculation has started that will make m_Data have valid data such that it can be accessed (DataReadLock can be set) or become failed
//		and used as arg to calculate a result for a referring FuncDC
//
//	normal operation:
//
//		DCF_CalcStarted until interestcount drops to 0.
//
//
//	special operations:
//
//		DomainUnit, ValuesUnit: (gevonden unit moet een config-item zijn).
//			IsOld() geldt, dus igv doCalc doet PrepareDataUsage het echte werk
//			normal operation if fine
//
//		SubItem: 
//			igv config-item; zie DomainUnit, ValuesUnit
//			igv CacheTree (relational operator, template call of for_each): DoCalc arg1
//			let op: bij verandering van 2e arg is wijziging van m_Data noodzakelijk.
//
//	CompoundDC's:
//
//		for_each
//		loop
//		TemplDC's

SharedTreeItem FuncDC::MakeResult() const // produce signature
{
#if defined(MG_DEBUG_DCDATA)
	DBG_START("FuncDc::MakeResult", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC);

	auto_flag_recursion_lock<DCFD_IsCalculating> reentryLock(Actor::m_State);
	const TreeItem* dContext = m_Data;

	dms_assert(IsMainThread());
#endif

	DetermineState(); // may trigger DoInvalidate -> reset m_Data, only MainThread may re-MakeResult
	if (WasFailed(FR_MetaInfo))
		return nullptr;

	static UInt32 debug_counter = 0;
	DBG_TRACE(("%s m_Data %s m_OperContext %s", debug_counter++, bool(m_Data), bool(m_OperContext)));

	if (!m_Data) 
	{
		DBG_TRACE(("MakeResult starts"));
		if (!MakeResultImpl())
		{
			dms_assert(WasFailed(FR_MetaInfo)); // MakeResult cannot suspend
			dms_assert(!DoesHaveSupplInterest());
			CancelOperContext();
			return nullptr;
		}
		dms_assert(m_Data);
		DBG_TRACE(("MakeResult completed well"));
	}
	
	if (IsNew()) // && !m_State.Get(DCF_CacheRootKnown))
	{
		dms_assert(m_Data->IsCacheRoot());
/*
		auto storeRecordPtr = DataStoreManager::Curr()->GetStoreRecord(GetLispRef());
		if (storeRecordPtr)
		{
			m_State.Set(DCF_CacheRootKnown);
			bool isStored = (storeRecordPtr->ts != 0);
			auto sfwa = DataStoreManager::Curr()->GetSafeFileWriterArray();

			SharedStr filenameBase = DataStoreManager::Curr()->m_StoreMap[GetLispRef()].fileNameBase;
			dms_assert(!filenameBase.empty());

			auto cacheRoot = GetNew();
			for (auto cacheItem = cacheRoot; cacheItem; cacheItem->WalkCurrSubTree(cacheItem))
			{
				bool isDataItem = IsDataItem(cacheItem);
				if (!isStored && !isDataItem)
					continue;
				SharedStr filename = filenameBase;
				if (cacheItem != cacheRoot)
					filename += '/' + cacheRoot->GetRelativeName(cacheItem);
					
				if (isDataItem)
				{
					auto openFile = OpenFileData(AsDataItem(cacheItem), filename+".dmsdata", sfwa); // issue: requires DataReady on domain => always store domain
					if (isStored)
						AsDataItem(cacheItem)->m_DataObject.assign(openFile.release());
					else
						AsDataItem(cacheItem)->m_FileName = filename;
				}
				else if (IsUnit(cacheItem))
				{
					MappedFileInpStreamBuff fin(filename+".dmsunit", sfwa, true, false);
					cacheItem->LoadBlobStream(&fin);
				}

			}
		}
*/
	}

//	dms_assert(m_State.Get(DCF_CacheRootKnown) == (IsNew() && m_Data->IsDcKnown()));
	if (m_Data->WasFailed(FR_MetaInfo))
		Fail(m_Data);

	if (WasFailed(FR_MetaInfo))
		return nullptr;

	actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("Actor::DecInterestCount") sg_ActorLockMap, this);
	auto operContext = GetOperContext();
	if (operContext)
		m_OtherSuppliersCopy = operContext->m_OtherSuppliers;

	if (!GetInterestCount())
	{
		DBG_TRACE(("ResetContext"));
		dms_assert(!DoesHaveSupplInterest());
		ResetOperContextImpl();
	}
	return m_Data;
}

auto FuncDC::CalcResult(Explain::Context* context) const -> FutureData
{
#if defined(MG_DEBUG_DCDATA)
	DBG_START("FuncDc::CalcResult", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC);

	auto_flag_recursion_lock<DCFD_IsCalculating> reentryLock(Actor::m_State);
	const TreeItem* dContext = m_Data;

	dms_assert(IsMainThread());
	dms_assert(!SuspendTrigger::DidSuspend());
#endif

	DetermineState(); // may trigger DoInvalidate -> reset m_Data, only MainThread may re-MakeResult
	if (WasFailed(FR_MetaInfo))
		return {};

#if defined(MG_DEBUG)
//	static TokenID testsID = GetTokenID("tests");
//	if (dContext && dContext->m_BackRef && dContext->m_BackRef->GetID() == testsID)
//		__debugbreak();
#endif

	FutureData thisFutureResult = this;
	if (m_State.Get(DCF_CalcStarted))
	{
		dms_assert(CheckCalculatingOrReady(m_Data->GetCurrRangeItem()) || m_Data->WasFailed(FR_Data));
		dms_assert(!SuspendTrigger::DidSuspend());
		return thisFutureResult;
	}

	// precondition if doCalc: Interest, SupplInterest, Not FR_MetaInfo, nor Args; FR_Data may occur in worker threads, but then re-Make is futile.
//	dms_assert(m_InterestCount);

	static UInt32 debug_counter = 0;
	DBG_TRACE(("%s m_Data %s m_OperContext %s context %s ", debug_counter++, bool(m_Data), bool(m_OperContext), bool(context)));

	if (!m_Data || !m_OperContext)
	{
		DBG_TRACE(("MakeResult starts"));
		if (!MakeResultImpl())
		{
			dms_assert(WasFailed(FR_MetaInfo) ); // MakeResult cannot suspend
			return {};
		}
		dms_assert(m_Data);
		dms_assert(m_OperContext);
		DBG_TRACE(("MakeResult completed well"));
	}
	m_Data->UpdateMetaInfo();

	if (m_Data->WasFailed(FR_Data))
		Fail(m_Data);

	if (WasFailed(FR_Data))
		return {};

	dms_assert(GetInterestCount());
	dms_assert(m_Data->IsPassor() || m_OperatorGroup->CanResultToConfigItem() );
	if (context && !m_OperatorGroup->CanExplainValue())
		context = nullptr;

	#if defined(MG_DEBUG_UPDATESOURCE)
		SupplInclusionTester guaranteeThatCompleteSupplRelIsTransitive(this);
	#endif

	DBG_TRACE(("CalcResult"));

	dms_assert(GetInterestCount()); 

	if (!IsAllDataReady(m_Data.get()) || context)
	{
		dms_assert(m_Data->GetInterestCount());
		dms_assert(m_State.Get(actor_flag_set::AF_SupplInterest) || context);

		CalcResultImpl(context);
		if (!m_Data || SuspendTrigger::DidSuspend())
		{
			dms_assert(SuspendTrigger::DidSuspend() || WasFailed());
			return {}; // maybe suspended or failed
		}
		dms_assert(!SuspendTrigger::DidSuspend());
		dms_assert(m_OperContext || IsDataReady(m_Data->GetCurrRangeItem()) || m_Data->WasFailed(FR_Data) || SuspendTrigger::DidSuspend());
	}
	m_State.Set(DCF_CalcStarted);
	return thisFutureResult;
}

/********** Interface **********/

typedef std::vector<const Class*> ArgClsSeqType;


bool FuncDC::MustCalcArg(oper_arg_policy ap, bool doCalc)
{
	switch (ap)
	{
		case oper_arg_policy::calc_subitem_root:
		case oper_arg_policy::calc_as_result: 
			return doCalc;
		case oper_arg_policy::calc_always:    
			return true;
//		case oper_arg_policy::calc_never:
//		case oper_arg_policy::is_templ:       
//		case oper_arg_policy::subst_with_subitems:
		default:
			return false;
	}
}

const Operator* FuncDC::GetOperator() const
{
	if (!m_Operator)
	{
		dms_assert(IsMainThread());
		ArgClsSeqType operandTypeSeq;
		if (WasFailed(FR_MetaInfo))
			ThrowFail();

		arg_index argCount = 0;
		for (const DcRefListElem* argIter = m_Args; argIter; argIter = argIter->m_Next, ++argCount)
		{
			const DataController* argDC = argIter->m_DC;
			if (argDC->WasFailed(FR_MetaInfo))
			{
				Fail(argDC);
				ThrowFail();
			}
			const Class* resCls = argDC->GetResultCls();
			dms_assert(resCls);
			operandTypeSeq.push_back( resCls );
		}

		dbg_assert( operandTypeSeq.size() == GetNrArgs() );
		m_Operator = m_OperatorGroup->FindOper(argCount, begin_ptr( operandTypeSeq ));
		dms_assert(m_Operator);
	}
	return m_Operator;
}

const Class* FuncDC::GetResultCls() const
{
	const Operator* oper = GetOperator();
	if (oper->HasRegisteredResultClass())
	{
		const Class* result = oper->GetResultClass();
		dms_assert(result);
		dms_assert(result != AbstrUnit    ::GetStaticClass());
		dms_assert(result != AbstrDataItem::GetStaticClass());
		return result;
	}
	return base_type::GetResultCls();
}

// =========================================  MakeResult

OArgRefs FuncDC::GetArgs(bool doUpdateMetaInfo, bool doCalcData) const
{
	dms_assert(IsMainThread());

	DBG_START("FuncDc::GetArgs", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC && doCalcData);

	dms_assert(!doCalcData || GetInterestCount());
	dms_assert(!doCalcData || DoesHaveSupplInterest());

	arg_index currArg = 0;
	ArgRefs argSeq; argSeq.reserve(GetNrArgs());

	SharedStr firstArgValue;
	for (const DcRefListElem* argIter = m_Args; argIter; ++currArg, argIter = argIter->m_Next) {
		dms_assert(argIter->m_DC); // DcRefListElem invariant

		bool mustCalcArg = MustCalcArg(currArg, doCalcData, firstArgValue.begin());

		ArgRef argRef;
		if (!mustCalcArg) {
			argRef.emplace<SharedTreeItem>(argIter->m_DC->MakeResult()); // post:CheckCalculatingOrReady
			dms_assert(!SuspendTrigger::DidSuspend()); // POSTCONDITION of argIter->m_DC->MakeResult();
		} else {
			dms_assert(!doCalcData || argIter->m_DC->GetInterestCount());
			FutureData fd = argIter->m_DC; fd = argIter->m_DC->CalcResultWithValuesUnits();
			dms_assert(!fd || argIter->m_DC->GetInterestCount());
			if (SuspendTrigger::DidSuspend())
				return {};
			dms_assert(!fd || CheckCalculatingOrReady(fd->GetOld()->GetCurrRangeItem()) || fd->WasFailed(FR_Data) || fd->GetOld()->WasFailed(FR_Data));
			argRef.emplace<FutureData>(std::move(fd));
			if (currArg == 0 && m_OperatorGroup->HasDynamicArgPolicies())
				firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(argIter->m_DC->GetOld())))->GetIndexedValue(0);
		}

		auto argItem = GetItem(argRef);
		if (argItem) {
			if (doUpdateMetaInfo)
				argItem->UpdateMetaInfo();
			dms_assert(argItem->m_State.GetProgress() >= PS_MetaInfo || argItem->WasFailed(FR_MetaInfo));
			if (argItem->WasFailed(mustCalcArg))
				argIter->m_DC->Fail(argItem, mustCalcArg ? FR_Data : FR_MetaInfo);
		}

		if (argIter->m_DC->WasFailed(mustCalcArg))
			Fail(argIter->m_DC, doCalcData ? FR_Data : FR_MetaInfo);

		dms_assert(argItem || WasFailed(doCalcData)); // POSTCONDITION of argIter->m_DC->GetResult();

		if (WasFailed(doCalcData))
			return {};
		dms_assert(argItem && !argItem->WasFailed(FR_MetaInfo));
		argSeq.emplace_back(std::move(argRef));
	}
	return argSeq;
}

bool FuncDC::MakeResultImpl() const
{
	dms_assert(IsMainThread());
	dms_check_not_debugonly; 

	dms_assert(!WasFailed(FR_MetaInfo));
	dms_assert(!SuspendTrigger::DidSuspend());

	StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
	InterestRetainContextBase base;
	dms_assert(!m_OperContext);
	// ============== Call GetResult for each of the arguments

#if defined(MG_DEBUG_DCDATA)
	DBG_START("FuncDc::MakeResultImpl", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC);
#endif

	bool result;
	try {
		UpdateMarker::ChangeSourceLock changeStamp(this, "FuncDC::MakeResult");
		UpdateLock lock(this, actor_flag_set::AF_UpdatingMetaInfo);
	
		// ============== Call the actual operator
		// TODO G8.1: CreateResult() Verwijderen uit OperationContext en constructie vermijden/uitstellen

		auto operContext = std::make_shared<OperationContext>(this, m_OperatorGroup);
		{
			leveled_critical_section::scoped_lock ocaLock(cs_OperContextAccess);

			dms_assert(!m_OperContext);
			m_OperContext = operContext;
			dms_assert( m_OperContext);
		}
		result = OperationContext_CreateResult(operContext.get(), this);
		if (!result)
		{
			dms_assert(SuspendTrigger::DidSuspend() || WasFailed(FR_MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
		}
		dms_assert(m_OperContext); // Still in MainThread, no other access to m_Oper
	}
	catch (...)
	{
		CatchFail(FR_MetaInfo);
		return false;
	}
	if (! result)
	{
		dms_assert(SuspendTrigger::DidSuspend() || WasFailed(FR_MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
		return false;
	}

	dms_assert(m_Data);
	dms_assert(!SuspendTrigger::DidSuspend() && !WasFailed(FR_MetaInfo) );  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
	dms_assert(m_Data->IsCacheItem() || m_Data->IsPassor() || m_OperatorGroup->CanResultToConfigItem() || IsTmp());

	return true;
}

// =========================================  CalcResult

void FuncDC::CalcResultImpl(Explain::Context* context) const
{
#if defined(MG_DEBUG_DCDATA)
	DBG_START("FuncDc::CalcResult", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC);
#endif

	dms_assert(IsMainThread());
	dms_check_not_debugonly;

	dms_assert(!WasFailed(FR_Data));
	dms_assert(!SuspendTrigger::DidSuspend());
	dms_assert(m_Data);
	dms_assert(GetInterestCount());

//	SharedTreeItemInterestPtr promise = m_Data;

	StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
	InterestRetainContextBase base;

	if (!DoesHaveSupplInterest())
	{
		UpdateLock lock(this, actor_flag_set::AF_ChangingInterest);
		StartSupplInterest();
	}

	// ============== Call GetResult for each of the arguments

	bool result = true;
	try {
		auto argInterest = GetArgs(false, true);
		if (!argInterest)
		{
			dms_assert(m_Data->WasFailed(FR_Data) || SuspendTrigger::DidSuspend());
			return;
		}
		dms_assert(!SuspendTrigger::DidSuspend());

		UpdateMarker::ChangeSourceLock changeStamp(this, "FuncDC::MakeResult");

		dms_assert(!WasFailed(FR_Data)); // should have resulted in exit.

		UpdateLock lock(this, actor_flag_set::AF_CalculatingData);

		// ============== Call the actual operator

		std::shared_ptr<OperationContext> operContext;
		{
			leveled_critical_section::scoped_lock ocaLock(cs_OperContextAccess);
			operContext = m_OperContext;
			dms_assert(!operContext || operContext->getStatus() != task_status::cancelled); // ==  OperContext should call ResetOperContextImplAndStopSupplInterest before status==Canceled.
		}
		dms_assert(operContext || CheckDataReady(m_Data) || !IsNew());
		if (operContext && !operContext->IsScheduled())
		{
			OperationContext_AssignResult(operContext.get(), this);

			result = operContext->ScheduleCalcResult(context, std::move(*argInterest) );
			dms_assert(operContext->IsScheduled() || !result); // this should provide that AssignResult will not be called twice
			dms_assert(!(*argInterest).size() || !result);
			if (result)
				SuspendTrigger::MarkProgress();
		}

		dms_assert(!result || (operContext && operContext->IsScheduled()) || CheckDataReady(m_Data) || (!IsNew() && CheckCalculatingOrReady(GetCacheRoot(m_Data))));
	}
	catch (...)
	{
		CatchFail(FR_Data);
		return;
	}
	if (!result)
	{
		dms_assert(SuspendTrigger::DidSuspend() || WasFailed(FR_Data));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
		return;
	}

	dms_assert(m_Data);
	dms_assert(!SuspendTrigger::DidSuspend() && !WasFailed(FR_MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
	dms_assert(m_Data->IsCacheItem() || m_Data->IsPassor() || m_OperatorGroup->CanResultToConfigItem() || IsTmp());
}

// =========================================

// provide correct supplier for SymbDC 
// or operators with CanResultToConfigItem (SubItem, Domain/ValuesUnit, Literal)
// normal configRef does NOT include all sub-items as template instantiation does
// in order not to create recursive supplier dependencies when item refers to anchestor
// this responsibility is moved to consumers in order to avoid recursion
// DC consumers are (limited to) FuncDC and implementors of AbstrCalculator.

ActorVisitState FuncDC::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	DcRefListElem* dcRefElem = m_Args.get_ptr(); // points to curr DcRefListElem
	SharedStr firstArgValue;
	for (arg_index argNr = 0; dcRefElem; dcRefElem = dcRefElem->m_Next, ++argNr)
	{
		dms_assert(m_OperatorGroup);
		if (!Test(svf, SupplierVisitFlag::ReadyDcsToo) && !MustCalcArg(argNr, true, firstArgValue.begin()) && !m_OperatorGroup->MustSupplyTree(argNr, firstArgValue.begin()))
			continue;

		const DataController* dc = dcRefElem->m_DC;

		if (visitor(dc) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

		auto dcResult = dc->MakeResult();
		if (!dcResult)
			continue;

		if (visitor(dcResult) == AVS_SuspendedOrFailed) // TODO, REMOVE, WHY, FIND OUT IF dc DOESN'T ALREADY COVER THIS.
			return AVS_SuspendedOrFailed;

		if (m_OperatorGroup->MustSupplyTree(argNr, firstArgValue.begin())) // TODO: zoveel mogelijk wegwerken dmv substitutie van argumenten
		{
			if (dcResult->VisitConstVisibleSubTree(visitor) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
		if (argNr == 0 && m_OperatorGroup->HasDynamicArgPolicies())
			firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(dc->CalcCertainResult()->GetOld())))->GetIndexedValue(0);
	}

	for (auto& s : m_OtherSuppliersCopy)
		if (visitor.Visit(s) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

	return AVS_Ready;
}


// *****************************************************************************
// Section:     StringDC implementation
// *****************************************************************************

SharedTreeItem StringDC::MakeResult() const
{
	LispPtr keyExpr = GetLispRef();
	dms_assert(keyExpr.IsStrn());

	if (!m_Data)
	{
		const_cast<StringDC*>(this)->SetNew( 
			CreateConstParam<SharedStr>(
				SharedStr( keyExpr.GetStrnBeg(),  keyExpr.GetStrnEnd() )  
			).get_ptr()
		);
	}

	dms_assert(m_Data);

	return m_Data;
}

// *****************************************************************************
// Section:     NumbDC implementation
// *****************************************************************************

SharedTreeItem NumbDC::MakeResult() const
{
	dms_assert(GetLispRef().IsNumb());

	if (!m_Data)
		const_cast<NumbDC*>(this)->SetNew( 
			CreateConstParam<Float64>(
				GetLispRef().GetNumbVal()
			).get_ptr() 
		);

	dms_assert(m_Data);
	return m_Data;
}

// *****************************************************************************
// Section:     UI64DC implementation
// *****************************************************************************

SharedTreeItem UI64DC::MakeResult() const
{
	dms_assert(GetLispRef().IsUI64());

	if (!m_Data)
		const_cast<UI64DC*>(this)->SetNew(
			CreateConstParam<UInt64>(
				GetLispRef().GetUI64Val()
				).get_ptr()
		);

	dms_assert(m_Data);
	return m_Data;
}

// *****************************************************************************
// Section:     SymbDC implementation
// *****************************************************************************

SymbDC::SymbDC(LispPtr keyExpr, const TokenID fullNameID)
	:	DataController(keyExpr) 
	,	m_FullNameID(fullNameID)
{
//	dms_assert(keyExpr.GetSymbID() && keyExpr.GetSymbStr()[0]=='/'); // Only full names in substituted exprs
//	const TreeItem* root = m_Key.second;
}

SharedTreeItem SymbDC::MakeResult() const
{
	dms_assert( !SuspendTrigger::DidSuspend() );

	if (!m_Data)
	{
		SharedStr fullName = SharedStr(m_FullNameID); // overcome lock on m_FullNameID
		auto sourceItem = SessionData::Curr()->GetConfigRoot()->FindItem(fullName);\
/* NYI
		MG_CHECK(sourceItem);
		TreeItem* res = nullptr;
		if (IsDataItem(sourceItem))
		{
			StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
			res = CreateCacheDataItem(AsDataItem(sourceItem)->GetAbstrDomainUnit(), AsDataItem(sourceItem)->GetAbstrValuesUnit(), AsDataItem(sourceItem)->GetValueComposition());
		}
		else {
			CopyTreeContext ctc(nullptr, sourceItem, "", DataCopyMode::CopyExpr | DataCopyMode::DontCopySubItems | DataCopyMode::MakeEndogenous | DataCopyMode::MakePassor | DataCopyMode::DontUpdateMetaInfo);
			res = ctc.Apply();
		}
		const_cast<SymbDC*>(this)->SetNew(res); // copy will be done by UpdateMetaInfo
*/
		const_cast<SymbDC*>(this)->SetOld(sourceItem); // copy will be done by UpdateMetaInfo
	}
	dms_assert( !SuspendTrigger::DidSuspend() ); // Follows from previous assert and FindItem doesn't call MustSuspend();

	if (!m_Data)
	{
		auto msg = mySSPrintF("Cannot find Item %s", m_FullNameID.GetStr());
		Fail(msg, FR_MetaInfo);
		return nullptr;
	}

	dms_assert( m_Data );

	dms_assert(!IsTmp());

	dms_assert( !SuspendTrigger::DidSuspend() );
	return m_Data;
}

auto SymbDC::CalcResult(Explain::Context* context) const -> FutureData
{
	dms_check(GetInterestCount());
	dms_assert(!SuspendTrigger::DidSuspend());
	MakeResult();

	if (!m_Data)
		return nullptr;

	dms_assert(m_Data);

	dms_assert(IsOld());
	dms_assert(!IsTmp());

	UpdateMarker::ChangeSourceLock changeStamp(this, "SymbDC::CalcResult");

	FutureData resultHolder( this );
	dms_assert(!SuspendTrigger::DidSuspend());
	//		if (m_Data->m_State.GetTransState() < actor_flag_set::AF_Validating)
	//			m_Data->SuspendibleUpdate(PS_Committed);
	bool suspended = !m_Data->PrepareDataUsage(DrlType::Suspendible);

	if (m_Data->WasFailed())
		Fail(m_Data);

	if (suspended)
	{
		dms_assert(SuspendTrigger::DidSuspend() || m_Data->WasFailed());
		return nullptr;
	}
	dms_assert(CheckCalculatingOrReady(m_Data->GetCurrRangeItem()));

	dms_assert(!SuspendTrigger::DidSuspend());
	return resultHolder;
}

ActorVisitState SymbDC::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (base_type::VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	if (WasFailed(FR_Determine))
		return AVS_SuspendedOrFailed;

	auto data = MakeResult();
	if (!data || visitor(data) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;

	return AVS_Ready;
}

