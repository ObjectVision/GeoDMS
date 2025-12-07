// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
#include "utl/mySPrintF.h"
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
#include "OperationContext.h"
#include "Param.h"
#include "SessionData.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
extern leveled_std_section cs_ThreadMessing;

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

#include "TreeItemContextHandle.h"

FuncDC::FuncDC(LispPtr keyExpr,	const AbstrOperGroup* og)
	:	DataController(keyExpr)
	,	m_OperatorGroup(og)
{
	dms_assert(og && (og->MustCacheResult() || !og->CanResultToConfigItem()));

	DBG_START("FuncDC", "ctor", false);
	DBG_TRACE(("keyExpr = %s", AsFLispSharedStr(keyExpr, FormattingFlags::ThousandSeparator).c_str()));

	if (og->IsDepreciated())
		reportF(SeverityTypeID::ST_Warning, "depreciated operator %s used: %s.", og->GetName(), og->GetObsoleteMsg());

	if (og->IsObsolete())
		throwErrorF("FuncDC", "obsolete operator %s used: %s.", og->GetName(), og->GetObsoleteMsg());

	assert(GetLispRef().IsRealList());    // no EndP allowed
	assert(GetLispRef().Left().IsSymb()); // operator or calculation scheme call

	if (og->IsTransient())
		m_State.Set(DCF_CanChange);
//	if (og->HasTemplArg())
//		m_State.Set(DCF_IsTmp);

	// for each subexpr in keyExpr do add arg
	OwningPtr<DcRefListElem>* nextArgPtr = &m_Args;
	for (LispPtr tailPtr = keyExpr.Right(); !tailPtr.EndP(); tailPtr = tailPtr.Right()) 
	{
		DBG_TRACE(("arg = %s", AsFLispSharedStr(tailPtr->Left(), FormattingFlags::ThousandSeparator).c_str()));
		DcRefListElem* dcRef = new DcRefListElem;
		nextArgPtr->assign(dcRef);
		MG_CHECK(tailPtr->Left());
		dcRef->m_DC = GetOrCreateDataController(tailPtr->Left());
		assert(dcRef->m_DC);
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

std::shared_ptr<OperationContext> FuncDC::resetOperContextImpl() const
{
	assert(cs_ThreadMessing.isLocked());
	leveled_critical_section::scoped_lock ocaLock(cs_OperContextAccess);

	assert(
		(!m_InterestCount) 
	||	!IsNew() 
	||	GetNew()->GetIsInstantiated()
	||	GetNew()->WasFailed(FailType::Data)
	||	CheckDataReady(GetNew())
	||	DSM::IsCancelling()
	||	m_State.GetProgress() == ProgressState::None // Just invalidated.
	);

	std::shared_ptr<OperationContext> operContext = std::move(m_OperContext);
	if (operContext) 
	{
		operContext->m_FuncDC.reset();
	}
	assert(!m_OperContext);
	return operContext;
}

garbage_can FuncDC::resetOperContextImplAndStopSupplInterest() const
{
	auto res = StopSupplInterest();

	res |= resetOperContextImpl();
	return res;
}

garbage_can FuncDC::ResetOperContextImplAndStopSupplInterest() const
{
	auto res = StopSupplInterest();

	leveled_critical_section::scoped_lock octmLock(cs_ThreadMessing);
	res |= resetOperContextImpl();
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

	auto ocSPtr = GetOperContext();
	if (!ocSPtr)
		return false;

	auto status = ocSPtr->GetStatus();
	return status != task_status::none && status != task_status::exception && status != task_status::cancelled;
}

void FuncDC::DoInvalidate() const
{
	m_Operator.reset();
	CancelOperContext();
	dms_assert(!IsCalculating());

	base_type::DoInvalidate();

	dms_assert(!m_Data);										 // dropped by base_type::DoInvalidate
	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());	 // set by base_type::DoInvalidate
	dms_assert(!IsCalculating());
}

garbage_can FuncDC::StopInterest () const noexcept
{ 
	auto garbage = ResetOperContextImplAndStopSupplInterest();
	garbage |= DataController::StopInterest(); 
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

// Postcondition of CalcResult(true):
//		Null is returned OR calculation has started that will make m_Data have valid data such that it can be accessed (DataReadLock can be set) or become failed
//		and used as arg to calculate a result for a referring FuncDC
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

	const TreeItem* dContext = m_Data;

	assert(IsMetaThread());
#endif

	DetermineState(); // may trigger DoInvalidate -> reset m_Data, only MainThread may re-MakeResult
	if (WasFailed(FailType::MetaInfo))
		return nullptr;

	static UInt32 debug_counter = 0;
	DBG_TRACE(("%s m_Data %s m_OperContext %s", debug_counter++, bool(m_Data), bool(m_OperContext)));

	if (!m_Data) 
	{
		DBG_TRACE(("MakeResult starts"));
		if (!MakeResultImpl())
		{
			assert(WasFailed(FailType::MetaInfo)); // MakeResult cannot suspend
			assert(!DoesHaveSupplInterest());
			return nullptr;
		}
		MG_CHECK(m_Data);
		DBG_TRACE(("MakeResult completed well"));
	}
	assert(m_Data);
	assert(!IsNew() || m_Data->IsCacheRoot());

	if (m_Data && m_Data->WasFailed(FailType::MetaInfo))
		Fail(m_Data);

	if (WasFailed(FailType::MetaInfo))
		return nullptr;

	actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("Actor::DecInterestCount") sg_ActorLockMap, this);

	if (GetInterestCount())
	{
		if (DoesHaveSupplInterest() && m_OtherSuppliers.size())
			RestartSupplInterestIfAny();
	}
	else
	{
		assert(!DoesHaveSupplInterest());
	}
	return m_Data;
}

