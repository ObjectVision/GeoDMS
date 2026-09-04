// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "MoreDataControllers.h"

#include "Parallel.h" // THREAD_LOCAL

#include "act/ActorSet.h"
#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/DebugContext.h"
#include "dbg/SeverityType.h"
#include "vt/StringArray.h"
#include "ser/FileStreamBuff.h"
#include "utl/StrFormat.h"
#include "utl/swap.h"
#include "xct/DmsException.h"

#include "LockLevels.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DC_Ptr.h"
#include "FreeDataManager.h"
#include "ItemLocks.h"
#include "LispTreeType.h"
#include "Operator.h"
#include "OperationContext.h"
#include "OperSignature.h" // MG_DEBUG op-sig §9 defense #1: SigUnitChecker_VerifyApplication
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
	return std::get<1>(ar).get();
}

const Actor* GetStatusActor(const ArgRef& ar)
{
	if (ar.index() == 0)
	{
		return std::get<0>(ar).get_ptr();
	}
	return std::get<1>(ar).get();
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
	DBG_TRACE(("keyExpr = {}", AsFLispSharedStr(keyExpr, FormattingFlags::ThousandSeparator).c_str()));

	if (og->IsDepreciated())
		reportF(SeverityTypeID::ST_Warning, "depreciated operator {} used: {}.", og->GetName(), og->GetObsoleteMsg());

	if (og->IsObsolete())
		throwErrorF("FuncDC", "obsolete operator {} used: {}.", og->GetName(), og->GetObsoleteMsg());

	assert(GetLispRef().IsRealList());    // no EndP allowed
	assert(GetLispRef().Left().IsSymb()); // operator or calculation scheme call

	if (og->IsTransient())
		m_State.Set(DCF_CanChange);
//	if (og->HasTemplArg())
//		m_State.Set(DCF_IsTmp);

	// for each subexpr in keyExpr do add arg
	std::unique_ptr<DcRefListElem>* nextArgPtr = &m_Args;
	for (LispPtr tailPtr = keyExpr.Right(); !tailPtr.EndP(); tailPtr = tailPtr.Right()) 
	{
		DBG_TRACE(("arg = {}", AsFLispSharedStr(tailPtr->Left(), FormattingFlags::ThousandSeparator).c_str()));
		DcRefListElem* dcRef = new DcRefListElem;
		nextArgPtr->reset(dcRef);
		MG_CHECK(tailPtr->IsOwned());
		MG_CHECK(tailPtr->Left());
		MG_CHECK(tailPtr->Left()->IsOwned());
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
	DMS_ENTERS(ord_level_type::OperContextAccess, dms_exclusive_v);
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
	||	SessionData::IsCurrCancelling()
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
	DMS_ENTERS(ord_level_type::ThreadMessing, dms_exclusive_v);
	auto res = StopSupplInterest();

	leveled_critical_section::scoped_lock octmLock(cs_ThreadMessing);
	res |= resetOperContextImpl();
	return res;
}

void FuncDC::CancelOperContext() const
{
	// the operation context is destroyed here
	DMS_ENTERS_ITEM(ord_level_type::ItemRegister, dms_exclusive_v);
	auto operContext = ResetOperContextImplAndStopSupplInterest();
}

bool FuncDC::IsCalculating() const
{
	// GetOperContext (80), then GetStatus (75)
	DMS_ENTERS(ord_level_type::ThreadMessing, dms_exclusive_v);
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
	DMS_ENTERS_ITEM(ord_level_type::ItemRegister, dms_exclusive_v);
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

	const TreeItem* dContext = m_Data.get().get();

	assert(IsMetaThread());
#endif

	DetermineState(); // may trigger DoInvalidate -> reset m_Data, only MainThread may re-MakeResult
	if (WasFailed(FailType::MetaInfo))
		return {};

	static UInt32 debug_counter = 0;
	DBG_TRACE(("{} m_Data {} m_OperContext {}", debug_counter++, bool(m_Data), bool(m_OperContext)));

	if (!m_Data) 
	{
		DBG_TRACE(("MakeResult starts"));
		if (!MakeResultImpl())
		{
			assert(WasFailed(FailType::MetaInfo)); // MakeResult cannot suspend
			assert(!DoesHaveSupplInterest());
			return {};
		}
		MG_CHECK(m_Data);
		DBG_TRACE(("MakeResult completed well"));
	}
	assert(m_Data);
	auto curr = GetCurr(); // owning snapshot; null if a weak arm (config item) expired
	if (!curr)
	{
		Fail(SharedStr("result item no longer exists"), FailType::MetaInfo); // callers rely on: null result => WasFailed or DidSuspend
		return {};
	}
	assert(!IsNew() || curr->IsCacheRoot());

	if (curr->WasFailed(FailType::MetaInfo))
		Fail(curr.get());

	if (WasFailed(FailType::MetaInfo))
		return {};

	for (auto* arg = m_Args.get(); arg; arg = arg->m_Next.get())
		if (arg->m_DC->m_State.Get(actor_flag_set::AF_IntegrityChecked))
			m_State.Set(actor_flag_set::AF_IntegrityChecked);

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
	return curr;
}

auto FuncDC::CallCalcResult(std::shared_ptr<Explain::Context> context) const -> FutureData
{
#if defined(MG_DEBUG_DCDATA)
	DBG_START("FuncDc::CallCalcResult", md_sKeyExpr.c_str(), MG_DEBUG_FUNCDC);

	const TreeItem* dContext = m_Data.get().get();

	assert(IsMetaThread());
	assert(!SuspendTrigger::DidSuspend());
#endif

	DetermineState(); // may trigger DoInvalidate -> reset m_Data, only MainThread may re-MakeResult
	if (WasFailed(FailType::MetaInfo))
		return {};

	FutureData thisFutureResult = this;

	// precondition if doCalc: Interest, SupplInterest, Not FailType::MetaInfo, nor Args; FailType::Data may occur in worker threads, but then re-Make is futile.
//	dms_assert(m_InterestCount);

	static UInt32 debug_counter = 0;
	DBG_TRACE(("{} m_Data {} m_OperContext {} context {} ", debug_counter++, bool(m_Data), bool(m_OperContext), bool(context)));

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
	auto curr = GetCurr(); // owning snapshot; null if a weak arm (config item) expired
	if (!curr)
	{
		Fail(SharedStr("result item no longer exists"), FailType::MetaInfo); // callers rely on: null result => WasFailed or DidSuspend
		return {};
	}
	curr->UpdateMetaInfo();

	if (curr->WasFailed(FailType::Data))
		Fail(curr.get());

	if ((WasFailed(FailType::Data) && !context) || WasFailed(FailType::MetaInfo))
		return {};

	assert(m_OperatorGroup);
	if (SuspendTrigger::BlockerBase::IsBlocked())
	{
		if (m_OperatorGroup->IsBetterNotInMetaScripting())
		{
			auto blockingActionContext = SuspendTrigger::BlockerBase::GetCurrBlockingAction();
			if (blockingActionContext && *blockingActionContext != '@')
				reportF(SeverityTypeID::ST_Warning, "operator {} is not suitable for processing {}"
					, m_OperatorGroup->GetName()
					, blockingActionContext
				);
		}
	}
	assert(GetInterestCount());
	assert(curr->IsCacheItem() || curr->IsPassor()|| m_OperatorGroup->CanResultToConfigItem() );

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
	{
		if (IsNew() && GetOperator()->CanRunParallel())
			mustStartCalc = !IsAllInterestedCalculatingOrDataReady(curr.get());
		else
			mustStartCalc = !IsAllDataCurrStandby(curr.get()); // condition required for operations such as parse_xml as first argument of a SubItem
	}

	if (mustStartCalc)
	{
		assert(curr->GetInterestCount());

		CallCalcResultImpl(context);
		if (!m_Data || SuspendTrigger::DidSuspend())
		{
			dms_assert(SuspendTrigger::DidSuspend() || WasFailed());
			return {}; // maybe suspended or failed
		}
		assert(!SuspendTrigger::DidSuspend());
		assert(m_OperContext || IsDataReady(curr.get()) || curr->WasFailed(FailType::Data) || SuspendTrigger::DidSuspend());
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

// =========================================  GetImpliedChecks (#1182)

// The set of IntegrityCheck conditions that evaluating this DC's key is certain to evaluate:
// the union over the args the engine calculates before the operator runs (MustCalcArg, as
// GetArgs applies it), plus this key's own condition when it is an integrity_check application.
// TreeItem_CreateCheckedExpr consults it to skip a wrap whose condition an embedded node
// already enforces. The DC graph mirrors the calculation DAG (FuncDC's ctor makes a DC per
// argument), so each node folds once per DC lifetime and a redundant guard is found at any
// depth; the set dies with its DC, so no separate registry or eviction is needed.

static check_set_ptr GetSharedEmptyCheckSet()
{
	// holds no LispRefs, so its CRT-exit destruction stays clear of the LispObj caches
	static check_set_ptr s_EmptyCheckSet = make_SharedThing<check_set>();
	return s_EmptyCheckSet;
}

// Split a condition on its conjunction spine: 'a && b && c' parses to and(and(a, b), c), and
// enforcing that for every element enforces a, b and c individually. Non-conjunctions yield
// themselves, so every condition has a normal form of one or more atoms.
static void CollectCheckAtoms(LispPtr cond, std::vector<LispPtr>& atoms)
{
	std::vector<LispPtr> todo; // explicit stack: a generated conjunction spine can be long
	todo.push_back(cond);
	while (!todo.empty())
	{
		LispPtr curr = todo.back();
		todo.pop_back();
		if (curr.IsRealList() && curr.Left().IsSymb() && curr.Left().GetSymbID() == token::and_)
		{
			for (LispPtr cursor = curr.Right(); cursor.IsRealList(); cursor = cursor.Right())
				todo.push_back(cursor.Left());
			continue;
		}
		atoms.push_back(curr);
	}
}

void InsertCheckAtoms(check_set& dest, LispPtr cond)
{
	std::vector<LispPtr> atoms;
	CollectCheckAtoms(cond, atoms);
	for (auto atom : atoms)
		dest.insert(LispRef(atom));
}

bool AreCheckAtomsImplied(const check_set& enforced, LispPtr cond)
{
	std::vector<LispPtr> atoms;
	CollectCheckAtoms(cond, atoms);
	if (atoms.empty())
		return false; // nothing recognisable to enforce: keep the guard
	for (auto atom : atoms)
		if (!enforced.contains(atom))
			return false;
	return true;
}

// an arg's checks only count when the engine calculates that arg before the operator runs;
// dynamic policies depend on arg 0's data, which is not consulted here: treat those args as
// non-contributing, which errs towards an extra wrap
static bool DataController_ArgContributes(const FuncDC* funcDC, arg_index argNr)
{
	if (funcDC->m_OperatorGroup->HasDynamicArgPolicies())
		return false;
	return funcDC->MustCalcArg(argNr, true, nullptr);
}

check_set_ptr DataController::GetImpliedChecks() const
{
	assert(IsMetaThread()); // same discipline as DC creation; keeps m_ImpliedChecks lock-free

	if (m_ImpliedChecks)
		return m_ImpliedChecks;

	struct frame_type { const FuncDC* funcDC; const DcRefListElem* nextArg; arg_index argNr = 0; };
	std::vector<frame_type> stack; // explicit stack: key expressions nest deeper than the C-stack allows

	auto scheduleOrResolve = [&stack](const DataController* dc)
	{
		if (dc->m_ImpliedChecks)
			return;
		auto funcDC = dynamic_cast<const FuncDC*>(dc);
		if (funcDC && funcDC->GetArgList())
			stack.emplace_back(frame_type{ funcDC, funcDC->GetArgList() });
		else
			dc->m_ImpliedChecks = GetSharedEmptyCheckSet(); // sourceDescr, symbol, literal or nullary application
	};

	scheduleOrResolve(this);
	while (!stack.empty())
	{
		frame_type& top = stack.back();
		if (top.nextArg)
		{
			const DataController* argDC = top.nextArg->m_DC.get();
			arg_index argNr = top.argNr;
			top.nextArg = top.nextArg->m_Next.get(); ++top.argNr;
			if (DataController_ArgContributes(top.funcDC, argNr))
				scheduleOrResolve(argDC); // may push a frame and invalidate top: not used below
			continue;
		}

		// post-order position: all contributing args of top.funcDC are folded
		const FuncDC* funcDC = top.funcDC;

		check_set_ptr singleContribution;
		bool multipleContributions = false;
		arg_index argNr = 0;
		for (const DcRefListElem* argIter = funcDC->GetArgList(); argIter && !multipleContributions; argIter = argIter->m_Next.get(), ++argNr)
		{
			if (!DataController_ArgContributes(funcDC, argNr))
				continue;
			const check_set_ptr& argChecks = argIter->m_DC->m_ImpliedChecks;
			assert(argChecks);
			if (argChecks->thing.empty() || argChecks == singleContribution)
				continue;
			if (singleContribution)
				multipleContributions = true;
			else
				singleContribution = argChecks;
		}

		// NB the operator group carries the applied operator's identity; DataController::GetID()
		// is the Object identity of the key's head node, which is its LispObj class, not its token
		LispPtr ownCond;
		if (funcDC->m_OperatorGroup->GetNameID() == token::integrity_check)
		{
			auto tail = funcDC->GetLispRef().Right(); // (<expr> <cond>)
			if (tail.IsRealList() && tail.Right().IsRealList())
				ownCond = tail.Right().Left();
		}

		if (!singleContribution && ownCond.EndP())
			funcDC->m_ImpliedChecks = GetSharedEmptyCheckSet();
		else if (!multipleContributions && (ownCond.EndP() || (singleContribution && AreCheckAtomsImplied(singleContribution->thing, ownCond))))
			funcDC->m_ImpliedChecks = singleContribution; // nothing added: share the arg's set
		else
		{
			check_set_ptr combined = make_SharedThing<check_set>();
			argNr = 0;
			for (const DcRefListElem* argIter = funcDC->GetArgList(); argIter; argIter = argIter->m_Next.get(), ++argNr)
				if (DataController_ArgContributes(funcDC, argNr))
				{
					const check_set& argChecks = argIter->m_DC->m_ImpliedChecks->thing;
					combined->thing.insert(argChecks.begin(), argChecks.end());
				}
			if (!ownCond.EndP())
				InsertCheckAtoms(combined->thing, ownCond);
			funcDC->m_ImpliedChecks = std::move(combined);
		}
		stack.pop_back();
	}

	assert(m_ImpliedChecks);
	return m_ImpliedChecks;
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
		for (const DcRefListElem* argIter = m_Args.get(); argIter; argIter = argIter->m_Next.get(), ++argCount)
		{
			const DataController* argDC = argIter->m_DC.get();
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
	for (const DcRefListElem* argIter = m_Args.get(); argIter; ++currArg, argIter = argIter->m_Next.get()) 
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
			assert(!fd || CheckCalculatingOrReady(fd->GetOld()->GetCurrRangeItem().get()) || fd->WasFailed(FailType::Data) || fd->GetOld()->WasFailed(FailType::Data)
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
			Fail(argIter->m_DC.get(), doCalcData ? FailType::Data : FailType::MetaInfo);

		dms_assert(argItem || WasFailed(doCalcData)); // POSTCONDITION of argIter->m_DC->GetResult();

		if (WasFailed(doCalcData))
			return {};
		assert(argItem && !argItem->WasFailed(FailType::MetaInfo));
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
	DBG_TRACE(("FuncDC: {}", funcDC->md_sKeyExpr));

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

		// The result subtree is now complete and all the units it references are still alive (sub-items of the
		// cache root, or held by *args). Have the kind-1 holder take owning refs to them so the weak
		// m_DomainUnit/m_ValuesUnit of the cache result items do not expire once *args / the operator locals drop.
		if (resultHolder.IsNew())
			resultHolder.CaptureResultUnits();

#if defined(MG_DEBUG)
		// op-sig §9 drift defense #1: replay the resolved member's described signature
		// against the freshly determined units (report-only; the operator, args and
		// result are all concrete and alive here -- the one place §9 specifies). The
		// checker reports via the cancellation-safe path, but wrap it defensively so no
		// checker throw can EVER reach the result-failing catch below and fail a result.
		if (resultHolder.IsNew())
			try { SigUnitChecker_VerifyApplication(oper, GetItems(*args), resultHolder.GetNew()); }
			catch (...) {}
#endif
	}
	catch (...)
	{
		if (resultHolder.IsNew())
			resultHolder->CatchFail(FailType::MetaInfo); // also calls resultHolder->StopSupplInterest() (the resulting data).
		resultHolder.CatchFail(FailType::MetaInfo);
	}

	bool resultingFlag = !resultHolder.WasFailed(FailType::MetaInfo);
	MarkCacheItems(funcDC);

	if (auto resultItem = resultHolder.GetCurr())
	{
		if (!resultItem->GetDynamicObjClass()->IsDerivedFrom(funcDC->m_Operator->GetResultClass()))
		{
			auto msg = mySSPrintF("result of {} is of type {}, expected type: {}"
				, funcDC->m_OperatorGroup->GetName()
				, resultItem->GetCurrentObjClass()->GetName()
				, funcDC->m_Operator->GetResultClass()->GetName()
			);
			resultItem->Fail(msg, FailType::MetaInfo);
		}
		if (resultItem->WasFailed(FailType::MetaInfo))
		{
			resultHolder.Fail(resultItem.get(), FailType::MetaInfo);
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
	assert(m_Data.get()->IsCacheItem() || m_Data.get()->IsPassor() || m_OperatorGroup->CanResultToConfigItem() || IsTmp());

	return true;
}

// =========================================  CallCalcResult

// #795: name the config item that a progress or error message about a calculation belongs to.
// Only a result that a config item refers to knows its own name (TreeItemDualRef::HasBackRef);
// the intermediate results of the same calculation -- an operator's arguments, the inline
// sub-expressions of a calculation rule -- are anonymous cache items, and a progress line about
// one of them used to name no item at all. Argument calculations are started from within
// CallCalcResultImpl of the calculation that needs them, all on the meta thread, so the name
// travels down that nesting through this one thread-local, and is stored on each participating
// DataController before its operator is scheduled -- possibly to run on a worker thread later.
namespace {
	THREAD_LOCAL SharedTreeItem s_CurrOriginItem; // meta thread only

	struct OriginNameLock
	{
		OriginNameLock(const TreeItemDualRef& self)
			: m_Prev(s_CurrOriginItem)
		{
			auto named = self.GetBackRefItem();
			if (!named)
			{
				named = self.GetOriginItem(); // keep the name a running calculation started with
				if (!named && m_Prev)
				{
					named = m_Prev; // adopt the config item whose calculation asked for this result
					self.SetOriginItem(named);
				}
			}
			if (named)
				s_CurrOriginItem = std::move(named);
		}
		~OriginNameLock() { s_CurrOriginItem = std::move(m_Prev); }

	private:
		SharedTreeItem m_Prev;
	};
} // anonymous namespace

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

	OriginNameLock originName(*this); // #795, see above: covers GetArgs and ScheduleCalcResult

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
			assert(m_Data.get()->WasFailed(FailType::Data) || SuspendTrigger::DidSuspend());
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

		assert(!result || operContext || CheckDataReady(m_Data.get().get()) || (!IsNew() && CheckCalculatingOrReady(GetCacheRoot(m_Data.get().get()))));
	}
	catch (...)
	{
		CatchFail(FailType::Data);
		return;
	}
	if (!result)
	{
		if (auto curr = GetCurr(); curr && curr->WasFailed())
			Fail(curr.get());
		assert(SuspendTrigger::DidSuspend() || WasFailed(FailType::Data));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
		return;
	}

	assert(m_Data);
	assert(!SuspendTrigger::DidSuspend() && !WasFailed(FailType::MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
	assert(m_Data.get()->IsCacheItem() || m_Data.get()->IsPassor() || m_OperatorGroup->CanResultToConfigItem() || IsTmp());
}

// =========================================

// provide correct supplier for SymbDC
// or operators with CanResultToConfigItem (SubItem, Domain/ValuesUnit, Literal)
// normal configRef does NOT include all sub-items as template instantiation does
// in order not to create recursive supplier dependencies when item refers to anchestor
// this responsibility is moved to consumers in order to avoid recursion
// DC consumers are (limited to) FuncDC and implementors of AbstrCalculator.

// #1167: a SubItem call visits the WHOLE result sub-tree of its arg 0 (see VisitSuppliers below),
// which is what keeps the members of a composite result together: unique()'s Values next to its
// nr_OrgEntity, subset()'s org_rel next to nr_OrgEntity, Dijkstra's OD members next to each other.
// Those members come out of ONE calculation, so naming one of them has to keep all of them alive.
//
// A PhaseContainer result is not that kind of composite: its members mirror source items that are
// calculated independently of each other. Supplying its whole tree makes ONE phase/x reference take
// interest on EVERY member, and PreCalcUpdate -- which collects exactly the members that carry
// interest -- then materialises the entire fenced container. Measured before this split: a fence
// over {A, B, C} with only A consumed calculated all three, where the same source container WITHOUT
// the fence calculated only A. A fence meant to bound the working set was widening it, and an
// unreferenced member read from storage (no calculation rule to collect) reported an error on every
// run.
//
// The split is between VALUES and SHAPE, not between members and no members. The data items of a
// phase result are independently calculable and must not be supplied wholesale; its units and
// containers carry the shape of the result -- a consumer naming phase/u as its domain resolves
// through the mirror unit -- and dropping those breaks the consumer with "is neither calculating
// nor ready nor failed; no read lock can be set on it" (measured on a fenced unit).
static bool DataController_SuppliesWholeResultTree(const DataController* argDC)
{
	auto argFuncDC = dynamic_cast<const FuncDC*>(argDC);
	if (!argFuncDC)
		return true;
	return argFuncDC->m_OperatorGroup->GetNameID() != token::PhaseContainer;
}

// Supply the shape of a phase result without supplying its values: forward every member except the
// data items, whose calculation is what the fence is supposed to schedule on demand.
static ActorVisitState DataController_VisitResultShape(const TreeItem* dcResult, const ActorVisitor& visitor)
{
	return dcResult->VisitConstVisibleSubTree(
		MakeDerivedBoolVisitor([&visitor](const Actor* suppl) -> bool
			{
				auto memberItem = dynamic_cast<const TreeItem*>(suppl);
				if (memberItem && IsDataItem(memberItem))
					return true; // an independently calculable value: leave it to whoever names it
				return visitor(suppl) != AVS_SuspendedOrFailed;
			}
		)
	);
}

ActorVisitState FuncDC::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	DcRefListElem* dcRefElem = m_Args.get(); // points to currently uniquely owned DcRefListElem
	SharedStr firstArgValue;  // may be filled with first arg value that encoded the role of consecutive arguments for OperatorGroups with Dyanmic Arguments
	for (arg_index argNr = 0; dcRefElem; dcRefElem = dcRefElem->m_Next.get(), ++argNr)
	{
		auto firstArgValueCPtr = firstArgValue.cbegin();
		assert(m_OperatorGroup);
		if (!Test(svf, SupplierVisitFlag::ReadyDcsToo)
			&& !MustCalcArg(argNr, true, firstArgValueCPtr)
			&& !m_OperatorGroup->MustSupplyTree(argNr, firstArgValueCPtr))
			continue;

		const DataController* dc = dcRefElem->m_DC.get(); // borrow shared owned dc;

		if (visitor(dc) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

		auto dcResult = dc->MakeResult();
		if (!dcResult)
			continue;

		if (visitor(dcResult.get()) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

		if (m_OperatorGroup->MustSupplyTree(argNr, firstArgValueCPtr) ||
			(Test(svf, SupplierVisitFlag::ScanSupplTree) && m_OperatorGroup->IsSubItemRoot(argNr, firstArgValueCPtr)))
		{
			// an arg declared subst_with_subitems (DiscrAlloc's claims, Overlay) asks for the tree as
			// such and gets it whole, phase result or not; only the SubItem route splits shape from values
			auto visitResult = (m_OperatorGroup->MustSupplyTree(argNr, firstArgValueCPtr) || DataController_SuppliesWholeResultTree(dc))
				? dcResult->VisitConstVisibleSubTree(visitor)
				: DataController_VisitResultShape(dcResult.get(), visitor);
			if (visitResult == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
		if (argNr == 0 && m_OperatorGroup->HasDynamicArgPolicies())
			firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(dc->CalcCertainResult()->GetOld())))->GetIndexedValue(0);
	}

	for (auto& s : m_OtherSuppliers)
		if (visitor.Visit(s.get()) == AVS_SuspendedOrFailed)
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
			).get()
		);
	}

	dms_assert(m_Data);

	return m_Data.get();
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
			).get()
		);

	assert(m_Data);
	return m_Data.get();
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
				).get()
		);

	assert(m_Data);
	return m_Data.get();
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
		auto sourceItem = curr->GetConfigRoot()->ResolveItemPath(fullName);
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
		if (sourceItem)
			const_cast<SymbDC*>(this)->SetOld(sourceItem.get()); // copy will be done by UpdateMetaInfo
	}
	dms_assert( !SuspendTrigger::DidSuspend() ); // Follows from previous assert and FindItem doesn't call MustSuspend();

	if (!m_Data)
	{
		auto msg = mySSPrintF("Cannot find Item {}", m_FullNameID);
		Fail(msg, FailType::MetaInfo);
		return {};
	}

	dms_assert( m_Data );

	dms_assert(!IsTmp());

	dms_assert( !SuspendTrigger::DidSuspend() );
	auto result = GetCurr(); // owning snapshot; null if the config item expired since SetOld
	if (!result)
	{
		auto msg = mySSPrintF("Item {} no longer exists", m_FullNameID);
		Fail(msg, FailType::MetaInfo);
		return {};
	}
	return result;
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
	auto curr = GetCurr(); // owning snapshot; null if the config item expired
	if (!curr)
		return nullptr;
	//		if (m_Data->m_State.GetTransState() < actor_flag_set::AF_Validating)
	//			m_Data->SuspendibleUpdate();
	bool suspended = !curr->PrepareDataUsage(DrlType::Suspendible);

	if (curr->WasFailed())
		Fail(curr.get());

	if (suspended)
	{
		dms_assert(SuspendTrigger::DidSuspend() || curr->WasFailed());
		return nullptr;
	}

	if (!CheckCalculatingOrReady(curr->GetCurrRangeItem().get()))
	{
		// #1152: PrepareDataUsage returned true without making curr calculating-or-ready and without
		// failing it; enforce the postcondition here so consumers (GetArgs) see a failed DC instead
		// of running an operator on an argument without data. Fail only this DC, not the referent.
		if (!WasFailed(FailType::Data))
			Fail(mySSPrintF("no data available for {}", curr->GetFullName()), FailType::Data);
		return nullptr;
	}

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
	if (!data || visitor(data.get()) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;

	return AVS_Ready;
}

