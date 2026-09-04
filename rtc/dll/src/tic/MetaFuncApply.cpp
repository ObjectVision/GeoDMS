// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Applying a calculation rule that is not a plain expression: instantiating a template
// or a function body under a holder, materializing a structured function result, the
// built-in map(), and running an operator group as a meta function.

#include "AbstrCalculator.h"
#include "TreeItemFunctionSpec.h"

#include "RtcInterface.h"
#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "ptr/LifetimeProtector.h"
#include "ser/AsString.h"
#include "set/StackUtil.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"
#include "xct/ErrMsg.h"
#include "xml/XMLOut.h"

#include "LispList.h"
#include "LispTreeType.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataController.h"
#include "DataItemClass.h"
#include "DC_Ptr.h"
#include "ExprRewrite.h"
#include "LispRef.h"
#include "OperGroups.h"
#include "Operator.h"
#include "OperSignature.h"
#include "SessionData.h"
#include "SupplCache.h"
#include "UnitClass.h"

#include "LispContextHandle.h"
#include "TreeItemContextHandle.h"
#include "TreeItemClass.h"
#include "MoreDataControllers.h"
#include "DataArrayValue.h"

#include <algorithm>
#include <bitset>
#include <functional>
#include <tuple>
#include <map>
#include <memory>
#include <set>

#include "HofTypeUnifier.h"

using namespace hof;

//----------------------------------------------------------------------
// MetaFuncCurry
//----------------------------------------------------------------------


void InstantiateMap(TreeItem* holder, const AbstrCalculator* ac, LispPtr mapExpr); // fwd (defined below, uses FunctionApplication)
namespace {
	void InstantiateStructuredFunctionResult(TreeItem* target, LispPtr rootKey, const StructuredFunctionResult& result); // fwd (defined below)
}

void MetaFuncCurry::operator ()(TreeItem* target, const AbstrCalculator* ac) const
{
	if (isMapCall)
		InstantiateMap(target, ac, fullLispExpr);
	else if (structuredResult)
		InstantiateStructuredFunctionResult(target, fullLispExpr, *structuredResult);
	else if (applyItem)
	{
		// K11a-3 on the INSTANTIATE path: validate structured/by-example parameter
		// contracts at the boundary before the body is copied (functions only).
		// Arguments resolve from the TARGET's parent -- the copy's binding context.
		CheckStructuredParamContracts(applyItem, fullLispExpr.Right(), target);
		InstantiateTemplate(target, applyItem, fullLispExpr.Right());
	}
	else if (og)
		ApplyAsMetaFunction(target, ac, og, fullLispExpr.Right());
}

LispRef MetaFuncCurry::GetAsLispRef() const
{
	return fullLispExpr;
}

LispRef GetAsLispRef(const MetaInfo& metaInfo)
{
	if (metaInfo.index() == 1)
		return std::get<LispRef>(metaInfo);
	else if (metaInfo.index() == 2)
		return GetAsLispRef(std::get<SharedTreeItem>(metaInfo)->GetCurrMetaInfo(metainfo_policy_flags::get_as_lispref));
	else
	{
		dms_assert(metaInfo.index() == 0);
		return std::get<MetaFuncCurry>(metaInfo).GetAsLispRef();
	}
}

void InstantiateTemplate(TreeItem* holder, const TreeItem* applyItem, LispPtr templCallArgList)
{
	// only config items can become template instantiations
	dms_assert(holder);

	if (holder->WasFailed(FailType::MetaInfo))
		return;
	if (holder->GetIsInstantiated())
		return;

	auto ctc = CopyTreeContext(holder, applyItem, ""
	,	applyItem->IsTemplate()
		? DataCopyMode(DataCopyMode::NoRoot | DataCopyMode::CopyExpr | DataCopyMode::MakeEndogenous)
		: DataCopyMode(DataCopyMode::NoRoot | DataCopyMode::CopyExpr)
	,	templCallArgList
	);
	ctc.Apply();

	holder->SetIsInstantiated();
}