auto FuncDC::CallCalcResult(std::shared_ptr<Explain::Context> context) const -> FutureData
{
#if defined(MG_DEBUG_DCDATA)
	DBG_START("FuncDc::CallCalcResult", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC);

	const TreeItem* dContext = m_Data;

	assert(IsMetaThread());
	assert(!SuspendTrigger::DidSuspend());
#endif

	DetermineState(); // may trigger DoInvalidate -> reset m_Data, only MainThread may re-MakeResult
	if (WasFailed(FailType::MetaInfo))
		return {};

#if defined(MG_DEBUG)
//	static TokenID testsID = GetTokenID("tests");
//	if (dContext && dContext->m_BackRef && dContext->m_BackRef->GetID() == testsID)
//		__debugbreak();
#endif

	FutureData thisFutureResult = this;

	// precondition if doCalc: Interest, SupplInterest, Not FailType::MetaInfo, nor Args; FailType::Data may occur in worker threads, but then re-Make is futile.
//	dms_assert(m_InterestCount);

	static UInt32 debug_counter = 0;
	DBG_TRACE(("%s m_Data %s m_OperContext %s context %s ", debug_counter++, bool(m_Data), bool(m_OperContext), bool(context)));

	if (!m_Data)
	{
		DBG_TRACE(("MakeResult starts"));
		if (!MakeResultImpl())
		{
			assert(WasFailed(FailType::MetaInfo) ); // MakeResult cannot suspend
			assert(!m_OperContext);
			return {};
		}
		assert(m_Data);
		DBG_TRACE(("MakeResult completed well"));
	}
	m_Data->UpdateMetaInfo();

	if (m_Data->WasFailed(FailType::Data))
		Fail(m_Data);

	if (WasFailed(FailType::Data) && !context || WasFailed(FailType::MetaInfo))
		return {};

	assert(m_OperatorGroup);
	if (SuspendTrigger::BlockerBase::IsBlocked())
	{
		if (m_OperatorGroup->IsBetterNotInMetaScripting())
		{
			auto blockingActionContext = SuspendTrigger::BlockerBase::GetCurrBlockingAction();
			if (blockingActionContext && *blockingActionContext != '@')
				reportF(SeverityTypeID::ST_Warning, "operator %s is not suitable for processing %s"
					, m_OperatorGroup->GetName()
					, blockingActionContext
				);
		}
	}
	assert(GetInterestCount());
	assert(m_Data->IsCacheItem() || m_Data->IsPassor()|| m_OperatorGroup->CanResultToConfigItem() );

	if (context)
	{
		if (!m_OperatorGroup->CanExplainValue())
			context.reset();
	}

	#if defined(MG_DEBUG_UPDATESOURCE)
		SupplInclusionTester guaranteeThatCompleteSupplRelIsTransitive(this);
	#endif

	DBG_TRACE(("CallCalcResult"));

	assert(GetInterestCount()); 

	assert(!IsTmp());
	bool mustStartCalc = (context != nullptr);
	if (!mustStartCalc)
		if (IsNew() && GetOperator()->CanRunParallel())
			mustStartCalc = !IsAllInterestedCalculatingOrDataReady(m_Data);
		else
			mustStartCalc = !IsAllDataReady(m_Data); // condition required for operations such as parse_xml as first argument of a SubItem

	if (mustStartCalc)
	{
		assert(m_Data->GetInterestCount());

		CallCalcResultImpl(context);
		if (!m_Data || SuspendTrigger::DidSuspend())
		{
			dms_assert(SuspendTrigger::DidSuspend() || WasFailed());
			return {}; // maybe suspended or failed
		}
		assert(!SuspendTrigger::DidSuspend());
		assert(m_OperContext || IsDataReady(m_Data) || m_Data->WasFailed(FailType::Data) || SuspendTrigger::DidSuspend());
	}
	return thisFutureResult;
}