namespace {
	void InstantiateStructuredFunctionResultMembers(TreeItem* parent
		, const std::vector<StructuredFunctionResultMember>& members
		, const AbstrUnit* reducedRootDomain, AbstrUnit* targetRootDomain)
	{
		for (const auto& member : members)
		{
			SharedMutableTreeItem child;
			if (member.key.EndP())
				child = parent->CreateItem(member.id);
			else
			{
				auto dc = GetOrCreateDataController(member.key);
				auto reducedItem = dc->MakeResult();
				if (!reducedItem)
				{
					dms_assert(dc->WasFailed(FailType::MetaInfo));
					parent->ThrowFail(dc.get());
				}

				if (IsDataItem(reducedItem.get()))
				{
					auto reducedData = AsDataItem(reducedItem.get());
					const AbstrUnit* memberDomain = reducedData->GetAbstrDomainUnit();
					if (reducedRootDomain && targetRootDomain && reducedRootDomain->UnifyDomain(memberDomain))
						memberDomain = targetRootDomain;
					child = CreateDataItem(parent, member.id, memberDomain
						, reducedData->GetAbstrValuesUnit(), reducedData->GetValueComposition());
				}
				else if (IsUnit(reducedItem.get()))
					child = AsUnit(reducedItem.get())->GetUnitClass()->CreateUnit(parent, member.id);
				else
					child = parent->CreateItem(member.id);

				child->SetCalculator(AbstrCalculator::ConstructFromLispRef(child.get(), member.key, CalcRole::Calculator));
			}

			InstantiateStructuredFunctionResultMembers(child.get(), member.subItems, reducedRootDomain, targetRootDomain);
			if (member.key.EndP())
				child->SetIsInstantiated();
		}
	}

	void InstantiateStructuredFunctionResult(TreeItem* target, LispPtr rootKey, const StructuredFunctionResult& result)
	{
		dms_assert(target);
		if (target->WasFailed(FailType::MetaInfo) || target->GetIsInstantiated())
			return;

		auto rootDC = GetOrCreateDataController(rootKey);
		auto reducedRoot = rootDC->MakeResult();
		if (!reducedRoot)
		{
			dms_assert(rootDC->WasFailed(FailType::MetaInfo));
			target->ThrowFail(rootDC.get());
		}
		target->SetDC(rootDC, reducedRoot.get());

		const AbstrUnit* reducedRootDomain = IsUnit(reducedRoot.get()) ? AsUnit(reducedRoot.get()) : nullptr;
		AbstrUnit* targetRootDomain = IsUnit(target) ? AsDynamicUnit(target) : nullptr;
		InstantiateStructuredFunctionResultMembers(target, result.subItems
			, reducedRootDomain, targetRootDomain);
		target->SetIsInstantiated();
	}
}

// WP3.3: map(function, container) -- populate `holder` with one child per data-item /
// unit child of the source container, each computed as function(child). The mapped
// function must take exactly one parameter (the element); its result type follows from
// the reduction. A first-order for_each replacement.
void InstantiateMap(TreeItem* holder, const AbstrCalculator* ac, LispPtr mapExpr)
{
	dms_assert(holder);
	if (holder->WasFailed(FailType::MetaInfo))
		return;
	if (holder->GetIsInstantiated())
		return;

	LispPtr args = mapExpr.Right();
	if (args.EndP() || args.Right().EndP() || !args.Right().Right().EndP())
		holder->throwItemError("map expects exactly two arguments: map(function, container)");
	LispPtr fExpr = args.Left();
	LispPtr srcExpr = args.Right().Left();
	if (!srcExpr.IsSymb())
		holder->throwItemError("map: the second argument must be the name of a container");

	// The mapped function is either a plain function name F (applied as F(child)) or a PARTIAL
	// APPLICATION of one leaving exactly one '_' hole for the mapped element, e.g.
	// map(Scale(k, _), src) -> Scale(k, child). Partial-application fixed arguments must be item
	// references or literals here (a nested sub-expression argument is not supported).
	const TokenID t_Hole = GetTokenID_st("_");
	LispPtr fHead = fExpr;
	std::vector<CallArg> fixedArgs; // resolved fixed args, in order; the hole slot is a placeholder
	SizeT holePos = SizeT(-1);
	if (fExpr.IsSymb())
	{
		fixedArgs.emplace_back().isHole = true; // one implicit element slot
		holePos = 0;
	}
	else if (!fExpr.EndP() && fExpr.Left().IsSymb())
	{
		fHead = fExpr.Left();
		SizeT i = 0;
		for (LispPtr a = fExpr.Right(); !a.EndP(); a = a.Right(), ++i)
		{
			LispPtr ae = a.Left();
			if (ae.IsSymb() && ae.GetSymbID() == t_Hole)
			{
				if (holePos != SizeT(-1))
					holder->throwItemError("map: a partial-application mapped function must leave exactly one '_' hole");
				holePos = i;
				fixedArgs.emplace_back().isHole = true;
			}
			else if (ae.IsSymb())
			{
				auto ai = ac->FindItem(ae.GetSymbID());
				if (!ai)
					holder->throwItemErrorF("map: partial-application argument '{}' not found", ae.GetSymbID());
				CallArg fa; fa.key = ai->GetCheckedKeyExpr(); fa.item = ai; fixedArgs.push_back(fa);
			}
			else
			{
				CallArg fa; fa.key = LispRef(ae); fixedArgs.push_back(fa); // a literal argument
			}
		}
		if (holePos == SizeT(-1))
			holder->throwItemError("map: a partial-application mapped function must leave one '_' hole for the element, e.g. map(F(k, _), src)");
	}
	else
		holder->throwItemError("map: the first argument must be a function or a partial application of one, e.g. map(F, src) or map(F(k, _), src)");

	auto funcItem = ac->FindItem(fHead.GetSymbID());
	if (!funcItem || !funcItem->IsFunctionItem())
		holder->throwItemErrorF("map: '{}' is not a function", fHead.GetSymbID());
	if (SizeT(TreeItem_GetFunctionParamCount(funcItem.get())) != fixedArgs.size())
		holder->throwItemErrorF("map: function '{}' takes {} parameter(s), but the (partial) application supplies {} including the '_' element"
			, funcItem->GetFullName().c_str(), UInt32(TreeItem_GetFunctionParamCount(funcItem.get())), UInt32(fixedArgs.size()));

	auto srcItem = ac->FindItem(srcExpr.GetSymbID());
	if (!srcItem)
		holder->throwItemErrorF("map: source '{}' not found", srcExpr.GetSymbID());
	srcItem->UpdateMetaInfo();

	SharedTreeItem errorHolder = make_shared_tree(holder, existing_obj{});
	for (const TreeItem* c = srcItem->_GetFirstSubItem(); c; c = c->GetNextItem())
	{
		if (!IsDataItem(c) && !IsUnit(c))
			continue;
		FunctionApplication appl;
		appl.m_FuncItem = funcItem.get();
		appl.m_ErrorHolder = errorHolder;
		for (SizeT i = 0; i != fixedArgs.size(); ++i)
		{
			if (i == holePos)
			{
				CallArg a; a.key = c->GetCheckedKeyExpr(); a.item = make_shared_tree(c, existing_obj{});
				appl.PushArg(a);
			}
			else
				appl.PushArg(fixedArgs[i]);
		}
		LispRef key = appl.Reduce();

		// Materialize the result's meta so each mapped child is a FIRST-CLASS typed item (a
		// DataItem / Unit of the derived type), not a plain TreeItem follower. The latter works as
		// a value (sum(out/a)) but is NOT directly assignable to a declared attribute
		// (out_a := out/a fails: 'DataItem<..> incompatible with a result of type TreeItem').
		auto dc = GetOrCreateDataController(key);
		auto res = dc->MakeResult();
		if (!res)
		{
			dms_assert(dc->WasFailed(FailType::MetaInfo));
			errorHolder->ThrowFail(dc.get());
		}
		SharedMutableTreeItem child;
		if (IsDataItem(res.get()))
		{
			auto rdi = AsDataItem(res.get());
			child = CreateDataItem(holder, c->GetNameID(), rdi->GetAbstrDomainUnit(), rdi->GetAbstrValuesUnit(), rdi->GetValueComposition());
		}
		else if (IsUnit(res.get()))
			child = AsUnit(res.get())->GetUnitClass()->CreateUnit(holder, c->GetNameID());
		else
			child = holder->CreateItem(c->GetNameID()); // a container/other result: keep the plain follower

		child->SetCalculator(AbstrCalculator::ConstructFromLispRef(child.get(), key, CalcRole::Calculator));
	}
	holder->SetIsInstantiated();
}