/********** Interface **********/

using ArgClsSeqType = std::vector<const Class*>;


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
//		case oper_arg_policy::calc_at_subitem:
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
		assert(IsMetaThread());
		ArgClsSeqType operandTypeSeq;
		if (WasFailed(FailType::MetaInfo))
			ThrowFail();

		arg_index argCount = 0;
		for (const DcRefListElem* argIter = m_Args; argIter; argIter = argIter->m_Next, ++argCount)
		{
			const DataController* argDC = argIter->m_DC;
			if (argDC->WasFailed(FailType::MetaInfo))
			{
				Fail(argDC);
				ThrowFail();
			}
			const Class* resCls = argDC->GetResultCls();
			assert(resCls);
			operandTypeSeq.push_back( resCls );
		}

		dbg_assert( operandTypeSeq.size() == GetNrArgs() );
		m_Operator = m_OperatorGroup->FindOper(argCount, begin_ptr( operandTypeSeq ));
		assert(m_Operator);
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
	assert(IsMetaThread());
	assert(!SuspendTrigger::DidSuspend()); // PRECONDITION

	DBG_START("FuncDc::GetArgs", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC && doCalcData);

	assert(!doCalcData || GetInterestCount());
	assert(!doCalcData || DoesHaveSupplInterest());

	arg_index currArg = 0;
	ArgRefs argSeq; argSeq.reserve(GetNrArgs());

	SharedStr firstArgValue; // may be filled with first arg value that encoded the role of consecutive arguments for OperatorGroups with Dyanmic Arguments
	for (const DcRefListElem* argIter = m_Args; argIter; ++currArg, argIter = argIter->m_Next) 
	{
		assert(argIter->m_DC); // DcRefListElem invariant

		bool mustCalcArg = MustCalcArg(currArg, doCalcData, firstArgValue.begin());

		ArgRef argRef;
		if (!mustCalcArg) {
			argRef.emplace<SharedTreeItem>(argIter->m_DC->MakeResult()); // post:CheckCalculatingOrReady
			assert(!SuspendTrigger::DidSuspend()); // POSTCONDITION of argIter->m_DC->MakeResult();
		} else {
			assert(!doCalcData || argIter->m_DC->GetInterestCount());
			FutureData fd = argIter->m_DC; fd = argIter->m_DC->CalcResultWithValuesUnits();
			MakeMax(this->m_PhaseNumber, argIter->m_DC->m_PhaseNumber);
			assert(!fd || argIter->m_DC->GetInterestCount());
			if (SuspendTrigger::DidSuspend())
				return {};
			assert(!fd || CheckCalculatingOrReady(fd->GetOld()->GetCurrRangeItem()) || fd->WasFailed(FailType::Data) || fd->GetOld()->WasFailed(FailType::Data) 
			|| dynamic_cast<const FuncDC*>(fd.get_ptr()) && dynamic_cast<const FuncDC*>(fd.get_ptr())->m_OperatorGroup->GetNameID() == token::subitem); // the latter can refer to a sub-items of a FenceContainer that has a upstream RangeItem
			argRef.emplace<FutureData>(std::move(fd));
			if (currArg == 0 && m_OperatorGroup->HasDynamicArgPolicies())
				firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(argIter->m_DC->GetOld())))->GetIndexedValue(0);
		}

		auto argItem = GetItem(argRef);
		if (argItem) {
			if (doUpdateMetaInfo)
				argItem->UpdateMetaInfo();
			dms_assert(argItem->m_State.GetProgress() >= ProgressState::MetaInfo || argItem->WasFailed(FailType::MetaInfo));
			if (argItem->WasFailed(mustCalcArg))
				argIter->m_DC->Fail(argItem, mustCalcArg ? FailType::Data : FailType::MetaInfo);
		}

		if (argIter->m_DC->WasFailed(mustCalcArg))
			Fail(argIter->m_DC, doCalcData ? FailType::Data : FailType::MetaInfo);

		dms_assert(argItem || WasFailed(doCalcData)); // POSTCONDITION of argIter->m_DC->GetResult();

		if (WasFailed(doCalcData))
			return {};
		dms_assert(argItem && !argItem->WasFailed(FailType::MetaInfo));
		argSeq.emplace_back(std::move(argRef));
	}
	return argSeq;
}

void MarkCacheItems(const FuncDC* funcDC)
{
	if (funcDC->IsNew())
	{
		// mark TimeStamp of result
		TreeItem* cacheRoot = funcDC->GetNew();
		TimeStamp ts = funcDC->GetLastChangeTS();
		phase_number fn = funcDC->GetPhaseNumber();
		for (TreeItem* cacheItem = cacheRoot; cacheItem; cacheItem = cacheRoot->WalkCurrSubTree(cacheItem))
		{
			cacheItem->MarkTS(ts);
			assert(cacheItem->m_PhaseNumber == 0);
			cacheItem->m_PhaseNumber = fn;
		}
	}
}

bool FuncDC_CreateResult(const FuncDC* funcDC)
{
	DBG_START("FuncDC", "CreateResult", MG_DEBUG_FUNCDC);
	DBG_TRACE(("FuncDC: %s", funcDC->md_sKeyExpr));

	assert(IsMetaThread());
	assert(funcDC);

	MG_DEBUGCODE(const TreeItem * oldItem = funcDC->GetOld());

	SuspendTrigger::FencedBlocker lockSuspend("FuncDC_CreateResult");

	assert(!funcDC->WasFailed(FailType::MetaInfo));
	assert(!SuspendTrigger::DidSuspend());

	TreeItemDualRef& resultHolder = *const_cast<FuncDC*>(funcDC);
	try {
		OArgRefs args = funcDC->GetArgs(true, false); // TODO, OPTIMIZE: CreateResult also sometimes calls GetArgs(false).

		assert(!SuspendTrigger::DidSuspend());
		if (!args)
		{
			assert(funcDC->WasFailed(FailType::MetaInfo));
			return false;
		}
		auto oper = funcDC->GetCurrOperator();
		if (!oper) {
			oper = funcDC->m_OperatorGroup->FindOperByArgs(*args);
			if (funcDC)
				funcDC->SetOperator(oper);
		}
		assert(oper);

		assert(!funcDC->WasFailed(FailType::MetaInfo));
		assert(!SuspendTrigger::DidSuspend());

		oper->CreateResultCaller(resultHolder, *args, funcDC->GetLispRef().Right()); // may set the fence number of funcDC
	}
	catch (...)
	{
		if (resultHolder.IsNew())
			resultHolder->CatchFail(FailType::MetaInfo); // also calls resultHolder->StopSupplInterest() (the resulting data).
		resultHolder.CatchFail(FailType::MetaInfo);
	}

	bool resultingFlag = !resultHolder.WasFailed(FailType::MetaInfo);
	MarkCacheItems(funcDC);

	if (resultHolder)
	{
		if (!resultHolder->GetDynamicObjClass()->IsDerivedFrom(funcDC->m_Operator->GetResultClass()))
		{
			auto msg = mySSPrintF("result of %s is of type %s, expected type: %s"
				, funcDC->m_OperatorGroup->GetName()
				, resultHolder->GetCurrentObjClass()->GetName()
				, funcDC->m_Operator->GetResultClass()->GetName()
			);
			resultHolder->Fail(msg, FailType::MetaInfo);
		}
		if (resultHolder->WasFailed(FailType::MetaInfo))
		{
			resultHolder.Fail(resultHolder.GetOld(), FailType::MetaInfo);
			resultingFlag = false;
		}
	}

	assert(resultingFlag != (SuspendTrigger::DidSuspend() || resultHolder.WasFailed(FailType::MetaInfo)));
	return resultingFlag;
}