OArgRefs ApplyMetaFunc_GetArgs(TreeItem* holder, const AbstrCalculator* ac, const AbstrOperGroup* og, LispPtr metaCallArgs)
{
	SubstitutionBuffer substBuff;

	arg_index currArg = 0;
	ArgRefs argSeq; argSeq.reserve(4);
	SharedStr firstArgValue;
	for (auto cursor = metaCallArgs; !cursor.EndP(); cursor = cursor.Right(), ++currArg)
	{
		auto oap = og->GetArgPolicy(currArg, firstArgValue.begin());
		bool mustCalcArg = FuncDC::MustCalcArg(oap, false);

		ArgRef argRef;
		if (!mustCalcArg) 
		{ // DomainContainer and ValuesContainer and SubsetContainer
			LispRef argExpr = cursor.Left();
			if (oap == oper_arg_policy::calc_as_result)
			{
				// Skipped rather than pushed into argSeq: the operator reads this
				// argument from metaCallArgs itself. Only a TRAILING argument may be
				// skipped, so argSeq stays index-aligned with the leading arguments --
				// that is the invariant. WHICH position it sits at is the group's own
				// business: select_with_attr_xxx puts the condition second, select_spec
				// puts it after its leading spec argument.
				assert(cursor.Right().EndP());
				continue;
			}
			if (oap == oper_arg_policy::calc_at_subitem)
			{
				assert(cursor.Right().EndP()); // no next args, argSeq must remain consistent with the first args..
				continue;
			}


			if (!argExpr.IsSymb())
			{
				auto errMsgTxt = mySSPrintF(
					"meta-function {} expects an item reference as argument {}, but an expression was given.\n"
					"Consider defining and using a separate item as {}"
					, og->GetNameID()
					, currArg + 1
					, AsFLispSharedStr(argExpr, FormattingFlags::ThousandSeparator)
				);
				holder->Fail(errMsgTxt, FailType::MetaInfo);
				return {};
			}
			TokenID symbID = cursor.Left().GetSymbID();
			if (auto vc = ValueClass::FindByScriptName(symbID))
				argRef.emplace<SharedTreeItem>(make_shared_tree(UnitClass::Find(vc)->CreateDefault(), existing_obj{})); // unitName -> [UnitName []] ofwel unitName().
			else
			{
				auto foundItem = ac->FindItem(symbID);
				if (!foundItem)
				{
					auto msg = SharedStr(symbID.AsStrRangeLock());
					holder->Fail(mySSPrintF("Cannot find {}", msg), FailType::MetaInfo);
				}
				else
					argRef.emplace<SharedTreeItem>(foundItem);
			}
			dms_assert((argRef.index() == 1 && (std::get<1>(argRef) != nullptr)) || holder->WasFailed(FailType::MetaInfo));
			dms_assert(!SuspendTrigger::DidSuspend()); // POSTCONDITION of argIter->m_DC->MakeResult();
		}
		else
		{
			assert(mustCalcArg);
			auto substResult = ac->SubstituteExpr(substBuff, cursor.Left());
			if (substResult.index() == 0)
				throwDmsErrF("in ApplyMetaFunc_GetArgs the sub expression '{}' is a MetaFunc('{}') and cannot be substituted"
					, AsString(cursor.Left().AsLispPtr())
					, AsString(std::get<0>(substResult).GetAsLispRef().AsLispPtr())
				);
			if (substResult.index() == 2)
				throwDmsErrF("in ApplyMetaFunc_GetArgs the sub expression {} is a SourceItem reference that cannot be substituted", AsString(cursor.Left()));
			MG_CHECK(substResult.index() == 1);

			FutureData dc = GetOrCreateDataController(std::get<1>(substResult)); // what about non-substitited stuff?
			dms_assert(dc);
			FutureData fd = dc->CalcResultWithValuesUnits();
			dms_assert(!fd || fd->GetInterestCount());
			dms_assert(!SuspendTrigger::DidSuspend());
			dms_assert(!fd || CheckCalculatingOrReady(fd->GetOld()->GetCurrRangeItem().get()) || fd->WasFailed(FailType::Data));
			dms_assert(fd || dc->WasFailed(FailType::Data));
			if (dc->WasFailed(FailType::Data))
				holder->Fail(dc.get_ptr(), FailType::MetaInfo);
			argRef.emplace<FutureData>(std::move(fd));
			if (currArg == 0 && og->HasDynamicArgPolicies())
				firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(dc->GetOld())))->GetIndexedValue(0);
		}
		auto argItem = GetItem(argRef);
		if (argItem) {
			argItem->UpdateMetaInfo();
			assert(argItem->m_State.GetProgress() >= ProgressState::MetaInfo || argItem->WasFailed(FailType::MetaInfo));
			if (argItem->WasFailed(mustCalcArg))
				holder->Fail(argItem, FailType::MetaInfo);
		}

		dms_assert(argItem || holder->WasFailed(FailType::MetaInfo)); // POSTCONDITION of argIter->m_DC->GetResult();

		if (holder->WasFailed(FailType::MetaInfo))
			return {};
		dms_assert(argItem && !argItem->WasFailed(FailType::MetaInfo));
		argSeq.push_back(argRef);
	}
	return argSeq;
}