bool FuncDC::MakeResultImpl() const
{
	assert(IsMetaThread());
	dms_check_not_debugonly; 

	assert(!WasFailed(FailType::MetaInfo));
	assert(!SuspendTrigger::DidSuspend());

	StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
	InterestRetainContextBase base;
	assert(!m_OperContext);
	// ============== Call GetResult for each of the arguments

#if defined(MG_DEBUG_DCDATA)
	DBG_START("FuncDc::MakeResultImpl", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC);
#endif
	if (WasFailed(FailType::MetaInfo))
		return false;

	bool result = false;
	try {
		UpdateMarker::ChangeSourceLock changeStamp(this, "FuncDC::MakeResult");
		UpdateLock lock(this, actor_flag_set::AF_UpdatingMetaInfo);
	
		// ============== Call the actual operator
		result = FuncDC_CreateResult(this);
		assert(result || SuspendTrigger::DidSuspend() || WasFailed(FailType::MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
	}
	catch (...)
	{
		assert(!result);
		CatchFail(FailType::MetaInfo);
	}
	if (! result)
	{
		assert(SuspendTrigger::DidSuspend() || WasFailed(FailType::MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
		return false;
	}

	assert(m_Data);
	assert(!SuspendTrigger::DidSuspend() && !WasFailed(FailType::MetaInfo) );  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
	assert(m_Data->IsCacheItem() || m_Data->IsPassor() || m_OperatorGroup->CanResultToConfigItem() || IsTmp());

	return true;
}

// =========================================  CallCalcResult

void FuncDC::CallCalcResultImpl(std::shared_ptr<Explain::Context> context) const
{
#if defined(MG_DEBUG_DCDATA)
	DBG_START("FuncDc::CallCalcResult", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC);
#endif

	assert(IsMetaThread());
	dms_check_not_debugonly;

	assert(!WasFailed(FailType::Data));
	assert(!SuspendTrigger::DidSuspend());
	assert(m_Data);
	assert(GetInterestCount());
	assert(!IsTmp());

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
		auto argRefs= GetArgs(false, true);
		if (!argRefs)
		{
			assert(m_Data->WasFailed(FailType::Data) || SuspendTrigger::DidSuspend());
			return;
		}
		assert(!SuspendTrigger::DidSuspend());

		UpdateMarker::ChangeSourceLock changeStamp(this, "FuncDC::MakeResult");

		assert(!WasFailed(FailType::Data)); // should have resulted in exit.

		UpdateLock lock(this, actor_flag_set::AF_CalculatingData);

		// ============== Call the actual operator

		// todo: merge ScheduleCalcResult into ctor of OperationContext, 
		// todo: add separate interface for reactivation with a specific ExplainValue::Context
		auto operContext = this->GetOperContext();
		if (!operContext)
		{
			operContext = OperationContext::CreateFuncDC(this);
			m_OperContext = operContext;
		}
		result = GetOperator()->PreCalcUpdate(*const_cast<FuncDC*>(this), *argRefs);
		if (!result)
		{
			assert(SuspendTrigger::DidSuspend());
			return;
		}
		result = operContext->GetStatus() != task_status::exception;
		if (operContext->GetStatus() == task_status::none || context)
			if (!m_OperContext->ScheduleCalcResult(std::move(*argRefs), context)) // connection with m_OperContext must have been established before scheduling, as direct-running -> done -> undo FuncDC -> OC relation
				result = false;

		assert(!IsNew() || GetNew()->m_LastChangeTS == m_LastChangeTS); // further changes in the resulting data must have caused resultHolder to invalidate, as IsNew results are passive

			// this function only runs in the main thread and should not be re-entrant, so we can safely assume that no other thread is filling the m_OperContext
		if (result)
			SuspendTrigger::MarkProgress();

		assert(!result || operContext || CheckDataReady(m_Data) || (!IsNew() && CheckCalculatingOrReady(GetCacheRoot(m_Data))));
	}
	catch (...)
	{
		CatchFail(FailType::Data);
		return;
	}
	if (!result)
	{
		if (GetOld()->WasFailed())
			Fail(GetOld());
		assert(SuspendTrigger::DidSuspend() || WasFailed(FailType::Data));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
		return;
	}

	assert(m_Data);
	assert(!SuspendTrigger::DidSuspend() && !WasFailed(FailType::MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
	assert(m_Data->IsCacheItem() || m_Data->IsPassor() || m_OperatorGroup->CanResultToConfigItem() || IsTmp());
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
	DcRefListElem* dcRefElem = m_Args.get_ptr(); // points to currently uniquely owned DcRefListElem
	SharedStr firstArgValue;  // may be filled with first arg value that encoded the role of consecutive arguments for OperatorGroups with Dyanmic Arguments
	for (arg_index argNr = 0; dcRefElem; dcRefElem = dcRefElem->m_Next, ++argNr)
	{
		auto firstArgValueCPtr = firstArgValue.cbegin();
		assert(m_OperatorGroup);
		if (!Test(svf, SupplierVisitFlag::ReadyDcsToo)
			&& !MustCalcArg(argNr, true, firstArgValueCPtr)
			&& !m_OperatorGroup->MustSupplyTree(argNr, firstArgValueCPtr))
			continue;

		const DataController* dc = dcRefElem->m_DC; // borrow shared owned dc;

		if (visitor(dc) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

		auto dcResult = dc->MakeResult();
		if (!dcResult)
			continue;

		if (visitor(dcResult) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

		if (m_OperatorGroup->MustSupplyTree(argNr, firstArgValueCPtr) || 
			Test(svf, SupplierVisitFlag::ScanSupplTree) && m_OperatorGroup->IsSubItemRoot(argNr, firstArgValueCPtr)) 
		{
			if (dcResult->VisitConstVisibleSubTree(visitor) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
		if (argNr == 0 && m_OperatorGroup->HasDynamicArgPolicies())
			firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(dc->CalcCertainResult()->GetOld())))->GetIndexedValue(0);
	}

	for (auto& s : m_OtherSuppliers)
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
				SharedStr(CharPtrRange( keyExpr.GetStrnBeg(),  keyExpr.GetStrnEnd() ))
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
	assert(GetLispRef().IsNumb());

	if (!m_Data)
		const_cast<NumbDC*>(this)->SetNew( 
			CreateConstParam<Float64>(
				GetLispRef().GetNumbVal().m_Value
			).get_ptr() 
		);

	assert(m_Data);
	return m_Data;
}

// *****************************************************************************
// Section:     UI64DC implementation
// *****************************************************************************

SharedTreeItem UI64DC::MakeResult() const
{
	assert(GetLispRef().IsUI64());

	if (!m_Data)
		const_cast<UI64DC*>(this)->SetNew(
			CreateConstParam<UInt64>(
				GetLispRef().GetUI64Val()
				).get_ptr()
		);

	assert(m_Data);
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
		auto curr = SessionData::Curr();
		MG_CHECK(curr);
		auto sourceItem = curr->GetConfigRoot()->FindItem(fullName);
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
		Fail(msg, FailType::MetaInfo);
		return nullptr;
	}

	dms_assert( m_Data );

	dms_assert(!IsTmp());

	dms_assert( !SuspendTrigger::DidSuspend() );
	return m_Data;
}

auto SymbDC::CallCalcResult(std::shared_ptr<Explain::Context> context) const -> FutureData
{
	dms_check(GetInterestCount());
	dms_assert(!SuspendTrigger::DidSuspend());
	MakeResult();

	if (!m_Data)
		return nullptr;

	dms_assert(m_Data);

	dms_assert(IsOld());
	dms_assert(!IsTmp());

	UpdateMarker::ChangeSourceLock changeStamp(this, "SymbDC::CallCalcResult");

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
	if (WasFailed(FailType::Determine))
		return AVS_SuspendedOrFailed;

	auto data = MakeResult();
	if (!data || visitor(data) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;

	return AVS_Ready;
}