bool ApplyMetaFunc_impl(TreeItem* holder, const AbstrCalculator* ac, const AbstrOperGroup* og, LispPtr metaCallArgs)
{
	dms_assert(ac);
	assert(holder);
	if (holder->WasFailed(FailType::MetaInfo))
		return false;
	bool result;
	try {
		auto args = ApplyMetaFunc_GetArgs(holder, ac, og, metaCallArgs);
		dms_assert(!SuspendTrigger::DidSuspend());
		if (!args)
		{
			dms_assert(holder->WasFailed(FailType::MetaInfo));
			return false;
		}

		auto oper = og->FindOperByArgs(*args);
		MG_CHECK(oper);
		dms_assert(!SuspendTrigger::DidSuspend());

		LifetimeProtector<TreeItemDualRef> resultHolder; resultHolder->SetTmp(holder);

		oper->CreateResultCaller(*resultHolder, *args, metaCallArgs);

		bool resultingFlag = !resultHolder->WasFailed(FailType::MetaInfo);

		if (!holder->GetDynamicObjClass()->IsDerivedFrom(oper->GetResultClass()))
		{
			auto msg = mySSPrintF("result of {} is of type {}, expected type: {}"
				, og->GetNameID()
				, resultHolder->GetCurrentObjClass()->GetNameID()
				, oper->GetResultClass()->GetNameID()
			);
			holder->Fail(msg, FailType::MetaInfo);
		}
		if (resultHolder->WasFailed(FailType::MetaInfo))
		{
			holder->Fail(resultHolder->GetOld(), FailType::MetaInfo);
			resultingFlag = false;
		}

		dms_assert(resultingFlag != (SuspendTrigger::DidSuspend() || resultHolder->WasFailed()));
		return resultingFlag;
	}
	catch (...)
	{
		holder->CatchFail(FailType::MetaInfo);
		return false;
	}
	if (!result)
	{
		dms_assert(SuspendTrigger::DidSuspend() || holder->WasFailed(FailType::MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
		return false;
	}

	dms_assert(!SuspendTrigger::DidSuspend() && !holder->WasFailed(FailType::MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
	return true;
}


// applied on direct arguments: for_each, loop
void ApplyAsMetaFunction(TreeItem* holder, const AbstrCalculator* ac, const AbstrOperGroup* og, LispPtr metaCallArgs)
{
//	let op: (oap & oap_is_templ) in argumenten van oper_group

	dms_assert(IsMetaThread());
	dms_check_not_debugonly;
	dms_assert(holder);
	dms_assert(ac);
	dms_assert(!SuspendTrigger::DidSuspend());
	if (holder->WasFailed(FailType::MetaInfo))
		return;
	if (holder->GetIsInstantiated())
		return;

	LispContextHandle operContext("ApplyMetaFunc_impl", LispRef(LispRef(og->GetNameID()), metaCallArgs));

#if defined(MG_DEBUG_DCDATA)
	DBG_START("ApplyMetaFunc_impl", "", false);
	DBG_TRACE(("metaCallExpr={}", AsFLispSharedStr(metaCallArgs, FormattingFlags::ThousandSeparator)));
#endif

	StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
	InterestRetainContextBase base;

	SuspendTrigger::FencedBlocker lockSuspend("ApplyAsMetaFunction");

	bool resultFlag = ApplyMetaFunc_impl(holder, ac, og, metaCallArgs);

	dms_assert(resultFlag || holder->WasFailed(FailType::MetaInfo));

	dms_assert(holder->GetIsInstantiated() || holder->WasFailed(FailType::MetaInfo));
	holder->SetIsInstantiated(); // REMOVE if the above assert is PROVEN.
}
