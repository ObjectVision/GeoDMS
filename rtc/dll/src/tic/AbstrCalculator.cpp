// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// AbstrCalculator: binds calculation rules (LispRef expressions) to tree
// items, resolves references, and applies meta-functions and templates.

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

// *****************************************************************************
// Section:    to be located into following code
// *****************************************************************************

metainfo_policy_flags arg2metainfo_polcy(oper_arg_policy oap)
{
	switch (oap)
	{
	case oper_arg_policy::calc_never:
		return metainfo_policy_flags::subst_never;
	case oper_arg_policy::calc_always:
	case oper_arg_policy::calc_as_result:
		return metainfo_policy_flags::subst_allowed;
	// case oper_arg_policy::calc_subitem_root:
	// case oper_arg_policy::is_templ:
	// case oper_arg_policy::subst_with_subitems:
	default:
		return metainfo_policy_flags::suppl_tree | metainfo_policy_flags::subst_never;
	}
}

SharedStr AbstrCalculator::GetExpr() const
{
	return GetAsFLispExprOrg(FormattingFlags::ThousandSeparator); 
}

void AbstrCalculator::WriteHtmlExpr(OutStreamBase& stream) const 
{ 
	stream.WriteValue(GetExpr().c_str());
}

//----------------------------------------------------------------------
// Definition of public helper funcs
//----------------------------------------------------------------------

SharedStr MakeUnknownIdentifierErrorMsg(SharedStr supplRefStr, BestItemRef bestItemRef)
{
	auto errMsg = mySSPrintF("Unknown identifier '{}'", supplRefStr);
	if (bestItemRef.first)
	{
		auto supplRefStrSize = supplRefStr.AsRange().size();
		auto notFoundPartSize = bestItemRef.second.AsRange().size();
		if (notFoundPartSize < supplRefStrSize)
		{
			auto bestFullName = bestItemRef.first->GetFullName();
			if (!bestFullName.empty())
				errMsg += mySSPrintF("\nDid you mean '{}' that refers to [[{}]]?\nThe '{}' part was not found there."
					, CharPtrRange(supplRefStr.begin(), supplRefStrSize - notFoundPartSize)
					, bestFullName.c_str()
					, bestItemRef.second
				);
		}
	}
	return errMsg;
}

LispRef GetLispRefForTreeItem(const TreeItem* sourceObject, const CopyTreeContext& copyContext)
{
	if (!sourceObject->IsCacheItem() || sourceObject->HasCalculator())
		return sourceObject->GetCheckedKeyExpr();

	dms_assert(! copyContext.m_SrcRoot->GetTreeParent()); // DEBUG, set all refs to Cache SubItems as direct paths.

	MG_CHECK(copyContext.m_DstRoot);
	MG_CHECK(copyContext.m_DstRoot->mc_DC);
	//	auto dstRootItem = debug_valcast<TreeItem*>(copyContext.m_DstRoot);
//	auto keyExpr = dstRootItem->GetBaseKeyExpr();
	auto keyExpr = copyContext.m_DstRoot->mc_DC->GetLispRef();
	assert(!keyExpr.EndP());

	return slSubItemCall(keyExpr, sourceObject->GetRelativeName(copyContext.m_SrcRoot).AsRange());
}

using item_ref_type = std::variant<SharedTreeItem, LispRef>;

SharedTreeItem FindSubItem(const TreeItem* sourceItem, SharedStr relPath)
{
	auto end = relPath.send();
	auto begin = relPath.begin();
	while (true)
	{
		dms_assert(!sourceItem->IsCacheItem());
		if (begin == end)
			return make_shared_tree(sourceItem, existing_obj{});
		auto delimPos = begin;
		while (delimPos != end && *delimPos != DELIMITER_CHAR)
			++delimPos;
		auto subItem = sourceItem->GetConstSubTreeItemByID(GetTokenID_mt(begin, delimPos));
		if (!subItem)
			throwErrorF("FindSubItem", "Cannot find {} from {}", SharedStr(CharPtrRange(begin, delimPos)), sourceItem->GetFullName().c_str());
		MG_CHECK(!subItem->IsCacheItem());
		sourceItem = subItem.get();
		begin = delimPos;
		if (begin != end)
		{
			dms_assert(*begin == DELIMITER_CHAR);
			++begin;
		}
	}
}

AbstrCalculatorRef CreateCalculatorForTreeItem(TreeItem* context, const TreeItem* sourceObject, const CopyTreeContext& copyContext)
{
	dms_assert(sourceObject);
	return AbstrCalculator::ConstructFromLispRef(context, GetLispRefForTreeItem(sourceObject, copyContext), CalcRole::Calculator);
}

//----------------------------------------------------------------------
// now independent Calculator related function; TODO G8: dismantle Calculators
//----------------------------------------------------------------------

TIC_CALL auto GetDC(const AbstrCalculator* calculator)->DataControllerRef
{
	auto metaInfo = calculator->GetMetaInfo();
	if (metaInfo.index() == 0)
		throwErrorF("GetDC", "KeyExpr expected but called with: {}\nMeta functions and template instantiations are not supported here."
			, AsFLispSharedStr(std::get<0>(metaInfo).GetAsLispRef(), FormattingFlags::ThousandSeparator).c_str()
		);
	if (metaInfo.index() == 2) // follow source
		return GetOrCreateDataController(std::get<2>(metaInfo)->GetCheckedKeyExpr());
	return GetOrCreateDataController(std::get<1>(metaInfo));
}

auto MakeResult(const AbstrCalculator* calculator)->make_result_t
{
	auto dc = GetDC(calculator);
	dc->MakeResult();
	//	auto resFuture = dc->CallCalcResult();

	return dc; // return owner of potential future.
}

auto CalledCalcHandle(const AbstrCalculator* calculator, const Class* cls)->calc_result_t
{
	if (SuspendTrigger::DidSuspend())
		return {};

	auto dc = MakeResult(calculator);
	assert(dc);
	assert(dc->GetOld() || dc->WasFailed(FailType::MetaInfo));
	if (dc->WasFailed(FailType::MetaInfo))
		return dc;
	CheckResultingTreeItem(dc->GetOld(), cls);
	auto result = dc->CallCalcResult(nullptr);

	if (SuspendTrigger::DidSuspend())
		return {};

	if (!result)
	{
		assert(dc->WasFailed(FailType::Data));
		dc->ThrowFail();
	}
	return result;
}

void ExplainResult(const AbstrCalculator* calculator, std::shared_ptr<Explain::Context> context)
{
	assert(context);
	auto dc = MakeResult(calculator);
	dms_assert(dc);
	dms_assert(dc->GetOld() || dc->WasFailed(FailType::MetaInfo));
	if (dc->WasFailed(FailType::MetaInfo))
		return;

	auto funcDC = dynamic_cast<const FuncDC*>(dc.get());
	if (!funcDC)
		return;
	if (!funcDC->m_OperatorGroup->CanExplainValue())
		return;

	auto result = dc->CallCalcResult(context);
}

void CheckResultingTreeItem(const TreeItem* refItem, const Class* desiredResultingClass)
{
	dms_assert(refItem);

	if (desiredResultingClass && !refItem->GetDynamicObjClass()->IsDerivedFrom(desiredResultingClass))
	{
		throwErrorF("CheckResult", "calculation will result in a {}, which is not castable to the type {} of the result item",
			refItem->GetDynamicObjClass()->GetName().c_str(),
			desiredResultingClass->GetName().c_str()
		);
	}
}

//----------------------------------------------------------------------
// MetaFuncCurry
//----------------------------------------------------------------------


void InstantiateMap(TreeItem* holder, const AbstrCalculator* ac, LispPtr mapExpr); // fwd (defined below, uses FunctionApplication)
namespace { // merges with the checker machinery's anonymous namespace below
	void CheckStructuredParamContracts(const TreeItem* applyItem, LispPtr argList, const TreeItem* target); // fwd (K11a-3 boundary check, instantiate path)
}

void MetaFuncCurry::operator ()(TreeItem* target, const AbstrCalculator* ac) const
{
	if (isMapCall)
		InstantiateMap(target, ac, fullLispExpr);
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

//----------------------------------------------------------------------
// Implementation Class AbstrParseResultCtorFunc
//----------------------------------------------------------------------

AbstrCalculator::AbstrCalculator(const TreeItem* context, CalcRole cr)
	:	m_Holder(make_weak_tree(context))
	,	m_CalcRole(cr)
{
	if (context)
		m_SearchContext = GetSearchContext(context, cr);

	if (!m_SearchContext)
		m_SearchContext = SessionData::Curr()->GetConfigRoot();
	assert(m_SearchContext);
}

AbstrCalculator::AbstrCalculator(const TreeItem* context, LispPtr lispRefOrg, CalcRole cr)
	: m_Holder(make_weak_tree(context))
	, m_LispExprOrg(lispRefOrg)
	, m_CalcRole(cr)
	, m_HasParsed(true)
{
	if (context)
		m_SearchContext = GetSearchContext(context, cr);

	if (!m_SearchContext)
		m_SearchContext = SessionData::Curr()->GetConfigRoot();
	if (cr == CalcRole::Calculator)
	{
		m_LispExprSubst = m_LispExprOrg;
		m_HasSubstituted = true; // already done by caller ?
	}
	assert(m_SearchContext);
}


AbstrCalculator::~AbstrCalculator()
{}

bool AbstrCalculator::CheckSyntax () const
{
	return true;
}

SharedTreeItem AbstrCalculator::SearchContext() const
{
	auto searchContext = m_SearchContext;
	MG_CHECK(searchContext);
	return searchContext;
}

static StaticTokenID thisToken("this");

auto AbstrCalculator::FindItem(TokenID itemRef) const -> SharedTreeItem
{
	assert(!itemRef.empty());
	assert(!m_Holder.expired());

	MG_SIGNAL_ON_UPDATEMETAINFO

	if (itemRef == thisToken)
		return m_Holder.lock();

	SharedStr itemRefStr(itemRef.AsStrRange());
	return SearchContext()->FindItem(itemRefStr);
}

auto AbstrCalculator::FindOrVisitItem(SubstitutionBuffer& buff, TokenID itemRef) const -> SharedTreeItem
{
	if (buff.optionalVisitor)
	{
		auto x = VisitSourceItem(itemRef, buff.svf, *buff.optionalVisitor);
		if (!x)
		{
			buff.avs = AVS_SuspendedOrFailed;
			return {};
		}
		return x.value();
	}
	return FindItem(itemRef);
}

BestItemRef AbstrCalculator::FindBestItem(TokenID itemRef) const
{
	assert(!itemRef.empty());
	assert(!m_Holder.expired());

	MG_SIGNAL_ON_UPDATEMETAINFO

		if (itemRef == thisToken)
			return { {}, {} };

	SharedStr itemRefStr(itemRef.AsStrRange());

	return SearchContext()->FindBestItem(itemRefStr);
}

auto AbstrCalculator::GetSourceItem() const -> SharedTreeItem  // directly referred persistent object.
{
	assert(IsMetaThread());
	assert(IsSourceRef());

	TokenID supplRefID = GetLispExprOrg().GetSymbID();
	auto foundItem = FindItem(supplRefID);
	if (!foundItem)
	{
		auto errMsg = MakeUnknownIdentifierErrorMsg(supplRefID.AsSharedStr(), FindBestItem(supplRefID));
		throwDmsErrD(errMsg.c_str());
	}
	return foundItem;
}

auto AbstrCalculator::VisitSourceItem(TokenID supplRefID, SupplierVisitFlag svf, const ActorVisitor& visitor) const -> std::optional<SharedTreeItem>  // directly referred persistent object.
{
	assert(IsMetaThread());

	if (supplRefID == thisToken)
		return SharedTreeItem{};

	SharedStr itemRefStr(supplRefID.AsStrRange());
	if (Test(svf, SupplierVisitFlag::ImplSuppliers))
		return SearchContext()->FindAndVisitItem(itemRefStr, svf, visitor);
	auto searchResult = SearchContext()->FindItem(itemRefStr);
	if (Test(svf, SupplierVisitFlag::NamedSuppliers))
		if (visitor.Visit(searchResult.get()) == AVS_SuspendedOrFailed)
			return {};
	return searchResult;

}

bool AbstrCalculator::IsSourceRef() const
{
	auto lispRefOrg = GetLispExprOrg();
	if (!lispRefOrg.IsSymb())
		return false;
	auto symbID = lispRefOrg.GetSymbID();
	if (ValueClass::FindByScriptName(symbID))
		return false;
	if (token::isConst(symbID))
		return false;
	return true;
}

AcConstructor* s_Constructor = nullptr;

AcConstructor* AbstrCalculator::GetConstructor()
{
	assert(s_Constructor); // CalcualtionRule parsing module must be registered
	return s_Constructor;
}

void AbstrCalculator::SetConstructor(AcConstructor* constructor)
{
	s_Constructor = constructor;
}

bool AbstrCalculator::HasTemplSource() const
{
	try {
		auto metaInfo = GetMetaInfo();
		return metaInfo.index() == 0 && std::get<MetaFuncCurry>(metaInfo).applyItem != nullptr;
	}
	catch (...)
	{
		return false;
	}
}

const TreeItem* AbstrCalculator::GetTemplSource() const
{ 
	auto metaInfo = GetMetaInfo();
	return std::get<MetaFuncCurry>(metaInfo).applyItem;
}

bool AbstrCalculator::IsForEachTemplHolder() const
{
	auto metaInfo = GetMetaInfo();
	if (metaInfo.index() != 0)
		return false;
	if (std::get<MetaFuncCurry>(metaInfo).applyItem)
		return false;
	auto og = std::get<MetaFuncCurry>(metaInfo).og;
	if (!og)
		return false;
	return og->HasTemplArg();
}

SharedTreeItem AbstrCalculator::GetForEachTemplSource() const
{ 
	auto metaInfo = GetMetaInfo();

	// PRECONDITION: IsForEachTemplHolder
	dms_assert(metaInfo.index() == 0);
	dms_assert(std::get<MetaFuncCurry>(metaInfo).applyItem);

	auto operGroup = std::get<MetaFuncCurry>(metaInfo).og;
	dms_assert(operGroup);
	dms_assert(operGroup->HasTemplArg());

	arg_index i = 0;
	SharedStr firstArgValue;

	SubstitutionBuffer substBuffer(false);

	LispPtr argRef = std::get<MetaFuncCurry>(metaInfo).fullLispExpr.Right();
	while (!argRef.EndP())
	{
		if (operGroup->IsArgTempl(i, firstArgValue.begin()))
			return FindItem(argRef.Left().GetSymbID());
		if (i == 0 && operGroup->HasDynamicArgPolicies())
		{
			auto dc = GetOrCreateDataController(std::get<1>(SubstituteExpr(substBuffer, argRef.Left())));
			FutureData fd = dc->CalcCertainResult();
			firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(fd->GetOld())))->GetIndexedValue(0);
		}
		argRef = argRef.Right(); ++i;
	}
	return {};
}

AbstrCalculatorRef AbstrCalculator::ConstructFromStr(const TreeItem* context, WeakStr expr, CalcRole cr)
{
	dms_assert(expr.IsDefined());
	SharedStr evaluatedExpr = EvaluatePossibleStringExpr(context, expr, cr);
	return ConstructFromDirectStr(context, evaluatedExpr, cr);
}

AbstrCalculatorRef AbstrCalculator::ConstructFromDirectStr(const TreeItem* context, WeakStr expr, CalcRole cr)
{
	return GetConstructor()->ConstructExpr(context, expr, cr);
}

AbstrCalculatorRef AbstrCalculator::ConstructFromLispRef(const TreeItem* context, LispPtr lispExpr, CalcRole cr)
{
	DBG_START("AbstrCalculator", "ConstructFromLispRef", false);
	DBG_TRACE(("lispExpr {}", AsFLispSharedStr(lispExpr, FormattingFlags::ThousandSeparator).c_str()));

	return new DC_Ptr(context, lispExpr, cr);
}

AbstrCalculatorRef AbstrCalculator::ConstructFromDBT(AbstrDataItem* context, const AbstrCalculator* src)
{
	return GetConstructor()->ConstructDBT(context, src);
}

SharedStr AbstrCalculator::GetAsFLispExprOrg(FormattingFlags ff) const
{
	return AsFLispSharedStr(GetLispExprOrg(), ff);
}

/*REMOVE
SharedStr AbstrCalculator::GetAsFLispExpr(FormattingFlags ff) const
{
	auto metaInfo = GetMetaInfo();
	return AsFLispSharedStr(GetAsLispRef(metaInfo), ff);
}
*/

UInt32 CountIndirections(CharPtr expr)
{
	dms_assert(expr);
	CharPtr exprBegin = expr;
	while (*expr && AbstrCalculator::MustEvaluate(expr) )
		++expr;
	return expr - exprBegin;
}

BestItemRef AbstrCalculator::GetErrorSource(const TreeItem* context, WeakStr expr)
{
	if (expr.empty())
		return {};

	auto exprPtr = expr.AsRange();
	auto nrEvals = CountIndirections(exprPtr.first);
	if (!nrEvals)
		return {};

	exprPtr.first += nrEvals;

	dms_assert(IsMetaThread());
	dms_assert(nrEvals); // else MustEvaluate would have returned false; PRECONDITION

	dms_assert(!MustEvaluate(exprPtr.begin()));
	FencedInterestRetainContext irc("AbstrCalculator::GetErrorSource");

	SharedStr resultStr(exprPtr);
	dms_assert(!MustEvaluate(resultStr.begin()));
	if (!context->InTemplate())
		while (nrEvals-- && !resultStr.empty())
		{
			AbstrCalculatorRef calculator = ConstructFromDirectStr(context, resultStr, CalcRole::Other);
			assert(calculator);
			auto res = CalledCalcHandle(calculator.get(), DataArray<SharedStr>::GetStaticClass());
			assert(res);
			if (res->WasFailed())
				return calculator->FindErrorneousItem();

			auto resItem = res->GetOld();

			irc.Add(res.get_ptr());
			irc.Add(resItem);

			const AbstrDataItem* resDataItem = AsDataItem(resItem);
			assert(resDataItem);

			if (WasInFailed(resDataItem))
			{
				if (resDataItem->WasFailed())
					return calculator->FindErrorneousItem();
				else
					return { resDataItem->GetTreeParent(), {} };
			}

			resultStr = GetValue<SharedStr>(resDataItem, 0);

			auto nrNewEvals = CountIndirections(resultStr.c_str());
			if (nrNewEvals)
				resultStr.erase(0, nrNewEvals);
			nrEvals += nrNewEvals;
		}
	return {};
}

SharedStr AbstrCalculator::EvaluatePossibleStringExpr(const TreeItem* context, WeakStr expr, CalcRole cr)
{
	if (expr.empty())
		return SharedStr();

	CharPtr exprPtr = expr.c_str();
	auto nrEvals = CountIndirections(exprPtr);
	if (!nrEvals)
		return expr;

	return EvaluateExpr(context, CharPtrRange(exprPtr+nrEvals, expr.send()), cr, nrEvals);
}

SharedStr AbstrCalculator::EvaluateExpr(const TreeItem* context, CharPtrRange expr, CalcRole cr, UInt32 nrEvals)
{
	dms_assert(IsMetaThread());
	dms_assert(nrEvals); // else MustEvaluate would have returned false; PRECONDITION
	dms_assert(!expr.empty()); // idem

	dms_assert(!MustEvaluate(expr.begin()));
	FencedInterestRetainContext irc("EvaluateExpr");

	SharedStr resultStr(expr);
	if (!context->InTemplate())
	while (nrEvals-- && !resultStr.empty())
	{
		AbstrCalculatorRef calculator = ConstructFromDirectStr(context, resultStr, cr);
		auto dc = MakeResult(calculator.get());
		irc.Add(dc.get());
		
		auto res = CalledCalcHandle(calculator.get(), DataArray<SharedStr>::GetStaticClass());
		assert(res);
		if (res->WasFailed()) // covers MetaInfo, Data, and Validate failures
			res->ThrowFail();

		auto resItem = res->GetOld();

		irc.Add(res.get_ptr());
		irc.Add(resItem);

		const AbstrDataItem* resDataItem = AsDataItem(resItem);
		assert(resDataItem);
		if (!resDataItem->WasFailed(FailType::Data))
			resultStr = GetTheValue<SharedStr>(resDataItem);
		if (resDataItem->WasFailed(FailType::Validate))
			context->Fail(resDataItem, FailType::MetaInfo);

		if (resDataItem->WasFailed(FailType::Committed)) // just pass on commit failures.
			context->Fail(resDataItem);

		if (!resultStr.IsDefined())
			// null is not a rule. A rule that deliberately results in no calculation rule must
			// evaluate to the empty string '' (see CalcFactory::ConstructExpr); a null result is an
			// oversight in the referenced value, and is failed here, the last point where the two
			// are distinguishable -- one level down, null and a deliberate '' both arrive empty().
			context->Fail("the '=' indirection evaluated to null; a rule that deliberately results in no calculation rule must evaluate to the empty string ''", FailType::MetaInfo);

		auto nrNewEvals = CountIndirections( resultStr.c_str() );
		if (nrNewEvals)
			resultStr.erase(0, nrNewEvals);
		nrEvals += nrNewEvals;
	}
	return resultStr;
}

ActorVisitState AbstrCalculator::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	assert(IsMetaThread());
	GetMetaInfo();

	assert(Test(svf, SupplierVisitFlag::NamedSuppliers) || Test(svf, SupplierVisitFlag::ImplSuppliers) || Test(svf, SupplierVisitFlag::Checker));
	if (dynamic_cast<const DC_Ptr*>(this))
		return AVS_Ready;

	if (IsSourceRef())
	{
		TokenID supplRefID = GetLispExprOrg().GetSymbID();
		auto optionalSourceItem = VisitSourceItem(supplRefID, svf, visitor);
		return optionalSourceItem ? AVS_Ready : AVS_SuspendedOrFailed;
	}

	if (!Test(svf, SupplierVisitFlag::ImplSuppliers))
	{
		for (const auto& suppl : m_NamedSuppliers)
		{
			auto visitResult = visitor(suppl.get());
			if (visitResult == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}

		return AVS_Ready;
	}

	SubstitutionBuffer substBuff(false);
	substBuff.svf = svf;
	substBuff.optionalVisitor = &visitor;
	SubstituteExpr(substBuff, RewriteExpr(GetLispExprOrg()));
	return substBuff.avs;
}

ActorVisitState AbstrCalculator::VisitImplSuppl(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* context, WeakStr expr, CalcRole cr)
{
	if (expr.empty())
		return AVS_Ready;

	CharPtr exprPtr = expr.c_str();
	auto nrEvals = CountIndirections(exprPtr);
	if (!nrEvals)
		return AVS_Ready;

	CDebugContextHandle   dContext("AbstrCalculator", exprPtr, false);
	TreeItemContextHandle checkPtr(context, "Context");

	dms_assert(nrEvals);
	FencedInterestRetainContext irc("AbstrCalculator::VisitImplSuppl");

	SharedStr resultStr(exprPtr+nrEvals MG_DEBUG_ALLOCATOR_SRC("AbstrCalculator::VisitImplSuppl.resultStr")); // creates a new copy of exprPtr
	while (!resultStr.empty())
	{
		AbstrCalculatorRef calculator = ConstructFromDirectStr(const_cast<TreeItem*>(context), resultStr, cr);
		auto dc = MakeResult(calculator.get());
		irc.Add(dc.get());

		auto res = CalledCalcHandle(calculator.get(), DataArray<SharedStr>::GetStaticClass());
		irc.Add(res.get_ptr());
		irc.Add(res->GetOld());

		if (calculator->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
		res->VisitSuppliers(svf, visitor);

		if (res->WasFailed())
			break; // referenced item missing or data failure: can't evaluate further indirections

		const AbstrDataItem* resDataItem = AsDataItem(res->GetOld());
		assert(resDataItem);

		if (!--nrEvals)
			break;

		resultStr = GetValue<SharedStr>(resDataItem, 0);

		auto nrNewEvals = CountIndirections( resultStr.c_str() );
		if (nrNewEvals)
			resultStr.erase(0, nrNewEvals);
		nrEvals += nrNewEvals;
	}
	return AVS_Ready;
}

SharedTreeItem AbstrCalculator::GetSearchContext(const TreeItem* holder, CalcRole cr)
{
	assert(holder);
	auto searchContext = holder->GetTreeParent();
	if (searchContext && cr == CalcRole::ArgCalc)
		searchContext = searchContext->GetTreeParent();
	if (!searchContext)
		searchContext = SessionData::Curr()->GetConfigRoot();

	assert(searchContext);
	return searchContext;
}

BestItemRef AbstrCalculator::FindErrorneousItem() const
{
	if (m_BestGuessErrorSuppl.first && m_BestGuessErrorSuppl.first->WasFailed())
		return m_BestGuessErrorSuppl;

	const TreeItem* errorneousItem = nullptr;
	auto errorChecker = [&errorneousItem](const Actor* a)
		{
			auto ti = dynamic_cast<const TreeItem*>(a);
			if (ti && !ti->IsCacheItem())
			{
				for (auto ri = ti; ri; ri = ri->GetCurrRefItem().get())
					if (WasInFailed(ri))
					{
						errorneousItem = ti;
						return  AVS_SuspendedOrFailed;
					}
				if (auto miDcPtr = ti->GetCalculatorMember())
				{
					if (miDcPtr->IsSourceRef())
						if (auto si = miDcPtr->GetSourceItem())
							for (SharedTreeItem ri = si; ri; ri = ri->GetCurrRefItem())
								if (WasInFailed(ri.get()))
								{
									errorneousItem = si.get();
									return  AVS_SuspendedOrFailed;
								}
				}
			}

			return AVS_Ready;
		};
	auto visitor = MakeDerivedBoolVisitor(std::move(errorChecker));

	if (IsSourceRef())
	{
		TokenID supplRefID = GetLispExprOrg().GetSymbID();
		VisitSourceItem(supplRefID, SupplierVisitFlag::CalcErrorSearch, std::move(visitor));
	}
	else
		VisitSuppliers(SupplierVisitFlag::CalcErrorSearch, std::move(visitor));

	return { make_shared_tree(errorneousItem, existing_obj{}), {} };
}

BestItemRef AbstrCalculator::FindPrimaryDataFailedItem() const
{
	const TreeItem* errorneousItem = nullptr;
	auto errorChecker = [&errorneousItem](const Actor* a)
		{
			auto ti = dynamic_cast<const TreeItem*>(a);
			if (ti && !ti->IsCacheItem())
			{
				if (WasInFailed(ti))
					goto foundError;

				try {
					if (IsDataItem(ti))
					{
						SharedDataItemInterestPtr adi = AsDataItem(ti);
						adi->PrepareDataUsage(DrlType::Certain);

						DataReadLock lock(adi);
					}
					if (IsUnit(ti))
					{
						SharedUnitInterestPtr au = AsUnit(ti);
						au->PrepareDataUsage(DrlType::Certain);
						au->GetCount();
					}
				}
				catch (...)
				{
					ti->CatchFail(FailType::Data);
				}
				if (ti->WasFailed(FailType::Data))
					goto foundError;
			}
			return AVS_Ready;
		foundError:
			errorneousItem = ti;
			return  AVS_SuspendedOrFailed;

		};
	auto visitor = MakeDerivedBoolVisitor(std::move(errorChecker));

	if (IsSourceRef())
	{
		TokenID supplRefID = GetLispExprOrg().GetSymbID();
		VisitSourceItem(supplRefID, SupplierVisitFlag::NamedSuppliers, std::move(visitor));
	}
	else
		VisitSuppliers(SupplierVisitFlag::NamedSuppliers, std::move(visitor));
	return { MakeSharedFromBorrowedObjectPtr(errorneousItem), {} };

}

// *****************************************************************************
// struct SubstitutionBuffer 
// *****************************************************************************

LispRef& SubstitutionBuffer::BufferedLispRef(metainfo_policy_flags mpf, LispPtr key)
{
	int index =
		(mpf & metainfo_policy_flags::suppl_tree) ? 2 :
		(mpf & metainfo_policy_flags::subst_never) ? 1 : 0;
	return m_SubstituteBuffer[index][key];
}


LispRef AbstrCalculator::slSupplierExpr(SubstitutionBuffer& substBuff, LispPtr supplRef, metainfo_policy_flags mpf) const
{
	TokenID supplRefID = supplRef.GetSymbID();
	if (token::isConst(supplRefID))
		return ExprList(supplRefID);

	auto supplier = FindOrVisitItem(substBuff, supplRefID);
	if (substBuff.optionalVisitor)
		return {};

	if (!supplier || supplier->IsCacheItem())
	{
		auto holder = m_Holder.lock();
		if (!holder)
			throwTaskCanceled();

		auto x = FindBestItem(supplRefID);
		if (!m_BestGuessErrorSuppl.first && x.first && !x.first->IsCacheItem() && x.first->WasFailed())
			m_BestGuessErrorSuppl = x;

		// #1188: fail AND stop here; never hand an unresolvable name back as a bare symbol.
		// A relative name in a substituted expression becomes a SymbDC that FindItem cannot
		// resolve from the config root, which then fails with an anonymous, unclickable
		// "Cannot find Item <name>" -- once per dependent DataController, hence the log spam,
		// while the real diagnosis is this "Unknown identifier '<name>'" on the item that holds
		// the reference. Throwing keeps the failure attached to that item; its consumers get it
		// through the regular supplier-failure route, which does refer to a config item.
		holder->ThrowFail(MakeUnknownIdentifierErrorMsg(supplRefID.AsSharedStr(), x), FailType::MetaInfo);
	}

	return slSupplierExprImpl(substBuff, supplier.get(), mpf);
}

void registerSupplier(SubstitutionBuffer& substBuff, const TreeItem* supplier)
{
	if (!substBuff.m_CollectSuppliers)
		return;

	// register an sequential ordinal for each first occurence of a supplier
	auto& countref = substBuff.m_SupplierSet[supplier];
	if (!countref)
		countref = substBuff.m_SupplierSet.size();
}


LispRef AbstrCalculator::slSupplierExprImpl(SubstitutionBuffer& substBuff, const TreeItem* supplier, metainfo_policy_flags mpf) const
{
	DBG_START("ExprCalculator", "slSupplierExprImpl", false);
	//	DBG_TRACE(("expr     {}", m_Expression.c_str()));
	//	DBG_TRACE(("supplRef {}", AsString(supplRef).c_str()) );

	dms_assert(supplier); // PRECONDITION
	auto holder = m_Holder.lock();
	if (!holder)
		throwTaskCanceled();
	if (holder->DoesContain(supplier))
	{
		if (m_CalcRole == CalcRole::Calculator || supplier != holder.get())
			holder->ThrowFail("Calulation rule would create a circular dependency", FailType::MetaInfo);
	}
	else
		supplier->UpdateMetaInfo();
	if (supplier->InTemplate() && !(mpf & metainfo_policy_flags::subst_never))
	{
		auto msg = mySSPrintF("Calulation rule would create a dependency on {} which is (part of) a template", supplier->GetFullName());
		holder->ThrowFail(msg, FailType::MetaInfo);
	}

	if (holder.get() != supplier)
		registerSupplier(substBuff, supplier);

	if (mpf & metainfo_policy_flags::subst_never || (!supplier->IsPassor() && !supplier->HasCalculator() && !IsDataItem(supplier) && !IsUnit(supplier)))
		return CreateLispTree(supplier, mpf & metainfo_policy_flags::suppl_tree);

	LispRef result = (m_CalcRole == CalcRole::Checker && holder.get() == supplier) ? supplier->GetKeyExprImpl() : supplier->GetCheckedKeyExpr();

#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace, "result={}", AsString(result).c_str());
	dms_assert(IsExpr(result));
#endif
	if (result.EndP())
		supplier->throwItemError("SubstitutionError");

	assert(!result.EndP());
	return result;
}

LispRef AbstrCalculator::SubstituteArgs(SubstitutionBuffer& substBuff, LispPtr localArgs, const AbstrOperGroup* og, arg_index argNr, SharedStr firstArgValue) const
{
	assert(og);

	// Iterate the arg list rather than tail-recursing on localArgs.Right();
	// keeps stack usage O(1) for long arg lists. Forward pass collects each
	// frame's substituted Left(); the right-to-left unwind preserves the
	// "no change -> return original pair" structural-sharing path per frame.
	struct frame_t
	{
		LispPtr originalPair; // == localArgs at this depth in the recursive form
		LispRef left;         // substituted Left()
	};
	std::vector<frame_t> frames;

	LispPtr cursor = localArgs;
	while (!cursor.EndP())
	{
		assert(cursor.IsList());

		metainfo_policy_flags mpf = arg2metainfo_polcy(og->GetArgPolicy(argNr, firstArgValue.begin()));
		LispRef left = SubstituteExpr_impl(substBuff, cursor.Left(), mpf);
		if (substBuff.avs == AVS_SuspendedOrFailed)
			return {};

		if (argNr == 0 && og->HasDynamicArgPolicies())
		{
			auto dc = GetOrCreateDataController(left);
			FutureData fd = dc->CalcCertainResult();
			firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(fd->GetOld())))->GetIndexedValue(0);
		}

		frames.push_back({cursor, std::move(left)});
		cursor = cursor.Right();
		++argNr;
	}

	if (substBuff.optionalVisitor)
		return {}; // visit-only mode: traversal recorded the suppliers; result is discarded

	// Build the substituted list right-to-left, reusing original pairs whenever
	// neither this frame's left nor the accumulated right has changed.
	LispRef right = cursor; // terminal EndP value (base case in the recursive form)
	for (auto it = frames.rbegin(); it != frames.rend(); ++it)
	{
		if (right == it->originalPair.Right() && it->left == it->originalPair.Left())
			right = it->originalPair;
		else
			right = LispRef(it->left, right);
	}
	return right;
}

// *****************************************************************************
// user-defined function application: meta-time beta-reduction (inline path)
//
// A function call that is used as an expression (nested, or bound to a typed
// holder) is reduced to a self-contained key expression: parameter references
// become the substituted argument key expressions, body-item references become
// their recursively reduced calculation rules, and imports/externals resolve
// through the function's strict search space (own sub-items + explicit usings).
// The tiled engine only ever sees the resulting first-order operator expression;
// identical applications intern to identical keys (applicative semantics).
// *****************************************************************************

namespace {

	static TokenID t_Hole = GetTokenID_st("_"); // partial-application placeholder
	static TokenID t_Map  = GetTokenID_st("map"); // built-in map(function, container) metafunction

	// WP4.5: the auto-imported standard prelude ('prelude' container under the config
	// root). A call head that resolves to a NON-callable item (e.g. a documentation
	// container that happens to carry an operator-like name) does not capture the call:
	// the prelude binding applies instead. Also the fallback for call heads inside
	// strict function scopes, which do not see the root's namespace usage.
	SharedTreeItem FindPreludeFunction(TokenID nameID)
	{
		static TokenID t_PreludeContainer = GetTokenID_st("prelude");
		auto sd = SessionData::Curr();
		if (!sd)
			return {};
		auto root = sd->GetConfigRoot();
		if (!root)
			return {};
		auto pre = root->GetConstSubTreeItemByID(t_PreludeContainer);
		if (!pre)
			return {};
		auto f = pre->GetConstSubTreeItemByID(nameID);
		if (!f || !f->IsTemplate())
			return {};
		return f;
	}
	static TokenID t_ApplyItem       = GetTokenID_st("apply_item");       // §5.9 'apply X(args)' marker head
	static TokenID t_InstantiateItem = GetTokenID_st("instantiate_item"); // §5.9 'instantiate X(args)' marker head
	static TokenID t_ApplyValue      = GetTokenID_st("apply_value");      // §5.10 '(args)' applied to a call result
	static TokenID t_ContainerLiteral = GetTokenID_st("container_literal"); // §5.9 '{ m: e; … }' argument literal
	static TokenID t_Member           = GetTokenID_st("member");            // §5.9 '(member name value)'
	static TokenID t_NoDomain         = GetTokenID_st("no_domain");         // §5.9 domain-less literal marker
	static TokenID t_Dot              = GetTokenID_st(".");                 // §5.9 current-domain reference in members

	// a destructured container-literal argument: the domain (if any) and its named members,
	// all already resolved to keys in the caller scope; bound to a structured parameter and
	// consumed member-by-member during body substitution (no anonymous item is materialized).
	struct ContainerLiteralArg
	{
		bool                                     hasDomain = false;
		LispRef                                  domainKey; // resolved domain unit key
		std::vector<std::pair<TokenID, LispRef>> members;   // member name -> resolved value key
	};

	// replace every bare '.' symbol (the current-domain reference) in a container-literal
	// member expression with the literal's domain expression, before caller-scope resolution
	LispRef ReplaceDot(LispPtr expr, LispPtr domainExpr)
	{
		if (expr.EndP())
			return expr;
		if (expr.IsSymb())
			return (expr.GetSymbID() == t_Dot) ? LispRef(domainExpr) : LispRef(expr);
		if (expr.IsRealList())
			return LispRef(ReplaceDot(expr.Left(), domainExpr), ReplaceDot(expr.Right(), domainExpr));
		return LispRef(expr);
	}

	// build a destructured container-literal argument from
	// (container_literal <domain|no_domain> (member name value)…), resolving the domain and
	// each member value (with '.' rebound to the domain) through `resolve` (caller or body scope)
	std::shared_ptr<ContainerLiteralArg> BuildContainerLiteral(LispPtr litExpr, const std::function<LispRef(LispPtr)>& resolve)
	{
		auto lit = std::make_shared<ContainerLiteralArg>();
		LispPtr domainExpr = litExpr.Right().Left();
		if (!(domainExpr.IsSymb() && domainExpr.GetSymbID() == t_NoDomain))
		{
			lit->hasDomain = true;
			lit->domainKey = resolve(domainExpr);
		}
		for (LispPtr m = litExpr.Right().Right(); !m.EndP(); m = m.Right())
		{
			LispPtr member = m.Left(); // (member name value)
			TokenID name    = member.Right().Left().GetSymbID();
			LispRef value   = ReplaceDot(member.Right().Right().Left(), domainExpr);
			lit->members.emplace_back(name, resolve(value));
		}
		return lit;
	}

	struct FunctionBinding;

	// one resolved argument to a function application: either a data/unit value
	// (key + optional plain-reference item), a function value (binding), or a hole
	struct CallArg
	{
		LispRef                          key;             // data/unit argument (empty otherwise)
		SharedTreeItem                   item;            // plain-reference item (member access), else null
		std::shared_ptr<FunctionBinding> binding;         // function value (plain ref or partial application), else null
		std::shared_ptr<ContainerLiteralArg> literal;     // §5.9 container-literal argument, else null
		bool                             isHole = false;  // '_' placeholder
		bool IsFunctionValue() const { return binding != nullptr; }
	};

	// §5.10 closure environment: the enclosing application's parameters and their bound
	// values, captured BY VALUE (already-substituted keys/items/bindings) when a nested
	// function is returned as a result. `next` chains the enclosing function's own
	// environment (nested closures). Because captured values are concrete interned
	// keys -- never unresolved symbols -- capture is hygienic by construction.
	struct ClosureEnv
	{
		const TreeItem*              funcItem = nullptr; // the enclosing function definition
		std::vector<CallArg>         args;               // its bound arguments, positionally
		std::shared_ptr<ClosureEnv>  next;               // the enclosing application's own env
	};

	// a function value: the function plus one slot per declared parameter. A slot with
	// isHole is unbound; applying the binding fills the holes left-to-right. `env` is
	// the captured closure environment when the function was returned as a result.
	struct FunctionBinding
	{
		SharedTreeItem       funcItem;
		std::vector<CallArg> slots;
		std::shared_ptr<ClosureEnv> env;
		UInt32 NrHoles() const { UInt32 n = 0; for (const auto& s : slots) if (s.isHole) ++n; return n; }
	};

	struct FunctionApplication
	{
		const TreeItem*                m_FuncItem = nullptr;
		const FunctionApplication*     m_Parent = nullptr; // enclosing application (nested calls only): recursion detection follows THIS chain, so unrelated re-entry through UpdateMetaInfo of externals cannot raise false 'recursive' errors
		SubstitutionBuffer*            m_SubstBuff = nullptr; // caller's buffer: body-resolved externals register as suppliers of the calling item
		SharedTreeItem                 m_ErrorHolder; // caller item, for failure attribution
		std::vector<LispRef>           m_ArgKeys;     // per param: data/unit key (empty for function-valued params)
		std::vector<SharedTreeItem>    m_ArgItems;    // per param: the referenced item iff the argument was a plain reference (enables member access), else null
		std::vector<std::shared_ptr<FunctionBinding>> m_ArgBindings; // per param: the bound function value iff the argument is a function, else null
		std::vector<std::shared_ptr<ContainerLiteralArg>> m_ArgLiterals; // per param: the container-literal argument, else null
		std::shared_ptr<ClosureEnv>    m_Env;         // §5.10: closure environment of the applied function, else null
		std::vector<const TreeItem*>   m_Params;      // the first N sub-items of m_FuncItem
		const TreeItem*                m_RestParam = nullptr; // '...x' rest param (the last param); binds m_ArgKeys[nrParams-1 .. end)
		std::map<const TreeItem*, LispRef> m_Reductions;
		std::set<const TreeItem*>      m_InProgress;

		void PushArg(const CallArg& a) { m_ArgKeys.push_back(a.key); m_ArgItems.push_back(a.item); m_ArgBindings.push_back(a.binding); m_ArgLiterals.push_back(a.literal); }

		// §5.10: this application's bound parameters as a closure environment, chained
		// to the environment it was itself applied in. Captured by value, so the result
		// is independent of where the capturing function value travels (#1166).
		std::shared_ptr<ClosureEnv> MakeCurrentEnv() const
		{
			auto env = std::make_shared<ClosureEnv>();
			env->funcItem = m_FuncItem;
			UInt32 nrParams = TreeItem_GetFunctionParamCount(m_FuncItem);
			env->args.reserve(nrParams);
			for (UInt32 i = 0; i != nrParams && i != m_ArgKeys.size(); ++i)
			{
				CallArg a;
				a.key = m_ArgKeys[i]; a.item = m_ArgItems[i];
				a.binding = m_ArgBindings[i]; a.literal = m_ArgLiterals[i];
				env->args.push_back(std::move(a));
			}
			env->next = m_Env;
			return env;
		}

		bool IsRestParamSymbol(TokenID sym) const { return m_RestParam && m_RestParam->GetID() == sym; }
		void SpliceRestArgs(std::vector<CallArg>& out) const
		{
			assert(m_RestParam);
			for (UInt32 k = TreeItem_GetFunctionParamCount(m_FuncItem) - 1; k != m_ArgKeys.size(); ++k)
			{
				CallArg a; a.key = m_ArgKeys[k]; a.item = m_ArgItems[k]; a.binding = m_ArgBindings[k]; a.literal = m_ArgLiterals[k];
				out.push_back(std::move(a));
			}
		}

		LispRef Reduce();
		CallArg ReduceValue(); // §5.10: like Reduce, but a function-typed result yields a closure binding
		bool ResolveEnvSymbol(TokenID symbID, SharedTreeItem* foundItemPtr, LispRef* keyPtr, std::shared_ptr<FunctionBinding>* bindingPtr); // §5.10 closure-env lookup
		LispRef ReduceBodyItem(const TreeItem* bodyItem);
		LispRef SubstituteBodyExpr(const TreeItem* refScope, LispPtr expr);
		LispRef ResolveBodySymbol(const TreeItem* refScope, TokenID symbID, SharedTreeItem* foundItemPtr);
		CallArg ResolveBodyArg(const TreeItem* refScope, LispPtr argExpr);
		SharedTreeItem ResolveBodyHeadFunction(const TreeItem* refScope, TokenID headID, std::shared_ptr<FunctionBinding>* paramBinding, bool mayFail = false);
	};

	void CheckFunctionDefinition(const TreeItem* funcItem); // WP3.4 + tranche 3 typed walker, defined below
	const TreeItem* ResolveVariant(const TreeItem* setItem, const std::vector<CallArg>& callArgs, SharedTreeItem errorHolder); // §5.7, defined below

	// a plain function reference is a binding with every slot a hole
	std::shared_ptr<FunctionBinding> MakeAllHoles(SharedTreeItem func)
	{
		auto b = std::make_shared<FunctionBinding>();
		b->funcItem = func;
		UInt32 n = TreeItem_GetFunctionParamCount(func.get());
		b->slots.resize(n);
		for (auto& s : b->slots) s.isHole = true;
		return b;
	}

	// #1166: a function nested in another function's BODY is applied in the enclosing
	// application's scope, so its body may reference the enclosing parameters. Give
	// such a callee this application's environment; a callee reached from the
	// definition scope or the prelude is not nested and gets none.
	bool IsNestedInside(const TreeItem* callee, const TreeItem* outer)
	{
		if (!callee || !outer)
			return false;
		for (auto p = callee->GetTreeParent(); p; p = p->GetTreeParent())
			if (p.get() == outer)
				return true;
		return false;
	}

	// fill the holes of `b` with `holeFills` left-to-right; the counts must match --
	// except for a '...x' rest function, whose LAST hole absorbs all surplus fills
	FunctionBinding MergeBinding(const FunctionBinding& b, const std::vector<CallArg>& holeFills)
	{
		bool hasRest = b.funcItem && b.funcItem->IsFunctionItem() && TreeItem_HasFunctionRestParam(b.funcItem.get());
		if (hasRest ? holeFills.size() < b.NrHoles() : holeFills.size() != b.NrHoles())
			throwErrorF("ExprParser", "'{}': function expects {}{} argument(s); {} provided"
				, b.funcItem->GetFullName().c_str(), b.NrHoles(), hasRest ? " or more" : "", holeFills.size());
		FunctionBinding r; r.funcItem = b.funcItem; r.env = b.env;
		UInt32 c = 0;
		for (const auto& slot : b.slots)
			r.slots.push_back(slot.isHole ? holeFills[c++] : slot);
		while (c < holeFills.size()) // rest surplus (guarded: non-rest counts are equal above)
			r.slots.push_back(holeFills[c++]);
		return r;
	}

	// §5.10: reduce a fully-bound application to its VALUE -- a data key, or a closure
	// binding when the applied function has a function-typed result
	CallArg ReduceMergedValue(const FunctionBinding& merged, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder)
	{
		FunctionApplication appl;
		appl.m_FuncItem = merged.funcItem.get();
		appl.m_Parent = parent;
		appl.m_SubstBuff = substBuff;
		appl.m_ErrorHolder = errorHolder;
		appl.m_Env = merged.env;
		for (const auto& slot : merged.slots)
			appl.PushArg(slot);
		return appl.ReduceValue();
	}

	LispRef ReduceMerged(const FunctionBinding& merged, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder)
	{
		CallArg r = ReduceMergedValue(merged, parent, substBuff, errorHolder);
		if (r.binding)
			throwErrorF("ExprParser", "'{}': a function value can only be applied with '(...)', passed as an argument, or returned as a result"
				, merged.funcItem->GetFullName().c_str());
		return r.key;
	}

	// caller-side (non-body) argument resolution: build a CallArg from a caller-scope
	// expression. `resolveData` substitutes an ordinary data expression to its key;
	// `findItem` resolves a bare symbol to its item (null if absent). A function
	// application with holes yields a partial binding; a full one is reduced to data.
	CallArg ResolveCallerArg(LispPtr argExpr,
		const std::function<LispRef(LispPtr)>& resolveData,
		const std::function<SharedTreeItem(TokenID)>& findItem,
		SubstitutionBuffer* substBuff, SharedTreeItem errorHolder)
	{
		if (argExpr.IsSymb())
		{
			TokenID sym = argExpr.GetSymbID();
			if (sym == t_Hole)
			{
				CallArg a; a.isHole = true; return a;
			}
			if (!token::isConst(sym) && !ValueClass::FindByScriptName(sym))
			{
				SharedTreeItem item = findItem(sym); // plain-reference item (member access) + function detection
				if (!item)
					if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
						item = pf; // prelude: implicit outermost namespace, also for function references
				if (item && item->IsFunctionItem())
				{
					if (substBuff) registerSupplier(*substBuff, item.get());
					CallArg a; a.binding = MakeAllHoles(item); return a;
				}
				CallArg a; a.key = resolveData(argExpr); a.item = item; return a;
			}
		}
		if (argExpr.IsRealList() && argExpr.Left().IsSymb())
		{
			TokenID headID = argExpr.Left().GetSymbID();

			// §5.9 container literal: resolve domain + members in caller scope ('.' -> domain)
			if (headID == t_ContainerLiteral)
			{
				CallArg a; a.literal = BuildContainerLiteral(argExpr, resolveData); return a;
			}

			// §5.10 applied call result as an argument: value or residual binding
			if (headID == t_ApplyValue)
			{
				CallArg fnVal = ResolveCallerArg(argExpr.Right().Left(), resolveData, findItem, substBuff, errorHolder);
				if (!fnVal.binding)
					throwErrorF("ExprParser", "'(...)' applied to an expression that is not a function value");
				std::vector<CallArg> outer;
				for (LispPtr a = argExpr.Right().Right(); !a.EndP(); a = a.Right())
					outer.push_back(ResolveCallerArg(a.Left(), resolveData, findItem, substBuff, errorHolder));
				FunctionBinding merged = MergeBinding(*fnVal.binding, outer);
				if (merged.NrHoles() == 0)
					return ReduceMergedValue(merged, nullptr, substBuff, errorHolder);
				CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
			}

			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			if (og->IsTemplateCall())
			{
				auto callee = findItem(headID);
				if (callee && callee->IsFunctionItem())
				{
					if (substBuff) registerSupplier(*substBuff, callee.get());
					std::vector<CallArg> sub;
					for (LispPtr a = argExpr.Right(); !a.EndP(); a = a.Right())
						sub.push_back(ResolveCallerArg(a.Left(), resolveData, findItem, substBuff, errorHolder));
					// §5.7: variant sets dispatch by argument type on nested calls too
					if (TreeItem_IsFunctionVariantSet(callee.get()))
					{
						auto variant = ResolveVariant(callee.get(), sub, errorHolder);
						callee = make_shared_tree(variant, existing_obj{});
						if (substBuff) registerSupplier(*substBuff, variant);
						CheckFunctionDefinition(variant);
					}
					FunctionBinding merged = MergeBinding(*MakeAllHoles(callee), sub);
					if (merged.NrHoles() == 0)
						return ReduceMergedValue(merged, nullptr, substBuff, errorHolder); // §5.10: data key OR closure binding
					CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
				}
			}
		}
		CallArg a; a.key = resolveData(argExpr); return a;
	}

	// §5.7 variant dispatch: value class of a reduced argument key
	const ValueClass* ArgValueClass(LispRef key, SharedTreeItem errorHolder)
	{
		if (key.EndP())
			return nullptr;
		auto dc = GetOrCreateDataController(key);
		auto res = dc->MakeResult();
		if (!res)
		{
			dms_assert(dc->WasFailed(FailType::MetaInfo));
			errorHolder->ThrowFail(dc.get());
		}
		if (IsDataItem(res.get()))
			return AsDataItem(res.get())->GetAbstrValuesUnit()->GetValueType();
		if (IsUnit(res.get()))
			return AsUnit(res.get())->GetValueType();
		return nullptr;
	}

	// declared value class of a variant parameter (params are inert, so prefer the
	// declared values-unit token -- a value-type name -- over resolving the unit)
	const ValueClass* ParamValueClass(const TreeItem* param)
	{
		if (IsDataItem(param))
		{
			if (auto vc = ValueClass::FindByScriptName(AsDataItem(param)->ValuesUnitToken()))
				return vc;
			auto vu = AsDataItem(param)->GetAbstrValuesUnit();
			return vu ? vu->GetValueType() : nullptr;
		}
		if (IsUnit(param))
			return AsUnit(param)->GetValueType();
		return nullptr;
	}

	// §5.7 v2: select the variant of `setItem` whose acceptance set matches the argument
	// value classes, taking the MOST SPECIFIC match (per-parameter subset comparison over
	// the closed value-class universe); a fully generic or plain position accepts more
	// than a concrete or tighter-constrained one. Definition-time disjointness
	// (TreeItem_CheckVariantSetDisjointness) guarantees overlapping variants are
	// specificity-ordered, so the most specific match is unique.
	const TreeItem* ResolveVariant(const TreeItem* setItem, const std::vector<CallArg>& callArgs, SharedTreeItem errorHolder)
	{
		std::vector<const ValueClass*> argVCs;
		argVCs.reserve(callArgs.size());
		for (const auto& a : callArgs)
			argVCs.push_back(ArgValueClass(a.key, errorHolder));

		const TreeItem* best = nullptr;
		SharedStr candidates;
		for (const TreeItem* v = setItem->_GetFirstSubItem(); v; v = v->GetNextItem())
		{
			if (!v->IsFunctionItem())
				continue;
			if (!candidates.empty())
				candidates = candidates + SharedStr(", ");
			candidates = candidates + SharedStr(v->GetID());

			if (!TreeItem_VariantMatches(v, argVCs))
				continue;
			if (!best)
			{
				best = v;
				continue;
			}
			int cmp = TreeItem_CompareVariantSpecificity(v, best);
			if (cmp == -1)
				best = v;
			else if (cmp != +1)
				throwErrorF("ExprParser", "call to variant set '{}': the arguments match variants '{}' and '{}' and neither is more specific"
					, setItem->GetFullName().c_str(), best->GetID().GetStr().c_str(), v->GetID().GetStr().c_str());
		}
		if (!best)
			throwErrorF("ExprParser", "call to variant set '{}': no variant matches the argument types (variants: {})"
				, setItem->GetFullName().c_str(), candidates.c_str());
		return best;
	}

	// structural compatibility of a bound function against a declared signature
	// exemplar: same arity, per-parameter and result item classes equal (a plain
	// TreeItem-classed signature position is a wildcard)
	// §5.10 Stage 2: does `tok` name a generic type/domain variable of `fn`?
	bool IsGenericVarOf(const TreeItem* fn, TokenID tok)
	{
		if (!tok)
			return false;
		UInt32 seqNr = 0, idx; TokenID var, cons; bool isDom;
		while (TreeItem_GetFunctionGenericParam(fn, seqNr++, &idx, &var, &cons, &isDom))
			if (var == tok)
				return true;
		return false;
	}

	// is `tok` a DOMAIN-sorted generic (`<D: domains>`) of fn? Only those carry a
	// unit IDENTITY; value-sorted generics range over classes. (isDom rides the
	// generic-parameter records, not the typeVars pair list.)
	bool IsDomainSortedVarOf(const TreeItem* fn, TokenID tok)
	{
		if (!tok)
			return false;
		UInt32 seqNr = 0, idx; TokenID var, cons; bool isDom;
		while (TreeItem_GetFunctionGenericParam(fn, seqNr++, &idx, &var, &cons, &isDom))
			if (var == tok)
				return isDom;
		return false;
	}

	// does `tok` appear in fn's OWN <...> type-parameter clause? (genericParams also
	// record lexically inherited enclosing variables; the ordered typeVars list holds
	// only the function's own declarations, so an own clause shadows the origin's)
	bool IsOwnDeclaredVar(const TreeItem* fn, TokenID tok)
	{
		if (!tok)
			return false;
		if (auto tvs = TreeItem_GetFunctionTypeVars(fn))
			for (const auto& tv : *tvs)
				if (tv.first == tok)
					return true;
		return false;
	}

	// ---- WP4.1 tranche 2: unification store over an application's type variables ----
	//
	// Robinson unification specialized to the shallow type terms of §5: value-class
	// variables and domain variables (concrete units are opaque, compared by
	// UnifyDomain -- key identity, §2). Variables are identified by (owner function,
	// name), so a signature-typed parameter can LINK the bound generic function's OWN
	// variable to one of the applied function's variables -- a genuine
	// variable-variable link, kept as a union-find equivalence class. Every class
	// carries at most one concrete binding and, for value variables, the intersection
	// of all member constraints as an acceptance set over the closed value-class
	// universe (the §5.7 v2 mechanism), so conflicts surface at the application that
	// creates them -- with attribution -- even when no member of the class is ever
	// bound concretely.

	using ValueClassSet = std::bitset<UInt32(ValueClassID::VT_Count)>;

	ValueClassSet GenericConstraintSet(TokenID constraintName)
	{
		ValueClassSet r;
		for (UInt32 v = 0; v != UInt32(ValueClassID::VT_Count); ++v)
			if (auto vc = ValueClass::FindByValueClassID(ValueClassID(v)))
				if (MatchesGenericConstraint(vc, constraintName))
					r.set(v);
		return r;
	}

	// the declared constraint of `var`: fn's ordered type-variable list, else the
	// generic-parameter records (which carry the same `<var: constraint>` pairs)
	TokenID DeclaredConstraintOf(const TreeItem* fn, TokenID var)
	{
		if (!fn)
			return TokenID(); // operator-signature variables have no owning function
		if (auto tvs = TreeItem_GetFunctionTypeVars(fn))
			for (const auto& tv : *tvs)
				if (tv.first == var)
					return tv.second;
		UInt32 seqNr = 0, idx; TokenID gv, cons; bool isDom;
		while (TreeItem_GetFunctionGenericParam(fn, seqNr++, &idx, &gv, &cons, &isDom))
			if (gv == var)
				return cons;
		return TokenID();
	}

	constexpr SizeT NO_TYPE_VAR = SizeT(-1);

	struct TypeUnifier
	{
		// tranche 3: variables are additionally keyed by an INSTANCE number, so every
		// binding/instantiation of a generic function gets its own copies of that
		// function's variables (two independent bindings of one generic function must
		// not link through a shared node). Instance 0 with owner == the checked
		// function marks that function's own variables; the definition-time walker
		// creates those as RIGID (skolem) variables: a rigid variable must hold for
		// EVERY instantiation, so it can never be bound to a concrete type, forced
		// equal to another rigid variable, or narrowed below its declared constraint.
		const TreeItem* m_ApplItem = nullptr; // the applied/checked function, for error attribution
		CharPtr m_Phase = "";                 // "" at application; "the definition of " at definition time

		// soft: an operator-support set (derived from the group's registered members,
		// OperSignature.h). A concrete class outside the set is a definition-time
		// error (reduction would find no member), but soft sets never narrow or
		// reject a rigid ∀-variable and stay out of `feasible`: operator support is
		// not a declared promise -- an unsupported instantiation fails at its own
		// reduction (S1), and the prelude's <T: any> null-aware predicates over
		// eq/lt depend on passing through them symbolically.
		struct ConstraintRec { TokenID name, constraint; SharedStr source; ValueClassSet set; bool soft = false; SharedStr setText = {}; };
		struct ValueNode
		{
			SizeT parent; // union-find: parent == own index at a root
			TokenID name; // the user-visible variable name (roots keep the rigid/outer one)
			bool rigid = false;
			const ValueClass* bound = nullptr;
			SharedStr boundSource;
			ValueClassSet feasible; // invariant: the intersection of all constraint sets
			std::vector<ConstraintRec> constraints;
		};
		// batch U (§8): the former DomainNode, generalized to ONE pool of unit-
		// identity nodes serving BOTH roles a unit can play -- the domain of a data
		// term AND (new) the values-unit identity of a data term. A unit variable
		// (unit parameter, domain-sorted generic) used in a values position of one
		// signature slot and a domain position of another therefore flows through a
		// single node -- the K2 bridge. Every unit node carries a companion CLASS
		// node (the ValueNode keyed by the same (owner, instance, name), so all
		// existing class-side resolution converges on it); the BindUnit/LinkUnit
		// invariant keeps unit identity and class reasoning consistent.
		struct UnitNode
		{
			SizeT parent;
			TokenID name;
			bool rigid = false;
			SharedTreeItem keepAlive; // owns the liveness of `bound`
			const AbstrUnit* bound = nullptr;
			SharedStr boundSource;
			SizeT classNode = NO_TYPE_VAR; // companion ValueNode: class-of(this unit)
		};
		// the `= {}` on these four keeps `TypeUnifier{ m_FuncItem }` (which names only the
		// first member) out of -Wmissing-field-initializers, like the members above
		std::vector<ValueNode> m_ValueNodes = {};
		std::vector<UnitNode>  m_UnitNodes = {};
		using VarKey = std::tuple<const TreeItem*, UInt32, TokenID>;
		std::map<VarKey, SizeT> m_ValueVarIndex = {}, m_UnitVarIndex = {};

		SizeT FindV(SizeT i) { while (m_ValueNodes[i].parent != i) i = m_ValueNodes[i].parent = m_ValueNodes[m_ValueNodes[i].parent].parent; return i; }
		SizeT FindU(SizeT i) { while (m_UnitNodes[i].parent != i) i = m_UnitNodes[i].parent = m_UnitNodes[m_UnitNodes[i].parent].parent; return i; }

		SharedStr FullName() const { return mySSPrintF("{}'{}'", m_Phase, m_ApplItem->GetFullName().c_str()); }

		// get-or-create the node for owner's variable; a freshly created node seeds its
		// acceptance set from the variable's declared constraint, attributed to
		// `declSource`; `fallbackConstraint` covers variables declared by an ENCLOSING
		// function (lexically visible, not in owner's own lists)
		SizeT ValueVar(const TreeItem* owner, UInt32 instance, TokenID name, const SharedStr& declSource, bool rigid = false, TokenID fallbackConstraint = TokenID())
		{
			auto [it, isNew] = m_ValueVarIndex.try_emplace(VarKey{ owner, instance, name }, m_ValueNodes.size());
			if (isNew)
			{
				ValueNode n; n.parent = m_ValueNodes.size(); n.name = name; n.rigid = rigid;
				n.feasible.set();
				TokenID cons = DeclaredConstraintOf(owner, name);
				if (!cons)
					cons = fallbackConstraint;
				if (cons)
				{
					ConstraintRec rec{ name, cons, declSource, GenericConstraintSet(cons) };
					n.feasible = rec.set;
					n.constraints.push_back(std::move(rec));
				}
				m_ValueNodes.push_back(std::move(n));
			}
			return it->second;
		}
		// get-or-create the unit-identity node for owner's variable, with its
		// companion class node created eagerly under the SAME (owner, instance,
		// name) key -- the one moment the key is known -- so any class-side path
		// (ValNode, signature bindings) resolves to the same node. `declaredCls`
		// is the DECLARED value class of a unit parameter (`unit<uint32> U`): the
		// identity varies per instantiation (rigid), but the class is pinned by
		// the declaration itself, so the companion binds concretely instead of
		// staying rigid; without it the companion follows the unit node's rigidity
		// and seeds from the variable's declared constraint (e.g. '<D: domains>').
		SizeT UnitVar(const TreeItem* owner, UInt32 instance, TokenID name, bool rigid = false, const ValueClass* declaredCls = nullptr, TokenID fallbackConstraint = TokenID())
		{
			auto [it, isNew] = m_UnitVarIndex.try_emplace(VarKey{ owner, instance, name }, m_UnitNodes.size());
			if (isNew)
			{
				// push the node BEFORE anything that can throw (the companion's
				// declared-class bind may): the index entry must never dangle
				SizeT idx = it->second;
				UnitNode n; n.parent = idx; n.name = name; n.rigid = rigid;
				m_UnitNodes.push_back(std::move(n));
				SharedStr declSource = mySSPrintF("the declaration of '{}'", name.GetStr().c_str());
				SizeT comp = ValueVar(owner, instance, name, declSource, rigid && !declaredCls, fallbackConstraint);
				m_UnitNodes[idx].classNode = comp;
				if (declaredCls)
					BindDeclaredClass(comp, declaredCls, declSource);
			}
			else if (declaredCls)
			{
				// a later caller may know the declared class the creating path did
				// not (get-or-create runs once; type applications and sig bindings
				// can reach a unit parameter's node before ParamType does) --
				// reconcile rather than silently drop the pin
				SizeT comp = m_UnitNodes[FindU(it->second)].classNode;
				if (comp != NO_TYPE_VAR)
					BindDeclaredClass(comp, declaredCls
						, mySSPrintF("the declaration of '{}'", name.GetStr().c_str()));
			}
			return it->second;
		}

		// bind a companion class node to a DECLARED class, but never onto a rigid
		// or already-bound node: a same-named type variable may legitimately own
		// the key (pathological shadowing) and an existing binding is either
		// already consistent or a conflict the unit side reports better -- defer
		void BindDeclaredClass(SizeT comp, const ValueClass* declaredCls, const SharedStr& declSource)
		{
			auto& cn = m_ValueNodes[FindV(comp)];
			if (!cn.rigid && !cn.bound)
				BindValue(comp, declaredCls, declSource);
		}

		void CheckFeasible(const ValueNode& n, const ValueClass* vt, const SharedStr& source)
		{
			bool hardOk = n.feasible.test(UInt32(vt->GetValueClassID()));
			for (const auto& rec : n.constraints)
				if (!rec.set.test(UInt32(vt->GetValueClassID())))
				{
					if (rec.soft)
						throwErrorF("ExprParser", "{}: {} ({}) is not among the value types supported by {} ({})"
							, FullName().c_str()
							, vt->GetName().c_str(), source.c_str()
							, rec.source.c_str(), rec.setText.c_str());
					if (!hardOk)
						throwErrorF("ExprParser", "{}: {} ({}) does not satisfy '{}: {}' ({})"
							, FullName().c_str()
							, vt->GetName().c_str(), source.c_str()
							, rec.name.GetStr().c_str(), rec.constraint.GetStr().c_str(), rec.source.c_str());
				}
			if (!hardOk)
				throwErrorF("ExprParser", "{}: {} ({}) does not satisfy the combined constraints on type variable '{}'"
					, FullName().c_str(), vt->GetName().c_str(), source.c_str(), n.name.GetStr().c_str());
		}

		// attach an operator-support set (see ConstraintRec::soft); a node already
		// bound outside the set errors immediately, an unbound node records the set
		// for its eventual binding, and `feasible` stays untouched so rigid
		// ∀-reasoning keeps using declared constraints only
		void AddSoftConstraint(SizeT i, const ValueClassSet& set, TokenID roleName, const SharedStr& source, const SharedStr& setText)
		{
			auto& n = m_ValueNodes[FindV(i)];
			if (n.bound)
			{
				if (!set.test(UInt32(n.bound->GetValueClassID())))
					throwErrorF("ExprParser", "{}: {} ({}) is not among the value types supported by {} ({})"
						, FullName().c_str()
						, n.bound->GetName().c_str(), n.boundSource.c_str()
						, source.c_str(), setText.c_str());
				return;
			}
			ConstraintRec rec;
			rec.name = roleName; rec.source = source; rec.set = set;
			rec.soft = true; rec.setText = setText;
			n.constraints.push_back(std::move(rec));
		}

		void BindValue(SizeT i, const ValueClass* vt, const SharedStr& source)
		{
			auto& n = m_ValueNodes[FindV(i)];
			if (n.rigid)
				throwErrorF("ExprParser", "{}: the body requires type variable '{}' to be {} ({}), but '{}' must remain generic in the definition"
					, FullName().c_str(), n.name.GetStr().c_str()
					, vt->GetName().c_str(), source.c_str(), n.name.GetStr().c_str());
			if (n.bound)
			{
				if (n.bound != vt)
					throwErrorF("ExprParser", "{}: inconsistent instantiation of type variable '{}': {} ({}) vs {} ({})"
						, FullName().c_str(), n.name.GetStr().c_str()
						, n.bound->GetName().c_str(), n.boundSource.c_str()
						, vt->GetName().c_str(), source.c_str());
				return;
			}
			CheckFeasible(n, vt, source);
			n.bound = vt; n.boundSource = source;
		}

		void LinkValue(SizeT a, SizeT b, const SharedStr& source)
		{
			SizeT ra = FindV(a), rb = FindV(b);
			if (ra == rb)
				return;
			if (m_ValueNodes[rb].rigid && !m_ValueNodes[ra].rigid)
				std::swap(ra, rb); // the rigid (or first) side survives as the class representative
			auto& na = m_ValueNodes[ra];
			auto& nb = m_ValueNodes[rb];
			if (na.rigid && nb.rigid)
				throwErrorF("ExprParser", "{}: the body requires type variables '{}' and '{}' to be equal ({}), but they are independent generic parameters of the definition"
					, FullName().c_str(), na.name.GetStr().c_str(), nb.name.GetStr().c_str(), source.c_str());
			if (na.rigid)
			{
				assert(!na.bound); // rigid variables never carry a concrete binding
				if (nb.bound)
					throwErrorF("ExprParser", "{}: the body requires type variable '{}' to be {} ({}), but '{}' must remain generic in the definition"
						, FullName().c_str(), na.name.GetStr().c_str()
						, nb.bound->GetName().c_str(), nb.boundSource.c_str(), na.name.GetStr().c_str());
				// FOR-ALL semantics: every instantiation allowed for the rigid variable
				// must be accepted by the other side's DECLARED constraints; soft
				// operator-support sets do not reject rigid variables (see ConstraintRec)
				if ((na.feasible & ~nb.feasible).any())
					for (const auto& rec : nb.constraints)
						if (!rec.soft && (na.feasible & ~rec.set).any())
							throwErrorF("ExprParser", "{}: type variable '{}' must satisfy '{}: {}' ({}) for every instantiation, which its declaration does not guarantee"
								, FullName().c_str(), na.name.GetStr().c_str()
								, rec.name.GetStr().c_str(), rec.constraint.GetStr().c_str(), rec.source.c_str());
			}
			if (na.bound && nb.bound && na.bound != nb.bound)
				throwErrorF("ExprParser", "{}: inconsistent instantiation of type variable '{}': {} ({}) vs {} ({})"
					, FullName().c_str(), na.name.GetStr().c_str()
					, na.bound->GetName().c_str(), na.boundSource.c_str()
					, nb.bound->GetName().c_str(), nb.boundSource.c_str());
			if (na.bound && !nb.bound)
				CheckFeasible(nb, na.bound, na.boundSource);
			if (!na.bound && nb.bound)
				CheckFeasible(na, nb.bound, nb.boundSource);
			if (!na.bound && !nb.bound && (na.feasible & nb.feasible).none())
			{
				// attribute a mutually exclusive pair when one exists
				for (const auto& recA : na.constraints)
					for (const auto& recB : nb.constraints)
						if ((recA.set & recB.set).none())
							throwErrorF("ExprParser", "{}: no value type can instantiate type variable '{}': '{}: {}' ({}) conflicts with '{}: {}' ({})"
								, FullName().c_str(), na.name.GetStr().c_str()
								, recA.name.GetStr().c_str(), recA.constraint.GetStr().c_str(), recA.source.c_str()
								, recB.name.GetStr().c_str(), recB.constraint.GetStr().c_str(), recB.source.c_str());
				throwErrorF("ExprParser", "{}: no value type satisfies the combined constraints on type variable '{}'"
					, FullName().c_str(), na.name.GetStr().c_str());
			}
			// merge rb into ra: ra keeps its (rigid/outer) name; payload and constraints unite
			if (!na.bound && nb.bound)
			{
				na.bound = nb.bound; na.boundSource = nb.boundSource;
			}
			na.feasible &= nb.feasible;
			na.constraints.insert(na.constraints.end(), nb.constraints.begin(), nb.constraints.end());
			nb.parent = ra;
		}

		// Unit-identity comparisons in this checker pass UM_AllowRightExpansion: the
		// checker always runs on the meta thread (definition/application checking
		// during meta-info construction), so UnifyDomain may intern the right
		// operand's DC too, which makes the comparison total and symmetric -- no
		// two-direction retry needed. (UM_AllowVoidRight is vestigial here: Void
		// units never reach a UnitNode -- they become Dom::Void at PositionType and
		// short-circuit in UnifyData -- but it is kept defensively.)
		static constexpr UnifyMode s_CheckerUM = UnifyMode(UM_AllowVoidRight | UM_AllowRightExpansion);

		// the batch-U invariant, confined to BindUnit/LinkUnit: binding a unit also
		// binds its companion class node to the unit's value class; linking two
		// unit nodes also links their companions. No caller ordering can then
		// desynchronize unit identity from class reasoning. The unit-side checks
		// run FIRST, so their (older, role-specific) diagnostics keep precedence.

		void BindUnit(SizeT i, SharedTreeItem keepAlive, const AbstrUnit* du, const SharedStr& source)
		{
			auto& n = m_UnitNodes[FindU(i)];
			if (n.rigid)
				throwErrorF("ExprParser", "{}: the body pins unit variable '{}' to a specific unit ({}); it must remain generic in the definition"
					, FullName().c_str(), n.name.GetStr().c_str(), source.c_str());
			if (n.bound)
			{
				if (!n.bound->UnifyDomain(du, "", "", s_CheckerUM))
					throwErrorF("ExprParser", "{}: inconsistent instantiation of unit variable '{}': the unit bound {} differs from the unit bound {}"
						, FullName().c_str(), n.name.GetStr().c_str()
						, n.boundSource.c_str(), source.c_str());
				return;
			}
			n.keepAlive = std::move(keepAlive); n.bound = du; n.boundSource = source;
			if (n.classNode != NO_TYPE_VAR)
				if (auto vt = du->GetValueType())
					BindValue(n.classNode, vt, source);
		}

		void LinkUnit(SizeT a, SizeT b, const SharedStr& source)
		{
			SizeT ra = FindU(a), rb = FindU(b);
			if (ra == rb)
				return;
			if (m_UnitNodes[rb].rigid && !m_UnitNodes[ra].rigid)
				std::swap(ra, rb);
			auto& na = m_UnitNodes[ra];
			auto& nb = m_UnitNodes[rb];
			if (na.rigid && nb.rigid)
				throwErrorF("ExprParser", "{}: the body requires unit variables '{}' and '{}' to be equal ({}), but they are independent in the definition"
					, FullName().c_str(), na.name.GetStr().c_str(), nb.name.GetStr().c_str(), source.c_str());
			if (na.rigid && nb.bound)
				throwErrorF("ExprParser", "{}: the body pins unit variable '{}' to a specific unit ({}); it must remain generic in the definition"
					, FullName().c_str(), na.name.GetStr().c_str(), nb.boundSource.c_str());
			if (na.bound && nb.bound && !na.bound->UnifyDomain(nb.bound, "", "", s_CheckerUM))
				throwErrorF("ExprParser", "{}: inconsistent instantiation of unit variable '{}': the unit bound {} differs from the unit bound {}"
					, FullName().c_str(), na.name.GetStr().c_str()
					, na.boundSource.c_str(), nb.boundSource.c_str());
			if (na.classNode != NO_TYPE_VAR && nb.classNode != NO_TYPE_VAR)
				LinkValue(na.classNode, nb.classNode, source);
			if (!na.bound && nb.bound)
			{
				na.keepAlive = nb.keepAlive; na.bound = nb.bound; na.boundSource = nb.boundSource;
			}
			nb.parent = ra;
		}
	};

	// WP4.1: enforce one 'sig<...>'-typed binding -- the bound function's positions
	// instantiate or LINK the target variables named by the type application: a
	// concrete position BINDS the mapped variable; a position naming the bound
	// function's OWN generic variable LINKS that variable (under its own fresh
	// `boundInstance`) to the mapped one. The target nodes come from callbacks so the
	// same pass serves application-time checking (targets = the applied function's
	// variables, instance 0) and the definition-time walker (targets resolved per
	// type-application argument; NO_TYPE_VAR skips a position).
	void LinkSignatureBinding(TypeUnifier& u, const TreeItem* sig, const TreeItem* boundFn,
		const std::vector<std::pair<TokenID, TokenID>>* sigVars, const std::vector<TokenID>* typeArgs,
		const std::function<SizeT(TokenID)>& targetValueNode,
		const std::function<SizeT(TokenID)>& targetUnitNode,
		UInt32 boundInstance, const SharedStr& bindSource)
	{
		std::map<TokenID, TokenID> sig2target;
		for (SizeT k = 0; k != sigVars->size(); ++k)
			sig2target[(*sigVars)[k].first] = (*typeArgs)[k];

		auto constrainPos = [&](const TreeItem* sigPos, const TreeItem* fnPos)
		{
			if (!sigPos || !fnPos || !IsDataItem(sigPos) || !IsDataItem(fnPos))
				return;

			auto itV = sig2target.find(AsDataItem(sigPos)->ValuesUnitToken());
			if (itV != sig2target.end())
				if (SizeT target = targetValueNode(itV->second); target != NO_TYPE_VAR)
				{
					TokenID fnVU = AsDataItem(fnPos)->ValuesUnitToken();
					if (fnVU && IsGenericVarOf(boundFn, fnVU))
						u.LinkValue(target, u.ValueVar(boundFn, boundInstance, fnVU, bindSource), bindSource);
					else if (auto vc = ParamValueClass(fnPos))
						u.BindValue(target, vc, bindSource);
				}

			auto itD = sig2target.find(AsDataItem(sigPos)->DomainUnitToken());
			if (itD != sig2target.end())
				if (SizeT target = targetUnitNode(itD->second); target != NO_TYPE_VAR)
				{
					TokenID fnDU = AsDataItem(fnPos)->DomainUnitToken();
					if (fnDU && IsGenericVarOf(boundFn, fnDU))
						u.LinkUnit(target, u.UnitVar(boundFn, boundInstance, fnDU), bindSource);
					else if (fnDU)
						if (auto defP = boundFn->GetTreeParent())
						{
							SharedStr fnDUName(fnDU.AsStrRange()); // materialized: a TokenStr must not span FindItem (parse-capable, token-registry lock)
							if (auto unitItem = defP->FindItem(fnDUName); unitItem && IsUnit(unitItem.get()))
								u.BindUnit(target, unitItem, AsUnit(unitItem.get())
									, mySSPrintF("by {}", bindSource.c_str()));
						}
				}
		};

		const TreeItem* sp = sig->_GetFirstSubItem();
		const TreeItem* fp = boundFn->_GetFirstSubItem();
		for (UInt32 k = 0, n = TreeItem_GetFunctionParamCount(sig); k != n && sp && fp; ++k, sp = sp->GetNextItem(), fp = fp->GetNextItem())
			constrainPos(sp, fp);
		constrainPos(
			sig->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(sig)).get(),
			boundFn->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(boundFn)).get());
	}

	void CheckFunctionSignature(const TreeItem* boundFn, const TreeItem* sigExemplar, CharPtr paramName)
	{
		UInt32 nrSigParams = TreeItem_GetFunctionParamCount(sigExemplar);
		UInt32 nrFnParams = TreeItem_GetFunctionParamCount(boundFn);
		if (nrSigParams != nrFnParams)
			throwErrorF("ExprParser", "function '{}' bound to parameter '{}' has {} parameter(s); its declared signature '{}' requires {}"
				, boundFn->GetFullName().c_str(), paramName
				, nrFnParams, sigExemplar->GetFullName().c_str(), nrSigParams);

		// a domain/values reference that names a parameter must name the positionally
		// same parameter on both sides (alpha-invariant); other references must match
		// literally
		auto normalizeUnitRef = [](const TreeItem* fn, TokenID unitRef) -> SharedStr
		{
			if (unitRef)
			{
				const TreeItem* param = fn->_GetFirstSubItem();
				for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(fn); j != n && param; ++j, param = param->GetNextItem())
					if (param->GetID() == unitRef)
						return mySSPrintF("#{}", j);
			}
			return SharedStr(unitRef);
		};

		const TreeItem* sigParam = sigExemplar->_GetFirstSubItem();
		const TreeItem* fnParam = boundFn->_GetFirstSubItem();
		for (UInt32 i = 0; i != nrSigParams; ++i, sigParam = sigParam->GetNextItem(), fnParam = fnParam->GetNextItem())
		{
			MG_CHECK(sigParam && fnParam);
			auto sigCls = sigParam->GetDynamicClass();
			if (sigCls == TreeItem::GetStaticClass())
				continue; // wildcard position
			if (fnParam->GetDynamicClass() != sigCls)
				throwErrorF("ExprParser", "function '{}' bound to parameter '{}': parameter {} is a {} but its declared signature '{}' requires a {}"
					, boundFn->GetFullName().c_str(), paramName, i + 1
					, fnParam->GetDynamicClass()->GetName().c_str()
					, sigExemplar->GetFullName().c_str()
					, sigCls->GetName().c_str());
			if (IsDataItem(sigParam))
			{
				auto sigADI = AsDataItem(sigParam);
				auto fnADI = AsDataItem(fnParam);
				if (sigADI->GetValueComposition() != fnADI->GetValueComposition())
					throwErrorF("ExprParser", "function '{}' bound to parameter '{}': parameter {} differs in value composition from its declared signature '{}'"
						, boundFn->GetFullName().c_str(), paramName, i + 1
						, sigExemplar->GetFullName().c_str());
				// a signature-side reference naming one of the signature's generic
				// variables is a wildcard position in v1 (kind-level checking, §5.10)
				if (!IsGenericVarOf(sigExemplar, sigADI->DomainUnitToken())
					&& normalizeUnitRef(sigExemplar, sigADI->DomainUnitToken()) != normalizeUnitRef(boundFn, fnADI->DomainUnitToken()))
					throwErrorF("ExprParser", "function '{}' bound to parameter '{}': the domain of parameter {} does not match the domain relationship required by signature '{}'"
						, boundFn->GetFullName().c_str(), paramName, i + 1
						, sigExemplar->GetFullName().c_str());
				if (!IsGenericVarOf(sigExemplar, sigADI->ValuesUnitToken())
					&& normalizeUnitRef(sigExemplar, sigADI->ValuesUnitToken()) != normalizeUnitRef(boundFn, fnADI->ValuesUnitToken()))
					throwErrorF("ExprParser", "function '{}' bound to parameter '{}': the values unit of parameter {} does not match signature '{}'"
						, boundFn->GetFullName().c_str(), paramName, i + 1
						, sigExemplar->GetFullName().c_str());
			}
		}

		auto sigResult = sigExemplar->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(sigExemplar));
		auto fnResult = boundFn->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(boundFn));
		if (sigResult && fnResult && sigResult->GetDynamicClass() != TreeItem::GetStaticClass()
			&& fnResult->GetDynamicClass() != sigResult->GetDynamicClass())
			throwErrorF("ExprParser", "function '{}' bound to parameter '{}': its result is a {} but the declared signature '{}' requires a {}"
				, boundFn->GetFullName().c_str(), paramName
				, fnResult->GetDynamicClass()->GetName().c_str()
				, sigExemplar->GetFullName().c_str()
				, sigResult->GetDynamicClass()->GetName().c_str());
	}

	// K11a-3: instantiation-point contract check for a structured / by-example unit
	// parameter (op-sig scope doc §3(c)). The declared member block is a CLOSED
	// interface, so each declared member must be PRESENT on the actual argument,
	// KIND-compatible, CLASS-compatible (incl. generic-constraint satisfaction and
	// cross-member consistency of a shared type variable), and -- for the relations
	// checkable at the boundary -- co-domained: a member declared over a SIBLING
	// member unit must relate to the argument's own that-unit, and a default-domain
	// member must be an attribute of the argument unit itself. Violations are
	// reported AT THE APPLICATION, naming the parameter and the member, instead of
	// transitively inside the reduced body. Deferred to the body's own reduction:
	// members over telescope parameters or generic DOMAIN variables, deep member
	// paths, members whose declared values token resolves outside the block, and
	// anything whose actual units are not resolvable here (null guards defer, never
	// misreport). memberSrc is the parameter (explicit block) or its by-example
	// exemplar; generic-variable handling applies to explicit blocks only (exemplar
	// tokens are the exemplar's lexical world).
	void CheckStructuredParamContract(const TreeItem* funcItem, const TreeItem* paramItem,
		const TreeItem* memberSrc, const TreeItem* argRoot)
	{
		assert(IsMetaThread()); // reduction runs on the meta thread; UnifyDomain below relies on it
		// K11a-4: a UNIT parameter requires a unit argument (whose identity the
		// default-domain members check against); a CONTAINER parameter accepts any
		// item carrying the members. A kind-mismatched argument fails the ordinary
		// binding diagnostics.
		bool unitParam = IsUnit(paramItem);
		if (unitParam && !IsUnit(argRoot))
			return;
		bool byExample = memberSrc != paramItem;
		SharedStr fnName(funcItem->GetFullName());
		SharedStr pName(paramItem->GetID().AsStrRange());
		constexpr UnifyMode um = UnifyMode(UM_AllowVoidRight | UM_AllowRightExpansion);

		// shared type variables: the first member's actual class binds; later members must agree
		std::map<TokenID, std::pair<const ValueClass*, SharedStr>> varBindings;

		// K11a-4: recurse into declared CONTAINER members (presence + the same
		// per-member checks under the nested block; nested blocks have no enclosing
		// unit, so default-domain membership is not claimed there)
		std::function<void(const TreeItem*, const TreeItem*, bool, const SharedStr&)> walkBlock;
		walkBlock = [&](const TreeItem* srcBlock, const TreeItem* argBlock, bool blockIsParamUnit, const SharedStr& prefix)
		{
		for (const TreeItem* m = srcBlock->_GetFirstSubItem(); m; m = m->GetNextItem())
		{
			// review finding: nested FUNCTIONS, TEMPLATES and type-alias exemplars are
			// implementation content, never a member contract (a template's internals
			// are exactly what the K11a-3 plain-template exemption already ruled out)
			if (m->IsTemplate() || m->IsFunctionItem())
				continue;
			if (!IsUnit(m) && !IsDataItem(m))
			{
				// declared container member: must be present; recurse for its members.
				// BY-EXAMPLE: the exemplar is a real config item whose sub-containers
				// are INCIDENTAL, not a declared interface -- never require them
				// (review finding: an exemplar's 'container meta { … }' made every
				// alternative argument fail).
				if (byExample || !m->_GetFirstSubItem())
					continue;
				SharedStr cName(prefix + SharedStr(m->GetID().AsStrRange()));
				auto c = argBlock->GetConstSubTreeItemByID(m->GetID());
				if (!c)
					throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: member '{}' is missing"
						, fnName.c_str(), pName.c_str(), cName.c_str());
				walkBlock(m, c.get(), false, cName + "/");
				continue;
			}
			SharedStr mName(prefix + SharedStr(m->GetID().AsStrRange()));
			auto a = argBlock->GetConstSubTreeItemByID(m->GetID());
			if (!a)
				throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: member '{}' is missing"
					, fnName.c_str(), pName.c_str(), mName.c_str());
			if (IsUnit(m))
			{
				if (!IsUnit(a.get()))
					throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' must be a unit"
						, fnName.c_str(), pName.c_str(), mName.c_str());
				auto wantCls = AsUnit(m)->GetValueType();
				auto gotCls = AsUnit(a.get())->GetValueType();
				if (wantCls && gotCls && wantCls != gotCls)
					throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: unit '{}' must be a unit<{}>, not a unit<{}>"
						, fnName.c_str(), pName.c_str(), mName.c_str()
						, wantCls->GetName().c_str(), gotCls->GetName().c_str());
				continue;
			}

			// data member
			if (!IsDataItem(a.get()))
				throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' must be an attribute"
					, fnName.c_str(), pName.c_str(), mName.c_str());
			auto declared = AsDataItem(m);
			auto actual   = AsDataItem(a.get());
			if (declared->GetValueComposition() != actual->GetValueComposition())
				throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' differs in value composition"
					, fnName.c_str(), pName.c_str(), mName.c_str());

			auto avu = actual->GetAbstrValuesUnit();
			if (TokenID vt = declared->ValuesUnitToken())
			{
				// ladder order mirrors BuildParamMembers (review finding: sibling and
				// generic-variable rungs BEFORE the ValueClass vocabulary, so a type
				// variable named like a value class stays the variable here too)
				bool isSibling = false;
				for (const TreeItem* u = srcBlock->_GetFirstSubItem(); u; u = u->GetNextItem())
					if (u->GetID() == vt && IsUnit(u))
					{
						isSibling = true;
						break;
					}
				if (isSibling)
				{
					// a sibling MEMBER UNIT: the actual member must relate to the
					// argument's own that-unit (the K2 identity, at the boundary)
					auto aSib = argBlock->GetConstSubTreeItemByID(vt);
					if (avu && aSib && IsUnit(aSib.get())
						&& !avu->UnifyDomain(AsUnit(aSib.get()), "", "", um))
					{
						SharedStr vtName(vt.AsStrRange());
						throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: the values of '{}' must be '{}' (the argument's own member unit)"
							, fnName.c_str(), pName.c_str(), mName.c_str(), vtName.c_str());
					}
				}
				else if (!byExample && (IsOwnDeclaredVar(funcItem, vt) || IsGenericVarOf(funcItem, vt)))
				{
					// generic value variable (own <...> clause OR positional generic --
					// same pair BuildParamMembers tests): satisfy the declared
					// constraint and stay consistent with any earlier member sharing
					// the variable
					auto gotCls = avu ? avu->GetValueType() : nullptr;
					if (gotCls)
					{
						if (TokenID cons = DeclaredConstraintOf(funcItem, vt))
							if (!GenericConstraintSet(cons).test(UInt32(gotCls->GetValueClassID())))
							{
								SharedStr vtName(vt.AsStrRange()), consName(cons.AsStrRange());
								throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' is an attribute<{}>, which does not satisfy '{}: {}'"
									, fnName.c_str(), pName.c_str(), mName.c_str()
									, gotCls->GetName().c_str(), vtName.c_str(), consName.c_str());
							}
						auto [it, isNew] = varBindings.try_emplace(vt, gotCls, mName);
						if (!isNew && it->second.first != gotCls)
						{
							SharedStr vtName(vt.AsStrRange());
							throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' ({}) and '{}' ({}) must share one value type for '{}'"
								, fnName.c_str(), pName.c_str()
								, it->second.second.c_str(), it->second.first->GetName().c_str()
								, mName.c_str(), gotCls->GetName().c_str(), vtName.c_str());
						}
					}
				}
				else if (auto wantCls = ValueClass::FindByScriptName(vt))
				{
					auto gotCls = avu ? avu->GetValueType() : nullptr;
					if (gotCls && gotCls != wantCls)
						throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' must be an attribute<{}>, not an attribute<{}>"
							, fnName.c_str(), pName.c_str(), mName.c_str()
							, wantCls->GetName().c_str(), gotCls->GetName().c_str());
				}
				// else: resolves outside the block / telescope / unknown -- defer
			}

			// default-domain members are attributes OF the argument unit (claimed
			// ONLY at a unit parameter's TOP block -- containers/nested blocks have
			// no member-domain default); a VOID actual domain broadcasts (the
			// language's single implicit coercion -- review finding: a parameter<>
			// member must not be rejected); explicit non-default domains (telescope
			// params, generic domain vars, scope units) are checked transitively by
			// the body's reduction
			TokenID dt = declared->DomainUnitToken();
			bool defaultDomain = !dt || dt == t_Dot || dt == srcBlock->GetID()
				|| (!byExample && srcBlock == memberSrc && dt == paramItem->GetID());
			if (defaultDomain && blockIsParamUnit)
			{
				auto adu = actual->GetAbstrDomainUnit();
				if (adu
					&& adu->GetValueType()->GetValueClassID() != ValueClassID::VT_Void
					&& !adu->UnifyDomain(AsUnit(argRoot), "", "", um))
					throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' must be an attribute of the argument unit itself"
						, fnName.c_str(), pName.c_str(), mName.c_str());
			}
		}
		};
		walkBlock(memberSrc, argRoot, unitParam, SharedStr());
	}

	// K11 leftover (2026-07-29): the INSTANTIATE path bypasses ReduceValue's binding
	// loop (it is a CopyTreeContext tree copy), so the K11a-3 contract check never
	// ran there -- a missing member surfaced as a transitive 'Unknown identifier
	// nw/F2' inside the copied body instead of the boundary message. This helper
	// applies the same check from MetaFuncCurry; expression arguments and
	// non-function apply-items (plain templates) defer, as at the inline site.
	//
	// Review finding (reproduced both ways, fixed): arguments must resolve from the
	// TARGET's parent -- the context the copied parameter's ArgCalc calculator will
	// bind in (param.parent.parent = target.parent) -- NOT via ac->FindItem (the
	// calculator's search context). The two coincide for a Calculator-role holder,
	// but when the 'instantiate f(...)' expression sits on a copied TEMPLATE
	// ARGUMENT (ArgCalc role) inside a template whose LOCAL shadows the call-site
	// name, ac resolved the call-site item while the copy binds the template-local
	// one: the checker validated the WRONG item (a false boundary rejection of a
	// previously-working config, and a false pass of a broken one).
	void CheckStructuredParamContracts(const TreeItem* applyItem, LispPtr argList, const TreeItem* target)
	{
		if (!applyItem || !applyItem->IsFunctionItem() || !target)
			return;
		auto bindScope = target->GetTreeParent();
		if (!bindScope)
			return;
		UInt32 nrParams = TreeItem_GetFunctionParamCount(applyItem);
		const TreeItem* param = applyItem->_GetFirstSubItem();
		LispPtr a = argList;
		for (UInt32 i = 0; i != nrParams && param && !a.EndP(); ++i, param = param->GetNextItem(), a = a.Right())
		{
			if (IsDataItem(param) || param->IsFunctionItem())
				continue;
			if (!(IsUnit(param) || !IsDataItem(param))) // unit or container parameters only
				continue;
			const TreeItem* memberSrc = param->_GetFirstSubItem() ? param : nullptr;
			SharedTreeItem exKeep;
			if (!memberSrc)
				if (auto ex = TreeItem_GetFunctionParamTypeExemplar(applyItem, i); ex && ex->_GetFirstSubItem())
				{
					exKeep = ex;
					memberSrc = exKeep.get();
				}
			if (!memberSrc)
				continue;
			LispPtr ae = a.Left();
			if (!ae.IsSymb())
				continue; // expression argument: defers, as at the inline site
			auto argItem = bindScope->FindItem(SharedStr(ae.GetSymbID().AsStrRange()));
			if (!argItem)
				continue; // an unresolvable argument fails through the ordinary path
			CheckStructuredParamContract(applyItem, param, memberSrc, argItem.get());
		}
	}

	LispRef FunctionApplication::Reduce()
	{
		CallArg r = ReduceValue();
		if (r.binding)
			throwErrorF("ExprParser", "'{}': a function value can only be applied with '(...)', passed as an argument, or returned as a result"
				, m_FuncItem->GetFullName().c_str());
		return r.key;
	}

	CallArg FunctionApplication::ReduceValue()
	{
		assert(m_FuncItem && m_FuncItem->IsTemplate()); // functions are IsTemplate too
		assert(m_ErrorHolder);

		for (const FunctionApplication* ancestor = m_Parent; ancestor; ancestor = ancestor->m_Parent)
			if (ancestor->m_FuncItem == m_FuncItem && ancestor->m_Env == m_Env
				// same function AND same closure environment: §5.10 allows distinct
				// closures of one nested function within a single reduction chain.
				// '...x' rest folds recurse with STRICTLY FEWER arguments -- well-founded
				// on the parent chain, so permitted; equal-or-more args = true recursion
				&& m_ArgKeys.size() >= ancestor->m_ArgKeys.size())
				throwErrorF("ExprParser", "'{}': recursive function application is not supported"
					, m_FuncItem->GetFullName().c_str());

		// §5.9 'apply T(args)': a plain template applied as an ad-hoc function -- its params
		// are its first N sub-items with N = the number of provided arguments (the template
		// binding rule), and its designated result is the CI-unique 'result' sub-item
		bool isPlainTemplate = !m_FuncItem->IsFunctionItem();

		UInt32 nrParams = isPlainTemplate ? m_ArgKeys.size() : TreeItem_GetFunctionParamCount(m_FuncItem);
		bool hasRest = !isPlainTemplate && TreeItem_HasFunctionRestParam(m_FuncItem);
		if (hasRest ? m_ArgKeys.size() < nrParams : m_ArgKeys.size() != nrParams)
			throwErrorF("ExprParser", "'{}': function expects {}{} argument(s); {} provided"
				, m_FuncItem->GetFullName().c_str(), nrParams, hasRest ? " or more" : "", m_ArgKeys.size());
		if (isPlainTemplate)
		{
			UInt32 nrChildren = 0;
			for (const TreeItem* c = m_FuncItem->_GetFirstSubItem(); c; c = c->GetNextItem())
				++nrChildren;
			if (nrChildren < nrParams)
				throwErrorF("ExprParser", "'apply' on template '{}': {} argument(s) provided but the template has only {} sub-item(s)"
					, m_FuncItem->GetFullName().c_str(), nrParams, nrChildren);
		}
		else
			// tranche 3: definition-time check at every application entry (once per
			// function) -- uniformly covers closures, prelude functions and variant
			// members, which do not all pass through the direct-call substitution site
			CheckFunctionDefinition(m_FuncItem);

		// WP4.1: signature-instantiation constraints collected from 'sig<V, D>'-typed
		// parameters, merged into the type/domain variable bindings after the data
		// arguments have been processed
		struct SigConstraint
		{
			UInt32 paramIndex;
			SharedTreeItem sig, boundFn;
			const std::vector<std::pair<TokenID, TokenID>>* sigVars;
			const std::vector<TokenID>* typeArgs;
		};
		std::vector<SigConstraint> sigConstraints;

		m_Params.clear(); m_Params.reserve(nrParams);
		const TreeItem* child = m_FuncItem->_GetFirstSubItem();
		for (UInt32 i = 0; i != nrParams; ++i, child = child->GetNextItem())
		{
			MG_CHECK(child); // guaranteed by the parser: params are the first nrParams sub-items
			m_Params.push_back(child);

			// '...x' rest parameter (always last): binds the argument TAIL m_ArgKeys[i..end),
			// spliced where the body passes it as a trailing call argument -- never a scalar
			if (hasRest && i == nrParams - 1)
			{
				m_RestParam = child;
				continue;
			}

			// meta-reference parameter ('item x'): bind the RAW item reference (the same
			// sourceDescr form PropValue's subst_never argument gets in a direct call),
			// never the argument's calculation/range key -- so PropValue & co read the
			// CONFIG item's metadata, and the reduced key equals the direct call's key
			if (TreeItem_IsFunctionMetaRefParam(m_FuncItem, i))
			{
				if (!m_ArgItems[i])
					throwErrorF("ExprParser", "'{}': parameter '{}' is an item (meta-reference) parameter; its argument must be a reference to a config item, not a calculated expression"
						, m_FuncItem->GetFullName().c_str()
						, child->GetID().GetStr().c_str());
				m_ArgKeys[i] = CreateLispTree(m_ArgItems[i].get(), false);
			}
			m_Reductions[child] = m_ArgKeys[i];

			if (auto declaredSig = TreeItem_GetFunctionParamSignature(m_FuncItem, i))
			{
				if (!m_ArgBindings[i])
					throwErrorF("ExprParser", "'{}': parameter '{}' requires a function argument matching signature '{}'"
						, m_FuncItem->GetFullName().c_str()
						, child->GetID().GetStr().c_str()
						, declaredSig->GetFullName().c_str());
				// a partial application's residual arity must match; the full structural
				// check applies only to plain (all-holes) function references (WP3.1 v1)
				UInt32 residualArity = m_ArgBindings[i]->NrHoles();
				UInt32 requiredArity = TreeItem_GetFunctionParamCount(declaredSig.get());
				if (residualArity == TreeItem_GetFunctionParamCount(m_ArgBindings[i]->funcItem.get()))
					CheckFunctionSignature(m_ArgBindings[i]->funcItem.get(), declaredSig.get(), child->GetID().GetStr().c_str());
				else if (residualArity != requiredArity)
					throwErrorF("ExprParser", "'{}': partial application bound to parameter '{}' has {} remaining argument(s); signature '{}' requires {}"
						, m_FuncItem->GetFullName().c_str(), child->GetID().GetStr().c_str()
						, residualArity, declaredSig->GetFullName().c_str(), requiredArity);

				// WP4.1: enforce the type application 'sig<V, D>' -- the bound function's
				// CONCRETE positions constrain this application's type variables, shared
				// with (and checked against) the data-argument bindings below
				if (auto sigTypeArgs = TreeItem_GetFunctionParamSigTypeArgs(m_FuncItem, i))
					if (auto sigVars = TreeItem_GetFunctionTypeVars(declaredSig.get()); sigVars && sigTypeArgs->size() == sigVars->size())
						sigConstraints.push_back({ i, declaredSig, m_ArgBindings[i]->funcItem, sigVars, sigTypeArgs });
			}

			// K11a-3: a structured / by-example unit parameter carries a declared
			// member interface -- validate the ACTUAL argument against it here, at the
			// call boundary (clear attribution), instead of deep inside the reduced
			// body. The argument's CONFIG item (m_ArgItems -- the same reference the
			// body's member access binds to) carries the members; an expression
			// argument has none here and defers to the body's own resolution.
			// FUNCTION items only (review finding): a plain template's unit-parameter
			// sub-items (helper locals with calculation rules) are NOT a declared
			// member contract -- 'apply' on such templates must keep working.
			// K11a-4: CONTAINER parameters (plain non-data, non-function items with a
			// member block or exemplar) carry the same declared interface.
			if (!isPlainTemplate && m_ArgItems[i]
				&& (IsUnit(child) || (!IsDataItem(child) && !child->IsFunctionItem())))
			{
				const TreeItem* memberSrc = child->_GetFirstSubItem() ? child : nullptr;
				SharedTreeItem exKeep;
				if (!memberSrc)
					if (auto ex = TreeItem_GetFunctionParamTypeExemplar(m_FuncItem, i); ex && ex->_GetFirstSubItem())
					{
						exKeep = ex;
						memberSrc = exKeep.get();
					}
				if (memberSrc)
					CheckStructuredParamContract(m_FuncItem, child, memberSrc, m_ArgItems[i].get());
			}
		}

		// generic type variables: bind each variable from the actual arguments' value
		// classes / domain units into the unification store (WP4.1 tranche 2), which
		// also receives variable-variable links from the signature-typed parameters
		// below -- consistency and constraint satisfaction are checked per equivalence
		// class, with attribution
		TypeUnifier unifier{ m_FuncItem };
		SharedStr declSource = mySSPrintF("declared by function '{}'", m_FuncItem->GetFullName().c_str());
		UInt32 seqNr = 0, gpIndex; TokenID gpVar, gpConstraint; bool gpIsDomain;
		while (TreeItem_GetFunctionGenericParam(m_FuncItem, seqNr++, &gpIndex, &gpVar, &gpConstraint, &gpIsDomain))
		{
			MG_CHECK(gpIndex < nrParams);
			const TreeItem* gpParam = m_Params[gpIndex];
			if (m_ArgKeys[gpIndex].EndP())
				throwErrorF("ExprParser", "'{}': parameter '{}' requires an attribute or unit argument"
					, m_FuncItem->GetFullName().c_str(), gpParam->GetID().GetStr().c_str());

			auto dc = GetOrCreateDataController(m_ArgKeys[gpIndex]);
			auto argResult = dc->MakeResult();
			if (!argResult)
			{
				dms_assert(dc->WasFailed(FailType::MetaInfo));
				m_ErrorHolder->ThrowFail(dc.get());
			}

			if (gpIsDomain)
			{
				// bind the domain variable from the argument's domain unit; a void
				// domain broadcasts into any D (the language's single implicit coercion)
				const AbstrUnit* du = IsDataItem(argResult.get()) ? AsDataItem(argResult.get())->GetAbstrDomainUnit() : nullptr;
				if (!du)
					throwErrorF("ExprParser", "'{}': parameter '{}' requires an attribute argument (its domain binds '{}')"
						, m_FuncItem->GetFullName().c_str(), gpParam->GetID().GetStr().c_str(), gpVar.GetStr().c_str());
				if (du->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
				{ /* void broadcasts into any D and does not constrain it */ }
				else
					unifier.BindUnit(unifier.UnitVar(m_FuncItem, 0, gpVar), argResult, du
						, mySSPrintF("via parameter '{}'", gpParam->GetID().GetStr().c_str()));
				continue;
			}
			const ValueClass* vt = nullptr;
			if (IsDataItem(argResult.get()))
				vt = AsDataItem(argResult.get())->GetAbstrValuesUnit()->GetValueType();
			else if (IsUnit(argResult.get()))
				vt = AsUnit(argResult.get())->GetValueType();
			if (!vt)
				throwErrorF("ExprParser", "'{}': parameter '{}' requires an attribute or unit argument"
					, m_FuncItem->GetFullName().c_str(), gpParam->GetID().GetStr().c_str());
			unifier.BindValue(unifier.ValueVar(m_FuncItem, 0, gpVar, declSource), vt
				, mySSPrintF("parameter '{}'", gpParam->GetID().GetStr().c_str()));
		}

		// WP4.1: merge signature-instantiation constraints -- for each 'sig<V, D>'-typed
		// parameter, the bound function's positions instantiate or LINK the applied
		// variables (LinkSignatureBinding). Each binding gets its OWN instance of the
		// bound function's variables: two independent bindings of the same generic
		// function must not link through a shared node (tranche 3 fix).
		UInt32 bindingInstance = 0;
		for (const auto& sc : sigConstraints)
		{
			const TreeItem* viaParam = m_Params[sc.paramIndex];
			SharedStr sigSource = mySSPrintF("function '{}' bound to parameter '{}'"
				, sc.boundFn->GetFullName().c_str(), viaParam->GetID().GetStr().c_str());
			LinkSignatureBinding(unifier, sc.sig.get(), sc.boundFn.get(), sc.sigVars, sc.typeArgs
				, [&](TokenID t) { return unifier.ValueVar(m_FuncItem, 0, t, declSource); }
				, [&](TokenID t) { return unifier.UnitVar(m_FuncItem, 0, t); }
				, ++bindingInstance, sigSource);
		}

		SharedTreeItem resultChild;
		if (isPlainTemplate)
		{
			// CI-unique 'result' sub-item designates the value of an applied template
			for (const TreeItem* c = m_FuncItem->_GetFirstSubItem(); c; c = c->GetNextItem())
				if (!stricmp(c->GetID().GetStr().c_str(), "result"))
				{
					if (resultChild)
						throwErrorF("ExprParser", "'apply' on template '{}': multiple sub-items named 'result'"
							, m_FuncItem->GetFullName().c_str());
					resultChild = make_shared_tree(c, existing_obj{});
				}
			if (!resultChild)
				throwErrorF("ExprParser", "'apply' on template '{}': no 'result' sub-item to take as the value; use 'instantiate {}(…)' for the steps"
					, m_FuncItem->GetFullName().c_str(), m_FuncItem->GetID().GetStr().c_str());
			if (resultChild->GetExpr().empty())
				throwErrorF("ExprParser", "'apply' on template '{}': the 'result' sub-item has no calculation rule"
					, m_FuncItem->GetFullName().c_str());
		}
		else
		{
			TokenID resultName = TreeItem_GetFunctionResultName(m_FuncItem);
			resultChild = m_FuncItem->GetConstSubTreeItemByID(resultName);
			if (!resultChild)
				throwErrorF("ExprParser", "'{}': designated result '{}' not found"
					, m_FuncItem->GetFullName().c_str(), resultName.GetStr().c_str());
		}

		// §5.10: a function-typed result yields a closure -- the nested function plus
		// this application's bound parameters, captured by value
		if (resultChild->IsFunctionItem())
		{
			auto env = MakeCurrentEnv();

			CallArg r;
			r.binding = std::make_shared<FunctionBinding>();
			r.binding->funcItem = resultChild;
			r.binding->slots.resize(TreeItem_GetFunctionParamCount(resultChild.get()));
			for (auto& s : r.binding->slots)
				s.isHole = true;
			r.binding->env = std::move(env);
			return r;
		}

		if (resultChild->GetExpr().empty())
			throwErrorF("ExprParser", "'{}' is a function signature without implementation and cannot be applied"
				, m_FuncItem->GetFullName().c_str());

		CallArg r;
		r.key = ReduceBodyItem(resultChild.get());
		return r;
	}

	LispRef FunctionApplication::ReduceBodyItem(const TreeItem* bodyItem)
	{
		auto memo = m_Reductions.find(bodyItem);
		if (memo != m_Reductions.end())
			return memo->second;

		if (!m_InProgress.insert(bodyItem).second)
			throwErrorF("ExprParser", "'{}': circular reference in function body"
				, bodyItem->GetFullName().c_str());

		LispRef result;
		SharedStr exprStr = bodyItem->GetExpr();
		if (exprStr.empty())
		{
			if (IsUnit(bodyItem))
				result = bodyItem->GetCheckedKeyExpr(); // local base unit: nominal identity, shared by all applications
			else
				throwErrorF("ExprParser", "'{}': local item without calculation rule cannot be used in an inlined function application"
					, bodyItem->GetFullName().c_str());
		}
		else
		{
			if (AbstrCalculator::MustEvaluate(exprStr.c_str()))
				throwErrorF("ExprParser", "'{}': leading-'=' string indirection is not supported inside function bodies"
					, bodyItem->GetFullName().c_str());
			auto bodyCalc = AbstrCalculator::ConstructFromStr(bodyItem, exprStr, CalcRole::Calculator);
			auto refScope = bodyItem->GetTreeParent(); // names resolve from the referencing item's own scope outward, as in instantiated form
			MG_CHECK(refScope);
			result = SubstituteBodyExpr(refScope.get(), RewriteExpr(bodyCalc->GetLispExprOrg()));
		}

		m_InProgress.erase(bodyItem);
		m_Reductions[bodyItem] = result;
		return result;
	}

	LispRef FunctionApplication::SubstituteBodyExpr(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.EndP())
			return expr;

		if (expr.IsRealList())
		{
			MG_CHECK(expr.Left().IsSymb());
			LispRef head = expr.Left();
			TokenID headID = head.GetSymbID();

			if (headID == token::sourceDescr)
				return ResolveBodySymbol(refScope, expr.Right().Left().GetSymbID(), nullptr);

			if (headID == token::arrow || headID == token::scope || headID == token::subitem)
				throwErrorF("ExprParser", "the '{}' construct is not yet supported inside inlined function bodies"
					"; bind the function application to a container to use the instantiating form"
					, headID.GetStr().c_str());

			// §5.10 applied call result in a data position: must reduce all the way to data
			if (headID == t_ApplyValue)
			{
				CallArg r = ResolveBodyArg(refScope, expr);
				if (r.binding)
					throwErrorF("ExprParser", "a function value can only be applied with '(...)', passed as an argument, or returned as a result");
				return r.key;
			}

			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			assert(og);
			// arity-aware head dispatch: an argument count no operator member accepts may
			// be served by a same-named function (prelude folds, log(x,base), ...).
			// The count is the EFFECTIVE arity: a trailing '...x' rest symbol expands to
			// its captured argument count -- this is what lets a fold body's recursive
			// call resolve to the binary OPERATOR on the last step (rest = 1 element)
			// and back to the function while more remain
			bool arityFallback = false;
			if (!og->IsTemplateCall())
			{
				UInt32 nrCallArgs = 0;
				for (LispPtr argPtr = expr.Right(); argPtr.IsRealList(); argPtr = argPtr.Right())
				{
					LispPtr a = argPtr.Left();
					if (a.IsSymb() && IsRestParamSymbol(a.GetSymbID()) && argPtr.Right().EndP())
					{
						nrCallArgs += m_ArgKeys.size() - (TreeItem_GetFunctionParamCount(m_FuncItem) - 1);
						break;
					}
					++nrCallArgs;
				}
				arityFallback = !og->AcceptsArity(nrCallArgs);
			}
			if (og->IsTemplateCall() || arityFallback)
			{
				// a function application in a data (body-expression) position: resolve the
				// head to a function value (a function-valued parameter's binding, or a
				// plain import), fill its holes with the call arguments, and reduce; a
				// residual (partially applied) result cannot stand in a data position.
				std::shared_ptr<FunctionBinding> paramBinding;
				auto headFn = ResolveBodyHeadFunction(refScope, headID, &paramBinding, /*mayFail*/ arityFallback);
				if (headFn)
				{
					std::vector<CallArg> holeFills;
					for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
					{
						LispPtr a = argPtr.Left();
						if (a.IsSymb() && IsRestParamSymbol(a.GetSymbID()))
						{
							if (!argPtr.Right().EndP())
								throwErrorF("ExprParser", "'{}': rest parameter '{}' must be the trailing argument of the call"
									, m_FuncItem->GetFullName().c_str(), a.GetSymbStr().c_str());
							SpliceRestArgs(holeFills); // '...x' passed on: splice the captured tail
							break;
						}
						holeFills.push_back(ResolveBodyArg(refScope, a));
					}

					// §5.7: a variant set called from a body dispatches by argument type, exactly
					// like the direct-call site (also reached by '...x' recursive fold steps)
					if (!paramBinding && TreeItem_IsFunctionVariantSet(headFn.get()))
					{
						auto variant = ResolveVariant(headFn.get(), holeFills, m_ErrorHolder);
						headFn = make_shared_tree(variant, existing_obj{});
						if (m_SubstBuff) registerSupplier(*m_SubstBuff, variant);
						CheckFunctionDefinition(variant);
					}
					FunctionBinding calleeBinding = paramBinding ? *paramBinding : *MakeAllHoles(headFn);
					if (!paramBinding && !calleeBinding.env && IsNestedInside(headFn.get(), m_FuncItem))
						calleeBinding.env = MakeCurrentEnv(); // #1166: nested callee sees the enclosing parameters

					FunctionBinding merged = MergeBinding(calleeBinding, holeFills);
					if (merged.NrHoles() != 0)
						throwErrorF("ExprParser", "'{}': a partial application can only be passed as an argument, not used as a value"
							, headID.GetStr().c_str());
					return ReduceMerged(merged, this, m_SubstBuff, m_ErrorHolder);
				}
				// arity-fallback probe found no function: fall through to the operator path,
				// whose FindOper reports the arity error
			}
			if (!og->MustCacheResult())
				throwErrorF("ExprParser", "'{}': meta function call is not supported inside function bodies"
					, headID.GetStr().c_str());

			// ordinary operator application: substitute the arguments
			std::vector<LispRef> substArgs;
			for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
			{
				LispPtr a = argPtr.Left();
				if (a.IsSymb() && IsRestParamSymbol(a.GetSymbID()))
				{
					// trailing '...x' into an OPERATOR call: splice the captured argument
					// keys (a fold body's last recursive step lands here: rest = 1 element
					// -> the binary operator)
					if (!argPtr.Right().EndP())
						throwErrorF("ExprParser", "'{}': rest parameter '{}' must be the trailing argument of the call"
							, m_FuncItem->GetFullName().c_str(), a.GetSymbStr().c_str());
					for (UInt32 k = TreeItem_GetFunctionParamCount(m_FuncItem) - 1; k != m_ArgKeys.size(); ++k)
					{
						if (m_ArgBindings[k] || m_ArgLiterals[k])
							throwErrorF("ExprParser", "'{}': a function value or container literal in '...{}' cannot be passed to operator '{}'"
								, m_FuncItem->GetFullName().c_str(), a.GetSymbStr().c_str(), headID.GetStr().c_str());
						substArgs.push_back(m_ArgKeys[k]);
					}
					break;
				}
				substArgs.push_back(SubstituteBodyExpr(refScope, a));
			}

			LispRef argList;
			for (auto ri = substArgs.rbegin(); ri != substArgs.rend(); ++ri)
				argList = LispRef(*ri, argList);

			LispRef result = RewriteExprTop(LispRef(head, std::move(argList)));

			if (og->CanResultToConfigItem())
			{
				DataControllerRef dc = GetOrCreateDataController(result);
				auto supplier = dc->MakeResult();
				if (!supplier)
				{
					dms_assert(dc->WasFailed(FailType::MetaInfo));
					m_ErrorHolder->ThrowFail(dc.get());
				}
				if (!supplier->IsCacheItem())
					result = supplier->GetCheckedKeyExpr();
			}
			return result;
		}

		if (expr.IsSymb())
		{
			TokenID symbID = expr.GetSymbID();
			if (token::isConst(symbID))
				return ExprList(symbID);
			if (ValueClass::FindByScriptName(symbID))
				return List(LispRef(expr)); // unitName -> [UnitName []], i.e. unitName()
			return ResolveBodySymbol(refScope, symbID, nullptr);
		}

		return expr; // numeric, string and UInt64 literals
	}

	// §5.10: look a name up in the captured closure environment(s): the enclosing
	// applications' parameters, nearest enclosure first. Returns true when bound.
	bool FunctionApplication::ResolveEnvSymbol(TokenID symbID, SharedTreeItem* foundItemPtr, LispRef* keyPtr, std::shared_ptr<FunctionBinding>* bindingPtr)
	{
		for (auto env = m_Env; env; env = env->next)
		{
			UInt32 i = 0;
			for (const TreeItem* c = env->funcItem->_GetFirstSubItem(); c && i < env->args.size(); c = c->GetNextItem(), ++i)
				if (c->GetID() == symbID)
				{
					const CallArg& a = env->args[i];
					if (foundItemPtr) *foundItemPtr = a.item;
					if (keyPtr)       *keyPtr = a.key;
					if (bindingPtr)   *bindingPtr = a.binding;
					return true;
				}
		}
		return false;
	}

	LispRef FunctionApplication::ResolveBodySymbol(const TreeItem* refScope, TokenID symbID, SharedTreeItem* foundItemPtr)
	{
		SharedStr fullStr(symbID.AsStrRange());
		CharPtr b = fullStr.begin(), e = fullStr.send();

		if (b != e && (*b == '.' || *b == '/'))
			throwErrorF("ExprParser", "'{}': dot-relative and absolute references are not supported inside function bodies"
				, fullStr.c_str());

		CharPtr slash = std::find(b, e, '/');
		TokenID firstTok = (slash == e) ? symbID : GetTokenID_mt(b, slash);

		// nearest-scope resolution: from the referencing item's scope outward, up to and
		// including the function item -- matching the resolution order of the instantiated form
		for (const TreeItem* scope = refScope; scope; scope = scope->GetTreeParent().get())
		{
			bool atFuncRoot = (scope == m_FuncItem);
			auto child = scope->GetConstSubTreeItemByID(firstTok);
			if (child)
			{
				// parameter?
				for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
					if (m_Params[i] == child.get())
					{
						if (child.get() == m_RestParam)
							throwErrorF("ExprParser", "'{}': parameter '{}' is a '...' rest parameter; it can only be passed on as the trailing argument of a function call"
								, m_FuncItem->GetFullName().c_str(), child->GetID().GetStr().c_str());
						// §5.9 parameter bound to a container literal: reduce a bare use to the
						// domain and 'param/member' to the named member value -- no arg item exists
						if (m_ArgLiterals[i])
						{
							const auto& lit = *m_ArgLiterals[i];
							if (foundItemPtr)
								*foundItemPtr = nullptr;
							if (slash == e)
							{
								if (!lit.hasDomain)
									throwErrorF("ExprParser", "'{}': the container literal bound to parameter '{}' has no domain unit and cannot be used as a unit"
										, fullStr.c_str(), firstTok.GetStr().c_str());
								return lit.domainKey;
							}
							TokenID memberName = GetTokenID_mt(slash + 1, e);
							for (const auto& mv : lit.members)
								if (mv.first == memberName)
									return mv.second;
							throwErrorF("ExprParser", "'{}': the container literal bound to parameter '{}' has no member '{}'"
								, fullStr.c_str(), firstTok.GetStr().c_str(), SharedStr(CharPtrRange(slash + 1, e)).c_str());
						}

						bool boundToFunction = (m_ArgBindings[i] != nullptr);
						if (slash == e)
						{
							if (boundToFunction)
								throwErrorF("ExprParser", "'{}': a function-valued parameter can only be applied or passed on as an argument"
									, fullStr.c_str());
							if (foundItemPtr)
								*foundItemPtr = m_ArgItems[i];
							return m_ArgKeys[i];
						}
						// member access through a (structured or composite-typed) parameter:
						// descend into the actual argument only (never its ancestors)
						auto argItem = m_ArgItems[i];
						if (!argItem)
							throwErrorF("ExprParser", "'{}': member access through parameter '{}' requires the corresponding argument to be a direct item reference"
								, fullStr.c_str(), firstTok.GetStr().c_str());
						if (boundToFunction)
							throwErrorF("ExprParser", "'{}': member access through a function-valued parameter is not supported"
								, fullStr.c_str());
						auto member = FindSubItem(argItem.get(), SharedStr(CharPtrRange(slash + 1, e)));
						if (!member)
							throwErrorF("ExprParser", "'{}': the argument '{}' bound to parameter '{}' has no member '{}'"
								, fullStr.c_str(), argItem->GetFullName().c_str(), firstTok.GetStr().c_str()
								, SharedStr(CharPtrRange(slash + 1, e)).c_str());
						member->UpdateMetaInfo();
						if (m_SubstBuff)
							registerSupplier(*m_SubstBuff, member.get());
						if (foundItemPtr)
							*foundItemPtr = member;
						return member->GetCheckedKeyExpr();
					}

				// local body item (possibly a nested path into it)
				SharedTreeItem target = child;
				if (slash != e)
				{
					// §12.7 slSubItemCall tranche: descend the DECLARED structure
					// segment-wise; a miss below an item WITH a calculation rule
					// resolves INTO its computed result -- slSubItemCall(reducedKey,
					// rest), the cache layer's canonical sub-item form (u/Values on
					// u := unique(x) & co.). SubItemOperator reports a missing
					// member per application; meta rules keep their own rejection
					// (ReduceBodyItem throws it). The deepest rule-bearing item on
					// the walked path wins (its members are keyed from itself);
					// rule-less misses keep the FindSubItem report exactly as before.
					CharPtr segBegin = slash + 1;
					std::vector<std::pair<SharedTreeItem, CharPtr>> descended;
					descended.emplace_back(target, segBegin);
					while (segBegin != e)
					{
						CharPtr segEnd = std::find(segBegin, e, DELIMITER_CHAR);
						auto sub = target->GetConstSubTreeItemByID(GetTokenID_mt(segBegin, segEnd));
						if (!sub)
						{
							for (auto ri = descended.rbegin(); ri != descended.rend(); ++ri)
								if (!ri->first->GetExpr().empty())
								{
									LispRef baseKey = ReduceBodyItem(ri->first.get());
									// A CONFIG-item reference (a sourceDescr key: the
									// local's rule was a bare import/def-scope/param alias
									// to a config item) is NOT a cache result, so the
									// cache-layer SubItemOperator cannot take it as a base.
									// But the member is directly resolvable: descend the
									// remaining path against the referenced config item and
									// emit that item's own key -- exactly as member access
									// through a structured parameter does (see above) --
									// instead of routing through the cache layer.
									if (baseKey.IsRealList() && baseKey.Left().IsSymb()
										&& baseKey.Left().GetSymbID() == token::sourceDescr)
									{
										LispPtr fullNameRef = baseKey.Right().Left();
										if (fullNameRef.IsSymb())
										{
											// materialize the name first: a TokenStr range holds the
											// token-registry lock, which FindItem (parse-capable) must not span
											SharedStr baseName(fullNameRef.GetSymbID().AsStrRange());
											if (auto baseItem = m_FuncItem->FindItem(baseName))
											{
												// FindSubItem throws a clean FindSubItem error on a genuinely missing member
												auto member = FindSubItem(baseItem.get(), SharedStr(CharPtrRange(ri->second, e)));
												member->UpdateMetaInfo();
												if (m_SubstBuff)
													registerSupplier(*m_SubstBuff, member.get());
												if (foundItemPtr)
													*foundItemPtr = member;
												return member->GetCheckedKeyExpr();
											}
										}
										break; // config base could not be resolved -> keep the pre-tranche throw
									}
									if (foundItemPtr)
										*foundItemPtr = nullptr;
									return slSubItemCall(std::move(baseKey), CharPtrRange(ri->second, e));
								}
							throwErrorF("FindSubItem", "Cannot find {} from {}"
								, SharedStr(CharPtrRange(segBegin, segEnd)), target->GetFullName().c_str());
						}
						target = sub;
						segBegin = (segEnd == e) ? e : segEnd + 1;
						descended.emplace_back(target, segBegin);
					}
				}
				if (foundItemPtr)
					*foundItemPtr = nullptr; // reduced local: no item identity to bind member access to
				return ReduceBodyItem(target.get());
			}
			if (atFuncRoot)
				break;
		}

		// §5.10: the captured closure environment -- the enclosing applications' bound
		// parameters -- is lexically nearer than any import or definition-scope item
		if (m_Env)
		{
			SharedTreeItem envItem; LispRef envKey; std::shared_ptr<FunctionBinding> envBnd;
			if (ResolveEnvSymbol(firstTok, &envItem, &envKey, &envBnd))
			{
				if (envBnd)
					throwErrorF("ExprParser", "'{}': a captured function value can only be applied or passed on as an argument"
						, fullStr.c_str());
				if (slash == e)
				{
					if (foundItemPtr)
						*foundItemPtr = envItem;
					return envKey;
				}
				// member access through a captured structured value: descend into the
				// argument item, as for a directly bound parameter
				if (!envItem)
					throwErrorF("ExprParser", "'{}': member access through captured '{}' requires the corresponding argument to be a direct item reference"
						, fullStr.c_str(), firstTok.GetStr().c_str());
				auto member = FindSubItem(envItem.get(), SharedStr(CharPtrRange(slash + 1, e)));
				if (!member)
					throwErrorF("ExprParser", "'{}': the argument captured as '{}' has no member '{}'"
						, fullStr.c_str(), firstTok.GetStr().c_str(), SharedStr(CharPtrRange(slash + 1, e)).c_str());
				member->UpdateMetaInfo();
				if (m_SubstBuff)
					registerSupplier(*m_SubstBuff, member.get());
				if (foundItemPtr)
					*foundItemPtr = member;
				return member->GetCheckedKeyExpr();
			}
		}

		// imports and externals: own scope + explicit imports first, then the lexical
		// definition scope (§4.6 revision 2026-07-13: identifiers resolve to what is
		// visible from the point of definition; the call site stays invisible).
		// The definition scope is the WHOLE enclosing chain, not just the immediate
		// parent: a function nested in another function's body must still see the
		// container scope. FindTreeItemByID stops ascending at the first item with a
		// using-cache, so one GetTreeParent() step only suffices while the function
		// sits directly in a container; from '/outer/inner' it lands on '/outer' and
		// the container was never consulted (ObjectVision/GeoDMS#1166).
		auto found = m_FuncItem->FindItem(fullStr);
		for (auto defScope = m_FuncItem->GetTreeParent(); !found && defScope; defScope = defScope->GetTreeParent())
			found = defScope->FindItem(fullStr);
		if (!found)
			throwErrorF("ExprParser", "'{}': unknown identifier in body of function '{}' (visible are: parameters, local items, 'using' imports, and the definition scope)"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		if (found->IsFunctionItem())
		{
			if (foundItemPtr)
			{
				if (m_SubstBuff)
					registerSupplier(*m_SubstBuff, found.get());
				*foundItemPtr = found; // function passed on as an argument: binding only, no key expression
				return {};
			}
			throwErrorF("ExprParser", "'{}': a function can only be applied or passed on as an argument"
				, fullStr.c_str());
		}
		if (found->InTemplate())
		{
			// #1166: a nested function's body may reference the ENCLOSING function's
			// LOCAL items. Those have no value of their own here -- they must be reduced
			// in the enclosing application, which holds its bindings. Its parameters are
			// captured by value in the environment (resolved above); a local is reduced
			// in place by delegating to that application on the parent chain. When the
			// nested function was returned as a closure and applied elsewhere, no parent
			// on the chain owns 'found' and the reference is rejected as before.
			for (const FunctionApplication* p = m_Parent; p; p = p->m_Parent)
				if (IsNestedInside(found.get(), p->m_FuncItem))
				{
					// non-const: the parent is a live stack application, and this is the
					// very reduction it would perform for the item itself (its memo and
					// in-progress set keep circular-reference detection intact)
					auto* encl = const_cast<FunctionApplication*>(p);
					if (foundItemPtr)
						*foundItemPtr = nullptr; // reduced in the enclosing scope: no item identity to bind member access to
					return encl->ReduceBodyItem(found.get());
				}
			throwErrorF("ExprParser", "'{}': reference to (part of) a template or function from body of function '{}'"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		}
		found->UpdateMetaInfo();
		if (m_SubstBuff)
			registerSupplier(*m_SubstBuff, found.get());
		if (foundItemPtr)
			*foundItemPtr = found;
		return found->GetCheckedKeyExpr();
	}

	// resolve a body-call head to the function being applied; sets *paramBinding when the
	// head is a function-valued parameter (so its pre-bound slots participate).
	// mayFail: arity-fallback probe -- return null instead of throwing when no function
	// is found (the caller then falls through to the operator path).
	SharedTreeItem FunctionApplication::ResolveBodyHeadFunction(const TreeItem* /*refScope*/, TokenID headID, std::shared_ptr<FunctionBinding>* paramBinding, bool mayFail)
	{
		if (paramBinding) *paramBinding = nullptr;

		if (auto headChild = m_FuncItem->GetConstSubTreeItemByID(headID))
			for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
				if (m_Params[i] == headChild.get())
				{
					if (!m_ArgBindings[i])
						throwErrorF("ExprParser", "'{}': parameter is applied as a function but the corresponding argument is not a function reference"
							, headID.GetStr().c_str());
					if (paramBinding) *paramBinding = m_ArgBindings[i];
					return m_ArgBindings[i]->funcItem;
				}

		// §5.10: a captured function value from the closure environment
		if (m_Env)
		{
			SharedTreeItem envItem; LispRef envKey; std::shared_ptr<FunctionBinding> envBnd;
			if (ResolveEnvSymbol(headID, &envItem, &envKey, &envBnd))
			{
				if (!envBnd)
					throwErrorF("ExprParser", "'{}': captured value is applied as a function but is not a function reference"
						, headID.GetStr().c_str());
				if (paramBinding) *paramBinding = envBnd;
				return envBnd->funcItem;
			}
		}

		auto callee = m_FuncItem->FindItem(SharedStr(headID.AsStrRange()));
		if (!callee || !callee->IsFunctionItem())
			if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
				if (auto lex = defParent->FindItem(SharedStr(headID.AsStrRange())); lex && lex->IsFunctionItem())
					callee = lex;
		if (!callee || !callee->IsFunctionItem())
			// the auto-imported prelude is the implicit outermost namespace for call heads
			if (auto pf = FindPreludeFunction(headID); pf && pf->IsFunctionItem())
				callee = pf;
		if (!callee || !callee->IsFunctionItem())
		{
			if (mayFail) // arity-aware head dispatch probe: no function -> operator path reports
				return {};
			if (!callee)
				throwErrorF("ExprParser", "'{}': unknown operator or function in body of function '{}'"
					, headID.GetStr().c_str(), m_FuncItem->GetFullName().c_str());
			throwErrorF("ExprParser", "'{}': template instantiations are not supported inside function bodies"
				, headID.GetStr().c_str());
		}
		if (m_SubstBuff)
			registerSupplier(*m_SubstBuff, callee.get());
		return callee;
	}

	// resolve one argument of a body-level function application to a CallArg (which may be
	// a data key, a function value / partial binding, or a hole).
	CallArg FunctionApplication::ResolveBodyArg(const TreeItem* refScope, LispPtr argExpr)
	{
		if (argExpr.IsSymb())
		{
			TokenID sym = argExpr.GetSymbID();
			if (sym == t_Hole)
			{
				CallArg a; a.isHole = true; return a;
			}
			if (!token::isConst(sym) && !ValueClass::FindByScriptName(sym))
			{
				SharedStr s(sym.AsStrRange());
				bool bare = std::find(s.begin(), s.send(), '/') == s.send();
				if (bare)
				{
					// function-valued parameter?
					if (auto headChild = m_FuncItem->GetConstSubTreeItemByID(sym))
						for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
							if (m_Params[i] == headChild.get() && m_ArgBindings[i])
							{
								CallArg a; a.binding = m_ArgBindings[i]; return a;
							}
					// §5.10: a captured value from the closure environment
					if (m_Env)
					{
						CallArg a;
						if (ResolveEnvSymbol(sym, &a.item, &a.key, &a.binding))
							return a;
					}
					// import or lexically visible function?
					auto callee = m_FuncItem->FindItem(s);
					if (!callee || !callee->IsFunctionItem())
						if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
							if (auto lex = defParent->FindItem(s); lex && lex->IsFunctionItem())
								callee = lex;
					if (!callee)
						if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
							callee = pf; // prelude: implicit outermost namespace, also for function references
					if (callee && callee->IsFunctionItem())
					{
						if (m_SubstBuff) registerSupplier(*m_SubstBuff, callee.get());
						CallArg a; a.binding = MakeAllHoles(callee); return a;
					}
				}
				CallArg a; a.key = ResolveBodySymbol(refScope, sym, &a.item); return a;
			}
		}
		if (argExpr.IsRealList() && argExpr.Left().IsSymb())
		{
			TokenID headID = argExpr.Left().GetSymbID();

			// §5.9 container literal passed to a nested call: resolve in body scope ('.' -> domain)
			if (headID == t_ContainerLiteral)
			{
				CallArg a; a.literal = BuildContainerLiteral(argExpr,
					[&](LispPtr e) { return SubstituteBodyExpr(refScope, e); });
				return a;
			}

			// §5.10 applied call result: reduce the inner expression to a function value,
			// bind the outer arguments; the result may again be a value or a binding
			if (headID == t_ApplyValue)
			{
				CallArg fnVal = ResolveBodyArg(refScope, argExpr.Right().Left());
				if (!fnVal.binding)
					throwErrorF("ExprParser", "'(...)' applied to an expression that is not a function value");
				std::vector<CallArg> outer;
				for (LispPtr a = argExpr.Right().Right(); !a.EndP(); a = a.Right())
					outer.push_back(ResolveBodyArg(refScope, a.Left()));
				FunctionBinding merged = MergeBinding(*fnVal.binding, outer);
				if (merged.NrHoles() == 0)
					return ReduceMergedValue(merged, this, m_SubstBuff, m_ErrorHolder);
				CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
			}

			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			if (og->IsTemplateCall())
			{
				std::shared_ptr<FunctionBinding> pb;
				auto headFn = ResolveBodyHeadFunction(refScope, headID, &pb);
				std::vector<CallArg> sub;
				for (LispPtr a = argExpr.Right(); !a.EndP(); a = a.Right())
				{
					LispPtr ae = a.Left();
					if (ae.IsSymb() && IsRestParamSymbol(ae.GetSymbID()))
					{
						if (!a.Right().EndP())
							throwErrorF("ExprParser", "'{}': rest parameter '{}' must be the trailing argument of the call"
								, m_FuncItem->GetFullName().c_str(), ae.GetSymbStr().c_str());
						SpliceRestArgs(sub); // '...x' passed on: splice the captured tail
						break;
					}
					sub.push_back(ResolveBodyArg(refScope, ae));
				}
				// §5.7: variant sets dispatch by argument type on nested calls too
				if (!pb && TreeItem_IsFunctionVariantSet(headFn.get()))
				{
					auto variant = ResolveVariant(headFn.get(), sub, m_ErrorHolder);
					headFn = make_shared_tree(variant, existing_obj{});
					if (m_SubstBuff) registerSupplier(*m_SubstBuff, variant);
					CheckFunctionDefinition(variant);
				}
				FunctionBinding calleeBinding = pb ? *pb : *MakeAllHoles(headFn);
				if (!pb && !calleeBinding.env && IsNestedInside(headFn.get(), m_FuncItem))
					calleeBinding.env = MakeCurrentEnv(); // #1166: nested callee sees the enclosing parameters
				FunctionBinding merged = MergeBinding(calleeBinding, sub);
				if (merged.NrHoles() == 0)
					return ReduceMergedValue(merged, this, m_SubstBuff, m_ErrorHolder); // §5.10: data key OR closure binding
				CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
			}
		}
		CallArg a; a.key = SubstituteBodyExpr(refScope, argExpr); return a;
	}

	// WP3.4 + WP4.1 tranche 3: definition-time validation of a function body -- the
	// scope/shape walk (every identifier must resolve; operator/function heads must be
	// known; direct calls have the right arity) now also derives TYPES, bottom-up over
	// the body reachable from the designated result. The function's own type/domain
	// variables (and its unit parameters) are RIGID: the body must be well-typed for
	// EVERY instantiation, so anything that would pin them to a concrete type/unit,
	// force two of them equal, or narrow them below their declared constraints is a
	// definition error -- caught here once, without any application. Each callee
	// instantiates its declared signature under a fresh variable instance. Built-in
	// operators, externals, variant selections, partial applications and
	// member/container accesses stay DEFERRED (type Unknown) and remain checked per
	// application by the reduction -- operator signatures are the next tranche.

	// §12.7: generated/computed member maps are keyed CASE-INSENSITIVELY --
	// TreeItem lookup (and hence SubItemOperator's GetCurrItem) accepts either
	// case (with a deprecation warning), so an exact-case map would falsely
	// miss legal references (S1). Folding is FIXED ASCII, matching the
	// engine's token equality -- locale-dependent folding (stricmp/tolower)
	// could diverge under a changed CRT locale (review finding)
	struct MemberPathLess
	{
		static char Fold(char ch) { return (ch >= 'A' && ch <= 'Z') ? char(ch - 'A' + 'a') : ch; }

		bool operator()(const SharedStr& a, const SharedStr& b) const
		{
			CharPtr ai = a.begin(), ae = a.send(), bi = b.begin(), be = b.send();
			for (; ai != ae && bi != be; ++ai, ++bi)
			{
				char fa = Fold(*ai), fb = Fold(*bi);
				if (fa != fb)
					return fa < fb;
			}
			return (ae - ai) < (be - bi);
		}
	};

	// the definition-time type of a body expression (kinds-level, §5 terms)
	struct DefType
	{
		enum class Kind : UInt8 { Unknown, Data, UnitVal, Func, Container } kind = Kind::Unknown;

		// value position (Data: the values class; UnitVal: the unit's class):
		// concrete class XOR unifier node XOR unknown
		const ValueClass* vc = nullptr;
		SizeT vNode = NO_TYPE_VAR;

		// value composition of a Data term when known (§18.4); Unknown = no claim.
		// Consumed by candidate elimination only (a Single-composition argument
		// cannot serve a sequence-registered member and vice versa)
		ValueComposition vcomp = ValueComposition::Unknown;

		// values-unit IDENTITY of a Data term (batch U, §8): a unit node when the
		// declared values token names a unit parameter or domain-sorted generic
		// (the function-signature K2 bridge -- the SAME node its domain role uses),
		// or the concrete scope unit it resolves to. Class reasoning stays on
		// vc/vNode; identity is compared by UnifyData's values-identity block.
		// Neither set = no identity claim (defers)
		SizeT vuNode = NO_TYPE_VAR;
		const AbstrUnit* vUnit = nullptr; SharedTreeItem vKeep;

		// domain position (Data: the domain; UnitVal: the unit's own identity)
		enum class Dom : UInt8 { Unknown, Void, Concrete, Node } dom = Dom::Unknown;
		const AbstrUnit* domUnit = nullptr; SharedTreeItem domKeep; // Concrete
		SizeT dNode = NO_TYPE_VAR;                                  // Node

		// Func: the function or signature item whose declared positions type an
		// application of this value; tokens naming varsOwner's generic variables
		// resolve to (varsOwner, instance) nodes; tok2owner adds the type-application
		// translation (sig var -> varsOwner var)
		const TreeItem* fn = nullptr;
		const TreeItem* varsOwner = nullptr;
		UInt32 instance = 0;
		std::shared_ptr<std::map<TokenID, TokenID>> tok2owner;

		// K11 member map (§12.7): the member set of a composite result -- the
		// pseudo-expanded members of a container-GENERATING meta application
		// (for_each, Kind::Container) OR the DESCRIBED sub-items of a
		// cacheable operator's cache result (unique/Values & co., any kind;
		// slSubItemCall tranche) -- keyed by the sub-item PATH (a name-array
		// entry may contain '/'), case-insensitively (TreeItem lookup accepts
		// either case). membersComplete marks a definitively-known set -- the
		// only state in which a missing member may be reported (sound: meta
		// rules reject every inline member access; described-complete cache
		// results make SubItemOperator certain to reject the same reference)
		std::shared_ptr<const std::map<SharedStr, DefType, MemberPathLess>> members;
		bool membersComplete = false;
	};

	struct FunctionChecker
	{
		const TreeItem*              m_FuncItem = nullptr;
		std::vector<const TreeItem*> m_Params;
		std::set<const TreeItem*>    m_InProgress;

		TypeUnifier                  m_Unifier;
		SharedStr                    m_DeclSource;
		UInt32                       m_NextInstance = 1; // 0 = the checked function's own (rigid) variables
		std::map<const TreeItem*, DefType> m_ItemTypes;  // memoized body-item types
		// batch D (§6.1): memoize operator-application results per (refScope,
		// hash-consed application expr). LispRefs are interned, so two textually
		// identical applications share one LispObj; keying on it makes repeated
		// subexpressions denote ONE result node -- a K6 SOUNDNESS prerequisite (two
		// `unique(a)`/`select(c)` occurrences reduce to a single DataController, so
		// their fresh existential units must be the same node), and a diagnostics
		// de-duplicator for every repeated subexpression as a bonus. refScope is in
		// the key because symbol resolution inside the application depends on it.
		// The key holds a STRONG LispRef (§12.7 review): the map entry itself pins
		// the interned node, so the pointer-ordered key can never dangle or ABA
		std::map<std::pair<const TreeItem*, LispRef>, DefType> m_ApplTypes;
		// K11a-3.1 review: memoize the checked function's own parameter types -- every
		// nw/member reference re-derived ParamType (and for a structured parameter
		// rebuilt the whole member map incl. per-member FindItem scope walks). Nodes
		// are get-or-create so this only removes rework, never changes a verdict.
		std::map<UInt32, DefType>    m_ParamTypes;
		std::vector<SharedTreeItem>  m_Keep;             // liveness of resolved callees

		DefType InferBodyItem(const TreeItem* refItem);
		DefType InferExpr(const TreeItem* refScope, LispPtr expr);
		DefType InferArg(const TreeItem* refScope, LispPtr argExpr);
		DefType InferApplication(const TreeItem* refScope, const DefType& fnVal, LispPtr argsList, CharPtr headName);
		DefType InferOperatorApplication(const AbstrOperGroup* og, TokenID headID, const TreeItem* refScope, LispPtr argsList); // op-sig batch A
		DefType ApplyOperRecord(const OperGroupSignatures::MergedRecord& mr, const SharedStr& headName, const std::vector<DefType>& argTerms
			, const TreeItem* refScope = nullptr, LispPtr argsList = LispPtr()); // K11b: refScope+argsList enable ArgContainer member enumeration
		// §6.2 cross-record fallback: the DOMAIN skeleton all surviving records agree on
		std::optional<OperGroupSignatures::MergedRecord> BuildDomainSkeletonRecord(const OperGroupSignatures* sigs, const std::vector<Int32>& recordIdxs);
		DefType ParamType(UInt32 idx);     // memoized front (m_ParamTypes)
		DefType ParamTypeImpl(UInt32 idx);
		// K11a-1: the declared member set of a structured unit parameter (a member
		// block on a `unit<...> P { … }` parameter), each member typed by its declared
		// value class over the parameter's own domain. Enables def-time typing of
		// `P/member` access (InferExpr case 2 / InferParamMember).
		std::shared_ptr<const std::map<SharedStr, DefType, MemberPathLess>>
			BuildParamMembers(const TreeItem* p, SizeT paramDomNode, const TreeItem* memberSrc = nullptr);
		// K11b: the CONCRETE members of a definition-scope container argument
		std::shared_ptr<const std::map<SharedStr, DefType, MemberPathLess>>
			BuildConcreteContainerMembers(const TreeItem* c);
		// K11b: link an ArgContainer position's shared domain/values against the actual members
		void LinkContainerArg(const SignatureRecord::Pos& p, const DefType& argTerm, LispPtr argExpr,
			const TreeItem* refScope, const SharedStr& argSrc, LispPtr argsList,
			const std::function<SizeT(sig_var)>& VN, const std::function<SizeT(sig_var)>& DN);
		DefType InferParamMember(UInt32 paramIdx, const SharedStr& memberPath);
		DefType DeclaredItemType(const TreeItem* item);
		DefType PositionType(const TreeItem* posItem, const TreeItem* fnDef, UInt32 instance,
			const TreeItem* ownerFn, UInt32 ownerInstance, const std::map<TokenID, TokenID>* tok2owner,
			const TreeItem* itemScope = nullptr);
		void UnifyData(const DefType& a, const DefType& b, const SharedStr& srcA, const SharedStr& srcB);

		// unifier nodes; the checked function's own variables (instance 0) are rigid.
		// A rigid variable may lexically belong to an ENCLOSING function (nested
		// declarations see the enclosing type-parameter clause) -- seed its constraint
		// from that declaration when the checked function's own lists lack it.
		SizeT ValNode(const TreeItem* owner, UInt32 inst, TokenID tok)
		{
			bool rigid = owner == m_FuncItem && inst == 0;
			TokenID fallback;
			if (rigid && !DeclaredConstraintOf(m_FuncItem, tok))
				for (auto enc = m_FuncItem->GetTreeParent(); enc && enc->IsFunctionItem(); enc = enc->GetTreeParent())
					if (TokenID c = DeclaredConstraintOf(enc.get(), tok))
					{
						fallback = c;
						break;
					}
			return m_Unifier.ValueVar(owner, inst, tok
				, rigid ? m_DeclSource : mySSPrintF("function '{}'", owner->GetFullName().c_str()), rigid, fallback);
		}
		SizeT UNode(const TreeItem* owner, UInt32 inst, TokenID tok, const ValueClass* declaredCls = nullptr)
		{
			// the token may name a unit PARAMETER of the owner whose declared class
			// must pin the companion regardless of WHICH path creates the node
			// first -- type applications and sig bindings reach here before
			// ParamType does, and creation is get-or-create-once (review finding:
			// an unpinned first creation froze the companion rigid/unconstrained,
			// making the verdict depend on the body's reference order)
			if (!declaredCls && owner && owner->IsFunctionItem())
			{
				const TreeItem* q = owner->_GetFirstSubItem();
				for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(owner); j != n && q; ++j, q = q->GetNextItem())
					if (q->GetID() == tok && IsUnit(q))
					{
						declaredCls = AsUnit(q)->GetValueType();
						break;
					}
			}
			// the companion class node needs the SAME enclosing-declaration
			// constraint fallback as ValNode: a rigid domain variable lexically
			// belonging to an ENCLOSING function (named here only through a type
			// application) must not seed an all-set acceptance -- the t3 defect-#3
			// rule, re-applied to the batch-U companions
			bool rigid = owner == m_FuncItem && inst == 0;
			TokenID fallback;
			if (rigid && !declaredCls && !DeclaredConstraintOf(m_FuncItem, tok))
				for (auto enc = m_FuncItem->GetTreeParent(); enc && enc->IsFunctionItem(); enc = enc->GetTreeParent())
					if (TokenID c = DeclaredConstraintOf(enc.get(), tok))
					{
						fallback = c;
						break;
					}
			return m_Unifier.UnitVar(owner, inst, tok, rigid, declaredCls, fallback);
		}
		SharedTreeItem ResolveUnitInScope(TokenID tok, const TreeItem* fnDef)
		{
			SharedStr s(tok.AsStrRange());
			auto u = fnDef->FindItem(s);
			if (!u || !IsUnit(u.get()))
				if (auto defP = fnDef->GetTreeParent()) // lexical definition scope (§4.6)
					u = defP->FindItem(s);
			if (u && IsUnit(u.get()) && !u->InTemplate()) // body-local units stay deferred (their meta info is not available here)
				return u;
			return {};
		}
		// a nested function's body may reference the ENCLOSING function's parameters
		// and locals; those are bound through the closure environment at reduction and
		// have no definition-time value here -- resolve them only to defer them
		SharedTreeItem FindEnclosingFunctionMember(TokenID tok)
		{
			for (auto enc = m_FuncItem->GetTreeParent(); enc && enc->IsFunctionItem(); enc = enc->GetTreeParent())
				if (auto c = enc->GetConstSubTreeItemByID(tok))
					return c;
			return {};
		}
		// a body-local declaration in a sub-container between the declaring item and
		// the function root shadows outer names at reduction; its meta info is not
		// available at definition time, so such declared-type tokens must defer
		bool HasBodyShadower(TokenID tok, const TreeItem* fromScope)
		{
			for (const TreeItem* scope = fromScope; scope; scope = scope->GetTreeParent().get())
			{
				if (scope == m_FuncItem)
					return false; // direct children are seen (and deferred) by ResolveUnitInScope
				if (scope->GetConstSubTreeItemByID(tok))
					return true;
			}
			return false;
		}

		// 0=parameter (index via *paramIdx), 1=local (via *local), 2=import/external; throws on unknown.
		// §12.7: the bare code 2 conflates CLOSED and OPEN references -- extKindPtr
		// discriminates (see ExtRefKind); externalOut receives the resolved item for
		// the DefScopeExternal case (the only evaluable one).
		// 3 (only when genSubPathOut is passed) = a path miss below an item whose
		// rule may GENERATE sub-items at instantiation (§12.7 for_each tranche):
		// *local is that generating item, *genSubPathOut the remaining path -- the
		// caller resolves it against the container's pseudo-expanded member set
		enum class ExtRefKind : UInt32 { DefScopeExternal = 0, ParamMember, PreludeFunc, ClosureCapture };
		int ResolveName(const TreeItem* refScope, TokenID sym, const TreeItem** local, UInt32* paramIdx = nullptr,
			SharedTreeItem* externalOut = nullptr, ExtRefKind* extKindPtr = nullptr, SharedStr* genSubPathOut = nullptr);

		// §12.7 impedance tranche: definition-time K13 spec processing. A body
		// sub-expression CLOSED over the formals (references no parameter, rest
		// slice, closure capture, or parameter member -- transitively through body
		// locals; externals terminate the scan since formals are lexically
		// invisible outside the function) is REDUCED to its DataController key --
		// the same hash-consed key every application interns (β-substitution is
		// the identity on closed expressions) -- and EVALUATED at definition scan,
		// storage-backed sources included (the explicit ruling). Any failure at
		// any stage yields nullopt/empty = defer, exactly as without the spec.
		std::map<std::pair<const TreeItem*, LispRef>, LispRef> m_ClosedKeyMemo; // strong key AND value LispRefs: nothing dangles
		std::set<const TreeItem*> m_ScanBusy; // cycle guard for body-local recursion (distinct from m_InProgress)
		LispRef TryBuildClosedKeyExpr(const TreeItem* refScope, LispPtr expr);
		LispRef TryBuildClosedKeyExprImpl(const TreeItem* refScope, LispPtr expr);
		std::optional<SharedStr> EvalClosedSpec(const TreeItem* refScope, LispPtr specExpr);
		std::optional<DefType> TrySpecProcessing(const AbstrOperGroup* og,
			const OperGroupSignatures::MergedRecord& mr, const std::vector<const Operator*>& survivors,
			const SharedStr& headName, const TreeItem* refScope, LispPtr argsList, const std::vector<DefType>& argTerms);

		// §12.7 for_each tranche: the container-generating meta family. A
		// closed name array (and, for for_each_ind, the closed field spec) is
		// EVALUATED at definition scan -- storage-backed sources included, per
		// the ruling -- and the application types as a Container whose member
		// set is the pseudo-expansion of the names; member domain/values types
		// come from the layout's unit positions (a formal unit parameter
		// contributes its unifier node: the K2 bridge). Any failure at any
		// stage defers exactly as before the tranche.
		std::optional<std::vector<SharedStr>> EvalClosedStrArray(const TreeItem* refScope, LispPtr expr);
		std::optional<DefType> TryMetaContainerProcessing(const AbstrOperGroup* og, TokenID headID,
			const TreeItem* refScope, LispPtr argsList, const std::vector<DefType>& argTerms);
		DefType InferGeneratedMember(TokenID sym, const TreeItem* genItem, const SharedStr& subPath);
	};

	// §12.7: may `item`'s calculation rule COMPUTE sub-items at application --
	// a meta head GENERATING config items (for_each_*), or a cacheable rule
	// whose composite cache result carries members (unique/Values & co., now
	// reachable through slSubItemCall)? Any non-empty rule qualifies since the
	// slSubItemCall tranche: reduction resolves the remaining path against the
	// rule's result, so a declared-tree miss below such an item is never
	// reported as unknown at definition -- it types via the pseudo-expanded or
	// described member set (a code-3 access) or defers to the per-application
	// SubItem check. (A leading-'=' rule qualifies too: InferBodyItem reports
	// its own honest error, matching ReduceBodyItem's.)
	bool RuleMayComputeSubItems(const TreeItem* item)
	{
		return !item->GetExpr().empty();
	}

	// K11a by-example (review finding): an EXEMPLAR is a real config item, so its
	// declared children are the member set only when nothing can ADD to them at
	// instantiation. A storage manager (GDAL & co. generate layer sub-items at
	// UpdateMetaInfo) or a calculation rule (a composite result contributes its
	// members) leaves the set OPEN -- membersComplete must then stay false, or a
	// body reference to a generated member is a false definition-time
	// "declares no member" error whose verdict even depends on whether the
	// exemplar's meta info happened to be updated first. An explicitly written
	// member block is always closed: it declares an interface, not an item.
	bool ExemplarMemberSetIsClosed(const TreeItem* exemplar)
	{
		return !RuleMayComputeSubItems(exemplar) && !exemplar->HasStorageManager();
	}

	// K11b: is `item` a plain CONTAINER -- something that can carry an operator's
	// ArgContainer members? Units and data items have their own positions, function
	// items and templates are inert type/logic carriers, never member bags.
	bool IsPlainContainer(const TreeItem* item)
	{
		return item && !IsUnit(item) && !IsDataItem(item) && !item->IsFunctionItem() && !item->IsTemplate();
	}

	int FunctionChecker::ResolveName(const TreeItem* refScope, TokenID sym, const TreeItem** local, UInt32* paramIdx,
		SharedTreeItem* externalOut, ExtRefKind* extKindPtr, SharedStr* genSubPathOut)
	{
		if (local) *local = nullptr;
		if (externalOut) *externalOut = nullptr;
		if (extKindPtr) *extKindPtr = ExtRefKind::DefScopeExternal;
		SharedStr fullStr(sym.AsStrRange());
		CharPtr b = fullStr.begin(), e = fullStr.send();
		if (b != e && (*b == '.' || *b == '/'))
			throwErrorF("ExprParser", "'{}': dot-relative and absolute references are not supported inside function bodies"
				, fullStr.c_str());
		CharPtr slash = std::find(b, e, '/');
		TokenID firstTok = (slash == e) ? sym : GetTokenID_mt(b, slash);

		for (const TreeItem* scope = refScope; scope; scope = scope->GetTreeParent().get())
		{
			bool atFuncRoot = (scope == m_FuncItem);
			auto child = scope->GetConstSubTreeItemByID(firstTok);
			if (child)
			{
				for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
					if (m_Params[i] == child.get())
					{
						if (paramIdx) *paramIdx = i;
						if (slash != e)
						{
							if (extKindPtr) *extKindPtr = ExtRefKind::ParamMember; // OPEN: depends on the argument
							// K11a-2: hand the member path to InferExpr, which types it
							// against a structured parameter's member map (else defers)
							if (genSubPathOut) *genSubPathOut = SharedStr(CharPtrRange(slash + 1, e));
							return 2;
						}
						return 0;
					}
				SharedTreeItem cursor = child;
				if (slash != e)
				{
					// segment-wise descend (mirroring FindSubItem, message included) so
					// a miss can be attributed to a GENERATING item on the walked path:
					// when an item's rule may GENERATE sub-items at instantiation
					// (§12.7 for_each tranche), the path remaining FROM that item
					// resolves against its pseudo-expanded member set (code 3) instead
					// of being unknown. The deepest generating item wins (its member
					// paths are keyed from itself); a generating ancestor above a
					// declared child covers names that route through declared items.
					CharPtr segBegin = slash + 1;
					std::vector<std::pair<SharedTreeItem, CharPtr>> descended;
					descended.emplace_back(cursor, segBegin);
					while (segBegin != e)
					{
						CharPtr segEnd = std::find(segBegin, e, DELIMITER_CHAR);
						auto sub = cursor->GetConstSubTreeItemByID(GetTokenID_mt(segBegin, segEnd));
						if (!sub)
						{
							if (genSubPathOut)
								for (auto ri = descended.rbegin(); ri != descended.rend(); ++ri)
									if (RuleMayComputeSubItems(ri->first.get()))
									{
										if (local) *local = ri->first.get();
										*genSubPathOut = SharedStr(CharPtrRange(ri->second, e));
										return 3;
									}
							throwErrorF("FindSubItem", "Cannot find {} from {}"
								, SharedStr(CharPtrRange(segBegin, segEnd)), cursor->GetFullName().c_str());
						}
						cursor = sub;
						segBegin = (segEnd == e) ? e : segEnd + 1;
						descended.emplace_back(cursor, segBegin);
					}
				}
				if (local) *local = cursor.get();
				return 1;
			}
			if (atFuncRoot)
				break;
		}

		auto found = m_FuncItem->FindItem(fullStr);
		// lexical definition scope (§4.6): the whole enclosing chain -- see the
		// matching walk in ResolveBodySymbol (ObjectVision/GeoDMS#1166). This site
		// types the reference and throws first, so it needs the same ascent.
		for (auto defScope = m_FuncItem->GetTreeParent(); !found && defScope; defScope = defScope->GetTreeParent())
			found = defScope->FindItem(fullStr);
		if (!found && slash == e)
			if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
			{
				if (extKindPtr) *extKindPtr = ExtRefKind::PreludeFunc; // a function value: not an evaluable data spec
				return 2; // prelude: implicit outermost namespace, also for function references
			}
		if (!found && FindEnclosingFunctionMember(firstTok))
		{
			if (extKindPtr) *extKindPtr = ExtRefKind::ClosureCapture; // OPEN: bound per application
			return 2; // captured through the closure environment; typed per application
		}
		if (!found)
			throwErrorF("ExprParser", "'{}': unknown identifier in body of function '{}' (visible are: parameters, local items, 'using' imports, and the definition scope)"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		if (!found->IsFunctionItem() && found->InTemplate())
		{
			// FindItem ascends the parent chain, so an ENCLOSING function's data/unit
			// parameter or local is 'found' here -- those are §5.10 closure captures,
			// bound through the environment at reduction: defer, don't reject
			if (FindEnclosingFunctionMember(firstTok))
			{
				if (extKindPtr) *extKindPtr = ExtRefKind::ClosureCapture; // OPEN
				return 2;
			}
			throwErrorF("ExprParser", "'{}': reference to (part of) a template or function from body of function '{}'"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		}
		// §12.7 (review finding): reduction resolves the closure ENVIRONMENT before
		// imports/definition scope (ResolveBodySymbol), so a definition-scope item
		// SHADOWED by an enclosing function's member is bound to the CAPTURE at
		// reduction -- classifying it DefScopeExternal would evaluate the wrong
		// (and formal-dependent) binding. Probe the shadow for the §12.7 caller
		if (extKindPtr && FindEnclosingFunctionMember(firstTok))
		{
			*extKindPtr = ExtRefKind::ClosureCapture; // OPEN: the capture shadows `found`
			return 2;
		}
		if (externalOut) *externalOut = found; // DefScopeExternal: CLOSED by construction (§12.7)
		return 2;
	}

	// §12.7: reduce a body sub-expression that is CLOSED over the formals to its
	// DataController key -- the SAME hash-consed key every application interns
	// (β-substitution is the identity on closed expressions), so a definition-time
	// evaluation reads the value once through the very DC reduction will use.
	// Empty result = open or not buildable: the caller defers. Mirrors the closed
	// subset of FunctionApplication::SubstituteBodyExpr; memoized (the strong
	// LispRefs in the memo also pin the built keys for the checker's lifetime).
	LispRef FunctionChecker::TryBuildClosedKeyExpr(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.EndP())
			return {};
		auto memoKey = std::make_pair(refScope, LispRef(expr));
		if (auto it = m_ClosedKeyMemo.find(memoKey); it != m_ClosedKeyMemo.end())
			return it->second;
		LispRef built = TryBuildClosedKeyExprImpl(refScope, expr);
		m_ClosedKeyMemo.emplace(std::move(memoKey), built);
		return built;
	}

	LispRef FunctionChecker::TryBuildClosedKeyExprImpl(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.IsRealList())
		{
			if (!expr.Left().IsSymb())
				return {};
			LispRef head = expr.Left();
			TokenID headID = head.GetSymbID();
			if (headID == token::sourceDescr)
				return TryBuildClosedKeyExpr(refScope, expr.Right().Left());
			if (headID == token::arrow || headID == token::scope || headID == token::subitem
				|| headID == t_ApplyValue || headID == t_ContainerLiteral)
				return {};
			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			if (og->IsTemplateCall() && !ValueClass::FindByScriptName(headID))
				return {}; // function-call heads: not buildable in v1 (ReduceValue re-entrancy)
			if (!og->IsTemplateCall() && !og->MustCacheResult())
				return {}; // meta heads: reduction rejects them in bodies anyway
			std::vector<LispRef> substArgs;
			for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
			{
				auto sub = TryBuildClosedKeyExpr(refScope, argPtr.Left());
				if (sub.EndP() && !argPtr.Left().EndP())
					return {}; // an open/unbuildable argument
				substArgs.push_back(sub);
			}
			LispRef argList;
			for (auto ri = substArgs.rbegin(); ri != substArgs.rend(); ++ri)
				argList = LispRef(*ri, argList);
			LispRef result = RewriteExprTop(LispRef(head, std::move(argList)));
			if (!og->IsTemplateCall() && og->CanResultToConfigItem())
			{
				DataControllerRef dc = GetOrCreateDataController(result);
				auto supplier = dc->MakeResult();
				if (!supplier)
					return {}; // metainfo failure: defer (the application reports it)
				if (!supplier->IsCacheItem())
					result = supplier->GetCheckedKeyExpr();
			}
			return result;
		}
		if (expr.IsSymb())
		{
			TokenID symbID = expr.GetSymbID();
			if (symbID == t_Hole)
				return {}; // (a '...x' rest symbol resolves to its param child below: class 0 = open)
			if (token::isConst(symbID))
				return ExprList(symbID);
			if (ValueClass::FindByScriptName(symbID))
				return List(LispRef(expr));
			const TreeItem* local = nullptr; UInt32 paramIdx = 0;
			SharedTreeItem external; ExtRefKind extKind = ExtRefKind::DefScopeExternal;
			switch (ResolveName(refScope, symbID, &local, &paramIdx, &external, &extKind))
			{
			case 0:
				return {}; // a formal: OPEN
			case 1:
			{
				if (!local)
					return {};
				if (!m_ScanBusy.insert(local).second)
					return {}; // cyclic body-local reference: defer (reduction reports)
				LispRef r;
				SharedStr exprStr = local->GetExpr();
				if (!exprStr.empty() && !AbstrCalculator::MustEvaluate(exprStr.c_str()))
				{
					auto calc = AbstrCalculator::ConstructFromStr(local, exprStr, CalcRole::Calculator);
					auto localScope = local->GetTreeParent();
					r = TryBuildClosedKeyExpr(localScope.get(), RewriteExpr(calc->GetLispExprOrg()));
				}
				m_ScanBusy.erase(local);
				return r;
			}
			default:
				if (extKind != ExtRefKind::DefScopeExternal || !external)
					return {}; // param-member / prelude-fn / closure capture: OPEN or not a data value
				if (external->IsFunctionItem() || external->InTemplate())
					return {};
				external->UpdateMetaInfo();
				return external->GetCheckedKeyExpr();
			}
		}
		return LispRef(expr); // numeric / string / UInt64 literal: a valid DC key of its own
	}

	// §12.7: evaluate a closed spec sub-expression at definition scan. Literal
	// fast path reads straight off the parse tree; everything else -- including
	// storage-backed definition-scope items, per the explicit ruling -- goes
	// through the standard meta-thread calculation (the CalcCertainResult idiom
	// the dynamic-argument-policies spec read already uses). ANY failure,
	// including a transient storage failure, yields nullopt = defer: the
	// application retries the same DC and reports properly if it persists.
	std::optional<SharedStr> FunctionChecker::EvalClosedSpec(const TreeItem* refScope, LispPtr specExpr)
	{
		if (specExpr.IsStrn())
			return SharedStr(CharPtrRange(specExpr.GetStrnBeg(), specExpr.GetStrnEnd()));
		auto defScope = m_FuncItem->GetTreeParent();
		if (!defScope || defScope->InTemplate())
			return std::nullopt; // a function nested in a template: no evaluation context
		LispRef key;
		try
		{
			key = TryBuildClosedKeyExpr(refScope, specExpr);
		}
		catch (...)
		{
			m_ScanBusy.clear(); // unwind the cycle-guard marks of the aborted scan
			return std::nullopt;
		}
		if (key.EndP())
			return std::nullopt;
		try
		{
			FencedInterestRetainContext irc("FunctionChecker::EvalClosedSpec");
			auto dc = GetOrCreateDataController(key);
			if (!dc)
				return std::nullopt;
			irc.Add(dc.get());
			auto resItem = dc->MakeResult();
			if (!resItem || dc->WasFailed() || !IsDataItem(resItem.get()))
				return std::nullopt;
			FutureData fd = dc->CalcCertainResult();
			if (!fd || fd->WasFailed())
				return std::nullopt;
			auto adi = AsDataItem(fd->GetOld());
			if (!adi || !adi->HasVoidDomainGuarantee()
				|| adi->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() != ValueClassID::VT_SharedStr)
				return std::nullopt;
			return GetTheValue<SharedStr>(adi);
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	// §12.7 for_each tranche: the array sibling of EvalClosedSpec -- evaluate a
	// closed string ARRAY (any domain) at definition scan and read all its
	// values, storage-backed sources included per the ruling. The evaluation
	// runs through the very hash-consed DC every instantiation of the meta
	// application will use (closedness ⟺ β-substitution is the identity), so
	// the value is read once and the cache entry is warmed. ANY failure yields
	// nullopt = defer: the instantiation retries the same DC and reports
	// properly if it persists.
	std::optional<std::vector<SharedStr>> FunctionChecker::EvalClosedStrArray(const TreeItem* refScope, LispPtr expr)
	{
		auto defScope = m_FuncItem->GetTreeParent();
		if (!defScope || defScope->InTemplate())
			return std::nullopt; // a function nested in a template: no evaluation context
		LispRef key;
		try
		{
			key = TryBuildClosedKeyExpr(refScope, expr);
		}
		catch (...)
		{
			m_ScanBusy.clear(); // unwind the cycle-guard marks of the aborted scan
			return std::nullopt;
		}
		if (key.EndP())
			return std::nullopt;
		try
		{
			FencedInterestRetainContext irc("FunctionChecker::EvalClosedStrArray");
			auto dc = GetOrCreateDataController(key);
			if (!dc)
				return std::nullopt;
			irc.Add(dc.get());
			auto resItem = dc->MakeResult();
			if (!resItem || dc->WasFailed() || !IsDataItem(resItem.get()))
				return std::nullopt;
			FutureData fd = dc->CalcCertainResult();
			if (!fd || fd->WasFailed())
				return std::nullopt;
			auto adi = AsDataItem(fd->GetOld());
			if (!adi || adi->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() != ValueClassID::VT_SharedStr)
				return std::nullopt;
			DataReadLock lock(adi);
			auto sa = const_array_cast<SharedStr>(lock);
			if (!sa)
				return std::nullopt;
			SizeT n = adi->GetAbstrDomainUnit()->GetDataCount();
			std::vector<SharedStr> result;
			result.reserve(n);
			for (SizeT i = 0; i != n; ++i)
				result.push_back(sa->GetIndexedValue(i));
			return result;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	// the declared type of parameter idx: rigid variables for generic positions, a
	// rigid identity for unit parameters, a Func value for signature-typed parameters
	DefType FunctionChecker::ParamType(UInt32 idx)
	{
		if (auto it = m_ParamTypes.find(idx); it != m_ParamTypes.end())
			return it->second;
		DefType pt = ParamTypeImpl(idx);
		m_ParamTypes[idx] = pt;
		return pt;
	}

	DefType FunctionChecker::ParamTypeImpl(UInt32 idx)
	{
		const TreeItem* p = m_Params[idx];
		if (auto sig = TreeItem_GetFunctionParamSignature(m_FuncItem, idx))
		{
			DefType r; r.kind = DefType::Kind::Func; r.fn = sig.get();
			m_Keep.push_back(sig);
			auto sigVars = TreeItem_GetFunctionTypeVars(sig.get());
			auto typeArgs = TreeItem_GetFunctionParamSigTypeArgs(m_FuncItem, idx);
			if (sigVars && typeArgs && typeArgs->size() == sigVars->size())
			{
				r.varsOwner = m_FuncItem; r.instance = 0;
				r.tok2owner = std::make_shared<std::map<TokenID, TokenID>>();
				for (SizeT k = 0; k != sigVars->size(); ++k)
					(*r.tok2owner)[(*sigVars)[k].first] = (*typeArgs)[k];
			}
			return r;
		}
		if (IsUnit(p))
		{
			DefType r; r.kind = DefType::Kind::UnitVal;
			r.vc = AsUnit(p)->GetValueType();
			r.dom = DefType::Dom::Node;
			// rigid identity (the unit bound at application); the declared class
			// pins the companion class node concretely (batch U)
			r.dNode = UNode(m_FuncItem, 0, p->GetID(), r.vc);
			// K11a-1: a structured unit parameter carries a member block (sub-items).
			// K11a by-example ('nw: network_links'): the parse-time clone carries only
			// the CLASS (ConfigProd.cpp DoRefTypeSignature) -- the retained UNIT
			// exemplar supplies the declared member block instead, typed through the
			// SAME BuildParamMembers ladder (declared kind/type only; exemplar DATA is
			// never read, risk R-b).
			if (p->_GetFirstSubItem())
			{
				r.members = BuildParamMembers(p, r.dNode);
				r.membersComplete = true;
			}
			else if (auto ex = TreeItem_GetFunctionParamTypeExemplar(m_FuncItem, idx); ex && ex->_GetFirstSubItem())
			{
				r.members = BuildParamMembers(p, r.dNode, ex.get());
				r.membersComplete = ExemplarMemberSetIsClosed(ex.get());
				m_Keep.push_back(ex);
			}
			return r;
		}
		if (IsDataItem(p))
			return PositionType(p, m_FuncItem, 0, nullptr, 0, nullptr);
		if (p->IsFunctionItem())
		{
			DefType r; r.kind = DefType::Kind::Func; r.fn = p; // bare 'name: function' / cloned exemplar
			return r;
		}
		// K11a-4: a CONTAINER parameter ('container cfg { … }', or by-example
		// 'cfg: Settings') -- the declared member block types exactly like a
		// structured unit parameter's, except there is no parameter unit: members
		// without an explicit domain DEFER instead of defaulting (NO_TYPE_VAR).
		// A plain member-less TreeItem parameter ('item x' meta-refs and friends)
		// stays deferred.
		{
			DefType r; r.kind = DefType::Kind::Container;
			if (p->_GetFirstSubItem())
			{
				r.members = BuildParamMembers(p, NO_TYPE_VAR);
				r.membersComplete = true;
				return r;
			}
			if (auto ex = TreeItem_GetFunctionParamTypeExemplar(m_FuncItem, idx); ex && ex->_GetFirstSubItem())
			{
				r.members = BuildParamMembers(p, NO_TYPE_VAR, ex.get());
				r.membersComplete = ExemplarMemberSetIsClosed(ex.get());
				m_Keep.push_back(ex);
				return r;
			}
		}
		return {}; // member-less plain parameters: deferred
	}

	// K11a-1: build the member map of a structured unit parameter `p`. Each declared
	// member sub-item is typed: a member UNIT gets a per-instantiation identity node; a
	// member ATTRIBUTE is Data with its declared value class and domain. K11a-1b: a
	// member attribute whose values token names a sibling member unit also carries that
	// unit's IDENTITY node (vuNode) -- so members sharing a node unit (F1,F2 both
	// attribute<nodeset>) unify over the SAME node at definition, and members over
	// different node units fail to unify.
	// K11a-3.1 (generic member types): member tokens resolve through the SAME ladder a
	// positional declaration uses (PositionType), innermost first --
	//   values: sibling member unit → the function's generic variables (a domain-
	//     sorted variable also carries unit identity, the K2 bridge) → ValueClass
	//     name → a telescope unit parameter → a definition-scope unit;
	//   domain: the parameter itself / '.' (the default) → sibling member unit →
	//     generic domain variable → telescope unit parameter → definition-scope
	//     unit (Void broadcasts) → otherwise DEFER (Dom::Unknown).
	// (Review findings, K11a-3.1 round:) generic variables come BEFORE ValueClass
	// names, matching PositionType -- a type variable named like a value class must
	// type members and body items to the SAME rigid node; a member declared without
	// a domain carries the implicit '.' entity token (ConfigProd RetrieveEntity),
	// never an empty one, so '.' selects the parameter-unit default; and member-unit
	// nodes are keyed by the PARAMETER-QUALIFIED token 'p/member' so same-named
	// member units of different structured parameters (or a member unit shadowing a
	// same-named telescope parameter) stay DISTINCT rigid units.
	// An explicit domain token must NEVER silently fall back to the parameter unit:
	// that mistyped `cost (E2)` as over the parameter and falsely rejected a correct
	// body item declared over E2 (rigid-rigid 'nw'≠'E2' conflict) -- unresolvable
	// tokens defer.
	std::shared_ptr<const std::map<SharedStr, DefType, MemberPathLess>>
	FunctionChecker::BuildParamMembers(const TreeItem* p, SizeT paramDomNode, const TreeItem* memberSrc)
	{
		// memberSrc: the item whose DECLARED sub-items form the member block -- the
		// parameter itself (explicit block) or its by-example UNIT exemplar. Node
		// qualification stays on the PARAMETER's name either way (two by-example
		// parameters of one exemplar must still be distinct rigid units).
		// By-example review finding (reproduced): EXEMPLAR member tokens are
		// lexically the EXEMPLAR's world -- they must never resolve against the
		// function's generic variables, telescope parameters, definition scope, or
		// the caller-chosen parameter name (a same-named parameter/scope unit
		// CAPTURED them, falsely rejecting correct programs at definition). In
		// by-example mode the non-sibling rungs are the ValueClass vocabulary and
		// the exemplar's own lexical scope; everything else defers.
		bool byExample = memberSrc != nullptr && memberSrc != p;
		if (!memberSrc)
			memberSrc = p;
		const TreeItem* scopeAnchor = byExample ? memberSrc : m_FuncItem;
		auto members = std::make_shared<std::map<SharedStr, DefType, MemberPathLess>>();
		SharedStr pName(p->GetID().AsStrRange()); // materialized: TokenStr must not span token creation below

		// K11a-4: one WALK per block, recursing into declared CONTAINER members with
		// the member path as prefix -- the map is FLAT, keyed by the full relative
		// path ('meta', 'meta/factor'), so deep member access types directly.
		// blockDomNode is the enclosing unit's node, or NO_TYPE_VAR when the block
		// has no enclosing unit (a container parameter / a nested container block):
		// there, members without an explicit domain DEFER instead of defaulting.
		// Sibling resolution is per block; qualified node tokens carry the full
		// path ('p/meta/subunit').
		std::function<void(const TreeItem*, const SharedStr&, SizeT)> walkBlock;
		walkBlock = [&](const TreeItem* block, const SharedStr& prefix, SizeT blockDomNode)
		{
			auto qualTok = [&](TokenID memberTok) -> TokenID
			{
				SharedStr mName(memberTok.AsStrRange());
				return prefix.empty()
					? GetTokenID_mt(mySSPrintF("{}/{}", pName.c_str(), mName.c_str()).c_str())
					: GetTokenID_mt(mySSPrintF("{}/{}{}", pName.c_str(), prefix.c_str(), mName.c_str()).c_str());
			};
			for (const TreeItem* m = block->_GetFirstSubItem(); m; m = m->GetNextItem())
			{
				// review finding: nested FUNCTIONS, TEMPLATES and type-alias exemplars
				// are implementation content, not members -- they must neither be typed
				// nor (through membersComplete) make a same-named reference an error
				if (m->IsTemplate() || m->IsFunctionItem())
					continue;
				DefType md;
				if (IsUnit(m))
				{
					md.kind = DefType::Kind::UnitVal;
					md.vc   = AsUnit(m)->GetValueType();
					md.dom  = DefType::Dom::Node;
					md.dNode = UNode(m_FuncItem, 0, qualTok(m->GetID()), md.vc);
				}
				else if (IsDataItem(m))
				{
					auto adi = AsDataItem(m);
					md.kind  = DefType::Kind::Data;
					md.vcomp = adi->GetValueComposition();

					if (TokenID vt = adi->ValuesUnitToken())
					{
						bool vMatched = false;
						for (const TreeItem* u = block->_GetFirstSubItem(); u; u = u->GetNextItem())
							if (u->GetID() == vt && IsUnit(u))
							{
								// K11a-1b: the member attribute's values unit IS the sibling
								// member unit -- carry its IDENTITY node, not just its class.
								// The node is keyed by the QUALIFIED token, so it is the SAME
								// node the member unit itself got above. Hence F1,F2 both
								// `attribute<nodeset>` share one node: the body's
								// pcount(nw/F1)+pcount(nw/F2) unifies over the single nodeset
								// domain at definition, and two members over DIFFERENT node
								// units fail to unify.
								md.vc = AsUnit(u)->GetValueType();
								md.vuNode = UNode(m_FuncItem, 0, qualTok(vt), md.vc);
								vMatched = true;
								break;
							}
						if (!vMatched && !byExample && (IsOwnDeclaredVar(m_FuncItem, vt) || IsGenericVarOf(m_FuncItem, vt)))
						{
							// K11a-3.1: `w: attribute<V>` under `<V: numerics>` -- the member's
							// values class IS the function's rigid variable, so body uses of
							// nw/w are checked under ∀ exactly like a positional attribute<V>
							md.vNode = ValNode(m_FuncItem, 0, vt);
							if (IsDomainSortedVarOf(m_FuncItem, vt))
								md.vuNode = UNode(m_FuncItem, 0, vt); // K2: identity through the values role
							vMatched = true;
						}
						if (!vMatched)
							if (auto vc = ValueClass::FindByScriptName(vt))
							{
								md.vc = vc;
								vMatched = true;
							}
						if (!vMatched && !byExample)
						{
							const TreeItem* q = m_FuncItem->_GetFirstSubItem();
							for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(m_FuncItem); j != n && q; ++j, q = q->GetNextItem())
								if (q->GetID() == vt && IsUnit(q))
								{
									// a telescope unit parameter in the VALUES role: its declared
									// class + per-instantiation identity (as PositionType does)
									md.vc = AsUnit(q)->GetValueType();
									md.vuNode = UNode(m_FuncItem, 0, vt, md.vc);
									vMatched = true;
									break;
								}
						}
						if (!vMatched)
							if (auto u = ResolveUnitInScope(vt, scopeAnchor))
							{
								md.vc = AsUnit(u.get())->GetValueType();
								md.vKeep = u; md.vUnit = AsUnit(u.get()); // identity too (batch U)
							}
						// else: unknown values class -- checked per application
					}

					// domain: default = the enclosing unit (or DEFER when the block has
					// none); an explicit token resolves through the ladder or DEFERS
					// (never silently the parameter unit). The block's own name (the
					// exemplar, in the by-example case) also selects the default: inside
					// its declaration that name IS the enclosing unit. The caller-chosen
					// PARAMETER name selects the default only for an explicit member
					// block (an exemplar token never means it).
					TokenID dt = adi->DomainUnitToken();
					if (!dt || dt == t_Dot || dt == block->GetID() || (!byExample && block == memberSrc && dt == p->GetID()))
					{
						if (blockDomNode != NO_TYPE_VAR)
						{
							md.dom = DefType::Dom::Node;
							md.dNode = blockDomNode;
						}
						// else: containers have no member-domain default -- defer
					}
					else
					{
						bool dMatched = false;
						for (const TreeItem* u = block->_GetFirstSubItem(); u; u = u->GetNextItem())
							if (u->GetID() == dt && IsUnit(u))
							{
								// over a sibling member unit -- the SAME (qualified) node that
								// member got
								md.dom = DefType::Dom::Node;
								md.dNode = UNode(m_FuncItem, 0, qualTok(dt), AsUnit(u)->GetValueType());
								dMatched = true;
								break;
							}
						if (!dMatched && !byExample && (IsOwnDeclaredVar(m_FuncItem, dt) || IsGenericVarOf(m_FuncItem, dt)))
						{
							md.dom = DefType::Dom::Node;
							md.dNode = UNode(m_FuncItem, 0, dt); // generic domain variable
							dMatched = true;
						}
						if (!dMatched && !byExample)
						{
							const TreeItem* q = m_FuncItem->_GetFirstSubItem();
							for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(m_FuncItem); j != n && q; ++j, q = q->GetNextItem())
								if (q->GetID() == dt && IsUnit(q))
								{
									// over a telescope unit parameter (UNode self-pins its class)
									md.dom = DefType::Dom::Node;
									md.dNode = UNode(m_FuncItem, 0, dt);
									dMatched = true;
									break;
								}
						}
						if (!dMatched)
							if (auto u = ResolveUnitInScope(dt, scopeAnchor))
							{
								if (AsUnit(u.get())->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
									md.dom = DefType::Dom::Void;
								else
								{
									md.dom = DefType::Dom::Concrete;
									md.domKeep = u; md.domUnit = AsUnit(u.get());
								}
								dMatched = true;
							}
						// !dMatched: md.dom stays Dom::Unknown -- defer
					}
				}
				// else (container / nested-function members): md stays Unknown -- the
				// member IS declared, so it must be IN the map (K11a-3 review finding:
				// dropping it while membersComplete=true made a direct `nw/meta`
				// reference a false "declares no member" definition error)
				SharedStr mName(m->GetID().AsStrRange());
				SharedStr mPath = prefix.empty() ? mName : prefix + mName;
				(*members)[mPath] = md;

				// K11a-4: recurse into a declared CONTAINER member -- its members type
				// under the flattened path ('meta/factor'); nested blocks have no
				// enclosing unit, so their default domains defer. Nested UNIT members'
				// sub-items stay deferred (the argument may carry label attrs etc.).
				if (!IsUnit(m) && !IsDataItem(m) && m->_GetFirstSubItem())
					walkBlock(m, mPath + "/", NO_TYPE_VAR);
			}
		};
		walkBlock(memberSrc, SharedStr(), paramDomNode);
		return members;
	}

	// K11b: type the members of a CONCRETE container (a definition-scope item passed
	// as an operator's ArgContainer argument). Unlike a parameter's member block these
	// members are real items, so their types are CONCRETE: a member unit is itself the
	// unit, and a member attribute's domain/values tokens resolve in the CONTAINER's
	// OWN scope (never the function's -- the by-example capture-shadowing lesson).
	// Members whose tokens do not resolve stay Unknown and simply defer.
	std::shared_ptr<const std::map<SharedStr, DefType, MemberPathLess>>
	FunctionChecker::BuildConcreteContainerMembers(const TreeItem* c)
	{
		auto members = std::make_shared<std::map<SharedStr, DefType, MemberPathLess>>();
		for (const TreeItem* m = c->_GetFirstSubItem(); m; m = m->GetNextItem())
		{
			DefType md;
			if (IsUnit(m))
			{
				md.kind = DefType::Kind::UnitVal;
				md.vc = AsUnit(m)->GetValueType();
				if (AsUnit(m)->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
					md.dom = DefType::Dom::Void;
				else
				{
					md.dom = DefType::Dom::Concrete;
					md.domKeep = m->shared_from_this(); md.domUnit = AsUnit(m);
				}
			}
			else if (IsDataItem(m))
			{
				auto adi = AsDataItem(m);
				md.kind = DefType::Kind::Data;
				md.vcomp = adi->GetValueComposition();
				if (TokenID vt = adi->ValuesUnitToken())
				{
					if (auto vc = ValueClass::FindByScriptName(vt))
						md.vc = vc;
					else if (auto u = ResolveUnitInScope(vt, c))
					{
						md.vc = AsUnit(u.get())->GetValueType();
						md.vKeep = u; md.vUnit = AsUnit(u.get());
					}
				}
				TokenID dt = adi->DomainUnitToken();
				if (!dt || dt == t_Dot || dt == c->GetID())
					; // a container has no enclosing unit: an implicit domain defers
				else if (auto u = ResolveUnitInScope(dt, c))
				{
					if (AsUnit(u.get())->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
						md.dom = DefType::Dom::Void;
					else
					{
						md.dom = DefType::Dom::Concrete;
						md.domKeep = u; md.domUnit = AsUnit(u.get());
					}
				}
			}
			// else: nested containers & co stay Unknown (declared, but untyped here)
			(*members)[SharedStr(m->GetID().AsStrRange())] = md;
		}
		return members;
	}

	// K11a-2: type `P/member` access at definition against the structured parameter's
	// member map. Hit → the member's type (so the body's use of it is checked under ∀).
	// K11a-3: a DIRECT-member miss under a COMPLETE declared interface is a
	// definition-time error -- the member block is the parameter's declared contract
	// (§4.6 strict scope), so a body reference outside it is wrong regardless of what
	// extra members an argument happens to provide. DEEP paths defer (the argument may
	// legitimately carry structure BELOW a declared member, e.g. nw/nodeset/label);
	// non-structured parameters defer as before (the per-application SubItem check runs).
	DefType FunctionChecker::InferParamMember(UInt32 paramIdx, const SharedStr& memberPath)
	{
		DefType pt = ParamType(paramIdx);
		if (pt.members)
		{
			if (auto it = pt.members->find(memberPath); it != pt.members->end())
				return it->second;
			if (pt.membersComplete)
			{
				// a DIRECT miss is an error; a DEEP miss is an error only when its
				// parent path is a declared container block we WALKED (K11a-4: the
				// flat map then holds that block's complete member set, so
				// 'cfg/nested/wrong' is as wrong as a direct miss). A deep path below
				// a UNIT member or an unwalked item still defers: the argument may
				// legitimately carry sub-structure there (label attributes, a
				// composite rule's generated members).
				auto slash = std::find(memberPath.begin(), memberPath.send(), '/');
				bool report = slash == memberPath.send();
				if (!report)
				{
					CharPtr lastSlash = memberPath.send();
					for (CharPtr q = memberPath.begin(); q != memberPath.send(); ++q)
						if (*q == '/')
							lastSlash = q;
					// NB: CharPtrRange, NOT (begin, end) -- SharedStr has no two-pointer
					// ctor, so that silently binds to SharedStr(zStr, debugSrcName) and
					// yields the WHOLE path
					SharedStr parentPath{ CharPtrRange(memberPath.begin(), lastSlash) };
					SharedStr parentPrefix = parentPath + "/";
					if (auto pit = pt.members->find(parentPath); pit != pt.members->end()
						&& pit->second.kind != DefType::Kind::Data && pit->second.kind != DefType::Kind::UnitVal)
						for (const auto& kv : *pt.members)
							if (kv.first.ssize() > parentPrefix.ssize()
								&& std::equal(parentPrefix.begin(), parentPrefix.send(), kv.first.begin()))
							{
								report = true; // the parent block was walked: its member set is closed
								break;
							}
				}
				if (report)
				{
					SharedStr pName(m_Params[paramIdx]->GetID().AsStrRange());
					throwErrorF("ExprParser", "the definition of '{}': parameter '{}' declares no member '{}'"
						, m_FuncItem->GetFullName().c_str(), pName.c_str(), memberPath.c_str());
				}
			}
		}
		return {};
	}

	// the declared annotation of a body item (or result); Unknown when undeclared
	DefType FunctionChecker::DeclaredItemType(const TreeItem* item)
	{
		if (IsDataItem(item))
			return PositionType(item, m_FuncItem, 0, nullptr, 0, nullptr, item->GetTreeParent().get());
		return {}; // units, containers, nested functions: no data annotation to check
	}

	// the declared type of one position (parameter or result declaration) of fnDef,
	// instantiated for one application under `instance`. Token resolution order:
	// the type-application translation (tok2owner -> varsOwner's variables), the
	// varsOwner's own variables (nested results reference their origin's variables),
	// fnDef's own generic variables, fnDef's unit parameters (per-instantiation
	// identity), and finally fnDef's definition scope (concrete units).
	DefType FunctionChecker::PositionType(const TreeItem* posItem, const TreeItem* fnDef, UInt32 instance,
		const TreeItem* ownerFn, UInt32 ownerInstance, const std::map<TokenID, TokenID>* tok2owner,
		const TreeItem* itemScope)
	{
		DefType r;
		if (!posItem)
			return r;
		if (IsUnit(posItem))
		{
			r.kind = DefType::Kind::UnitVal;
			r.vc = AsUnit(posItem)->GetValueType();
			r.dom = DefType::Dom::Node;
			r.dNode = UNode(fnDef, instance, posItem->GetID(), r.vc);
			return r;
		}
		if (!IsDataItem(posItem))
			return r; // container/typed-by-example positions: deferred

		r.kind = DefType::Kind::Data;
		auto adi = AsDataItem(posItem);
		r.vcomp = adi->GetValueComposition();

		if (TokenID vTok = adi->ValuesUnitToken())
		{
			if (tok2owner)
				if (auto it = tok2owner->find(vTok); it != tok2owner->end())
				{
					vTok = it->second, r.vNode = ValNode(ownerFn, ownerInstance, vTok);
					if (IsDomainSortedVarOf(ownerFn, vTok))
						r.vuNode = UNode(ownerFn, ownerInstance, vTok); // K2 identity through the sig binding too
				}
			if (r.vNode == NO_TYPE_VAR)
			{
				// an own <...> clause shadows the origin's variables; unmapped tokens
				// of a type application (tok2owner set) belong to fnDef's own lexical
				// world and never resolve to the origin's variables.
				// batch U: a values token naming a DOMAIN-SORTED generic or a unit
				// PARAMETER additionally carries the unit's IDENTITY (vuNode) -- the
				// SAME node its domain role uses, which is the K2 bridge; a token
				// resolving to a concrete scope unit carries that unit (vUnit)
				if (IsOwnDeclaredVar(fnDef, vTok))
				{
					r.vNode = ValNode(fnDef, instance, vTok);
					if (IsDomainSortedVarOf(fnDef, vTok))
						r.vuNode = UNode(fnDef, instance, vTok);
				}
				else if (!tok2owner && ownerFn && IsGenericVarOf(ownerFn, vTok))
				{
					r.vNode = ValNode(ownerFn, ownerInstance, vTok);
					if (IsDomainSortedVarOf(ownerFn, vTok))
						r.vuNode = UNode(ownerFn, ownerInstance, vTok);
				}
				else if (IsGenericVarOf(fnDef, vTok))
				{
					r.vNode = ValNode(fnDef, instance, vTok);
					if (IsDomainSortedVarOf(fnDef, vTok))
						r.vuNode = UNode(fnDef, instance, vTok);
				}
				else if (auto vc = ValueClass::FindByScriptName(vTok))
					r.vc = vc;
				else if (itemScope && HasBodyShadower(vTok, itemScope))
					; // a body-local declaration shadows the outer name: defer
				else
				{
					// a unit parameter of fnDef in the VALUES role: per-instantiation
					// identity + the class its declaration pins (`unit<uint32> U`)
					const TreeItem* q = fnDef->_GetFirstSubItem();
					for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(fnDef); j != n && q; ++j, q = q->GetNextItem())
						if (q->GetID() == vTok && IsUnit(q))
						{
							r.vc = AsUnit(q)->GetValueType();
							r.vuNode = UNode(fnDef, instance, vTok, r.vc);
							break;
						}
					if (r.vuNode == NO_TYPE_VAR && r.vc == nullptr)
						if (auto u = ResolveUnitInScope(vTok, fnDef))
						{
							r.vc = AsUnit(u.get())->GetValueType(); // kinds-level class
							r.vKeep = u; r.vUnit = AsUnit(u.get()); // + identity (batch U)
						}
					// else: unknown values class (checked per application)
				}
			}
		}

		if (TokenID dTok = adi->DomainUnitToken())
		{
			if (tok2owner)
				if (auto it = tok2owner->find(dTok); it != tok2owner->end())
					dTok = it->second, r.dom = DefType::Dom::Node, r.dNode = UNode(ownerFn, ownerInstance, dTok);
			if (r.dom == DefType::Dom::Unknown)
			{
				if (IsOwnDeclaredVar(fnDef, dTok))
				{
					r.dom = DefType::Dom::Node; r.dNode = UNode(fnDef, instance, dTok);
				}
				else if (!tok2owner && ownerFn && IsGenericVarOf(ownerFn, dTok))
				{
					r.dom = DefType::Dom::Node; r.dNode = UNode(ownerFn, ownerInstance, dTok);
				}
				else if (IsGenericVarOf(fnDef, dTok))
				{
					r.dom = DefType::Dom::Node; r.dNode = UNode(fnDef, instance, dTok);
				}
				else if (itemScope && HasBodyShadower(dTok, itemScope))
				{ /* a body-local declaration shadows the outer name: defer */ }
				else
				{
					// a unit parameter of fnDef: its per-instantiation identity
					const TreeItem* q = fnDef->_GetFirstSubItem();
					for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(fnDef); j != n && q; ++j, q = q->GetNextItem())
						if (q->GetID() == dTok && IsUnit(q))
						{
							r.dom = DefType::Dom::Node; r.dNode = UNode(fnDef, instance, dTok, AsUnit(q)->GetValueType());
							break;
						}
					if (r.dom == DefType::Dom::Unknown)
						if (auto u = ResolveUnitInScope(dTok, fnDef))
						{
							if (AsUnit(u.get())->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
								r.dom = DefType::Dom::Void;
							else
							{
								r.dom = DefType::Dom::Concrete; r.domKeep = u; r.domUnit = AsUnit(u.get());
							}
						}
				}
			}
		}
		return r;
	}

	void FunctionChecker::UnifyData(const DefType& a, const DefType& b, const SharedStr& srcA, const SharedStr& srcB)
	{
		if (a.kind == DefType::Kind::Unknown || b.kind == DefType::Kind::Unknown)
			return; // deferred to per-application checking
		if (a.kind != b.kind || a.kind == DefType::Kind::Func)
			return; // kind confusion and function-value conformance are handled at binding sites

		// value positions
		if (a.vNode != NO_TYPE_VAR && b.vNode != NO_TYPE_VAR)
			m_Unifier.LinkValue(a.vNode, b.vNode, srcB);
		else if (a.vNode != NO_TYPE_VAR && b.vc)
			m_Unifier.BindValue(a.vNode, b.vc, srcB);
		else if (b.vNode != NO_TYPE_VAR && a.vc)
			m_Unifier.BindValue(b.vNode, a.vc, srcA);
		else if (a.vc && b.vc && a.vc != b.vc)
			throwErrorF("ExprParser", "the definition of '{}': {} ({}) does not match {} ({})"
				, m_FuncItem->GetFullName().c_str()
				, a.vc->GetName().c_str(), srcA.c_str(), b.vc->GetName().c_str(), srcB.c_str());

		// values-unit IDENTITY (batch U): declared identities through unit
		// parameters and domain-sorted generics -- the function-signature K2
		// bridge (`attribute<E> rel (D); attribute<V> vals (E)` flows both roles
		// of E through ONE unit node). Terms without identity information defer;
		// concrete pairs compare by defining-expression identity under the
		// checker's total-symmetric mode, exactly like domains below
		if (a.kind == DefType::Kind::Data && b.kind == DefType::Kind::Data)
		{
			if (a.vuNode != NO_TYPE_VAR && b.vuNode != NO_TYPE_VAR)
				m_Unifier.LinkUnit(a.vuNode, b.vuNode, srcB);
			else if (a.vuNode != NO_TYPE_VAR && b.vUnit)
				m_Unifier.BindUnit(a.vuNode, b.vKeep, b.vUnit, srcB);
			else if (b.vuNode != NO_TYPE_VAR && a.vUnit)
				m_Unifier.BindUnit(b.vuNode, a.vKeep, a.vUnit, srcA);
			// concrete-vs-concrete deliberately DEFERS (S1, review finding): reduction
			// checks values units by UnifyValues (class + metric, AllowDefaultLeft) --
			// two key-distinct metric-less units of one class unify there, so a
			// key-identity error here would reject configs that reduce fine. Identity
			// is enforced only through a declared unit-variable contract (the arms
			// above), a surface that did not resolve before batch U.
		}

		// domain positions (void broadcasts; unknown defers)
		using Dom = DefType::Dom;
		if (a.dom == Dom::Unknown || b.dom == Dom::Unknown || a.dom == Dom::Void || b.dom == Dom::Void)
			return;
		if (a.dom == Dom::Node && b.dom == Dom::Node)
			m_Unifier.LinkUnit(a.dNode, b.dNode, srcB);
		else if (a.dom == Dom::Node && b.dom == Dom::Concrete)
			m_Unifier.BindUnit(a.dNode, b.domKeep, b.domUnit, srcB);
		else if (b.dom == Dom::Node && a.dom == Dom::Concrete)
			m_Unifier.BindUnit(b.dNode, a.domKeep, a.domUnit, srcA);
		else if (a.dom == Dom::Concrete && b.dom == Dom::Concrete)
			if (!a.domUnit->UnifyDomain(b.domUnit, "", "", TypeUnifier::s_CheckerUM))
				throwErrorF("ExprParser", "the definition of '{}': the domain of {} differs from the domain of {}"
					, m_FuncItem->GetFullName().c_str(), srcA.c_str(), srcB.c_str());
	}

	// type one application: infer/validate all arguments, unify them against the
	// applied function's declared parameters under a fresh instance, and return the
	// declared result type under that instance
	DefType FunctionChecker::InferApplication(const TreeItem* refScope, const DefType& fnVal, LispPtr argsList, CharPtr headName)
	{
		std::vector<DefType> argTerms;
		UInt32 nrArgs = 0; bool anyHole = false;
		for (LispPtr a = argsList; !a.EndP(); a = a.Right())
		{
			bool hole = a.Left().IsSymb() && a.Left().GetSymbID() == t_Hole;
			anyHole |= hole;
			argTerms.push_back(hole ? DefType{} : InferArg(refScope, a.Left()));
			++nrArgs;
		}
		if (fnVal.kind != DefType::Kind::Func || !fnVal.fn)
			return {};
		const TreeItem* fnDef = fnVal.fn;
		if (TreeItem_IsFunctionVariantSet(fnDef))
			return {}; // variant selection is argument-class-dependent: per application
		if (TreeItem_HasFunctionRestParam(fnDef))
			return {}; // '...x' variadic: the rest binding is per application (splice + fold)
		if (!TreeItem_GetFunctionResultName(fnDef))
			return {}; // no declared signature (bare 'name: function' values): per application
		UInt32 nrParams = TreeItem_GetFunctionParamCount(fnDef);
		if (!anyHole && nrArgs != nrParams)
			throwErrorF("ExprParser", "'{}': function '{}' expects {} argument(s); {} provided"
				, headName, fnDef->GetFullName().c_str(), nrParams, nrArgs);
		if (anyHole)
			return {}; // partial application: residual arity and types per application

		UInt32 instance = m_NextInstance++;
		const TreeItem* ownerFn = fnVal.varsOwner;
		UInt32 ownerInstance = fnVal.instance;
		const std::map<TokenID, TokenID>* t2o = fnVal.tok2owner.get();

		const TreeItem* p = fnDef->_GetFirstSubItem();
		for (UInt32 k = 0; k != nrParams && p; ++k, p = p->GetNextItem())
		{
			SharedStr pName(p->GetID().AsStrRange()); // materialized: TokenStr temporaries must not span nested walks (token-registry lock)
			if (auto declaredSig = TreeItem_GetFunctionParamSignature(fnDef, k))
			{
				// signature-typed parameter: a plain function reference with a declared
				// signature is checked and linked here; passed-through values without
				// one (bare 'name: function') and partial bindings defer
				if (argTerms[k].kind == DefType::Kind::Func && argTerms[k].fn && !argTerms[k].varsOwner
					&& argTerms[k].fn->IsFunctionItem() && TreeItem_GetFunctionResultName(argTerms[k].fn))
				{
					const TreeItem* bound = argTerms[k].fn;
					CheckFunctionSignature(bound, declaredSig.get(), pName.c_str());
					auto sigVars = TreeItem_GetFunctionTypeVars(declaredSig.get());
					auto typeArgs = TreeItem_GetFunctionParamSigTypeArgs(fnDef, k);
					if (sigVars && typeArgs && typeArgs->size() == sigVars->size())
					{
						SharedStr bindSource = mySSPrintF("function '{}' bound to parameter '{}' of '{}'"
							, bound->GetFullName().c_str(), pName.c_str(), fnDef->GetFullName().c_str());
						// per type-application argument, the target variable may belong to
						// fnDef (fresh instance) or, through the value's origin, to ownerFn
						auto targetV = [&](TokenID t) -> SizeT
						{
							if (t2o) if (auto it = t2o->find(t); it != t2o->end()) return ValNode(ownerFn, ownerInstance, it->second);
							if (IsOwnDeclaredVar(fnDef, t)) return ValNode(fnDef, instance, t);
							if (!t2o && ownerFn && IsGenericVarOf(ownerFn, t)) return ValNode(ownerFn, ownerInstance, t);
							if (IsGenericVarOf(fnDef, t)) return ValNode(fnDef, instance, t);
							return NO_TYPE_VAR;
						};
						auto targetD = [&](TokenID t) -> SizeT
						{
							if (t2o) if (auto it = t2o->find(t); it != t2o->end()) return UNode(ownerFn, ownerInstance, it->second);
							if (IsOwnDeclaredVar(fnDef, t)) return UNode(fnDef, instance, t);
							if (!t2o && ownerFn && IsGenericVarOf(ownerFn, t)) return UNode(ownerFn, ownerInstance, t);
							if (IsGenericVarOf(fnDef, t)) return UNode(fnDef, instance, t);
							return NO_TYPE_VAR;
						};
						LinkSignatureBinding(m_Unifier, declaredSig.get(), bound, sigVars, typeArgs
							, targetV, targetD, m_NextInstance++, bindSource);
					}
				}
				else if (argTerms[k].kind != DefType::Kind::Unknown && argTerms[k].kind != DefType::Kind::Func)
					throwErrorF("ExprParser", "the definition of '{}': parameter '{}' of function '{}' requires a function argument"
						, m_FuncItem->GetFullName().c_str(), pName.c_str(), fnDef->GetFullName().c_str());
				continue;
			}
			DefType pT = PositionType(p, fnDef, instance, ownerFn, ownerInstance, t2o);
			SharedStr argSrc = mySSPrintF("argument {} of '{}'", k + 1, headName);
			SharedStr parSrc = mySSPrintF("parameter '{}' of function '{}'", pName.c_str(), fnDef->GetFullName().c_str());
			UnifyData(argTerms[k], pT, argSrc, parSrc);
		}

		auto resultChild = fnDef->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(fnDef));
		if (!resultChild)
			return {};
		if (resultChild->IsFunctionItem())
		{
			if (t2o || ownerFn)
				return {}; // deeper chains of function-valued results: per application
			DefType r; r.kind = DefType::Kind::Func; r.fn = resultChild.get();
			r.varsOwner = fnDef; r.instance = instance; // its positions reference fnDef's variables
			m_Keep.push_back(resultChild);
			return r;
		}
		return PositionType(resultChild.get(), fnDef, instance, ownerFn, ownerInstance, t2o);
	}

	// argument-position inference: function references become Func values (mirroring
	// ResolveBodyArg); container literals defer; everything else infers as expression
	DefType FunctionChecker::InferArg(const TreeItem* refScope, LispPtr argExpr)
	{
		if (argExpr.IsSymb())
		{
			TokenID sym = argExpr.GetSymbID();
			if (sym == t_Hole || token::isConst(sym) || ValueClass::FindByScriptName(sym))
				return {};
			SharedStr s(sym.AsStrRange());
			if (std::find(s.begin(), s.send(), '/') == s.send())
			{
				// function-typed parameters short-circuit by name (mirroring the
				// reducer's binding lookup in ResolveBodyArg); DATA parameters do
				// NOT -- a nearer body local may shadow them (nearest-scope, exactly
				// like ResolveBodySymbol resolves the argument at reduction)
				if (auto headChild = m_FuncItem->GetConstSubTreeItemByID(sym))
					for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
						if (m_Params[i] == headChild.get()
							&& (TreeItem_GetFunctionParamSignature(m_FuncItem, i) || m_Params[i]->IsFunctionItem()))
							return ParamType(i);
				for (const TreeItem* scope = refScope; scope; scope = scope->GetTreeParent().get())
				{
					if (auto child = scope->GetConstSubTreeItemByID(sym))
					{
						for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
							if (m_Params[i] == child.get())
								return ParamType(i);
						if (child->IsFunctionItem())
						{
							DefType r; r.kind = DefType::Kind::Func; r.fn = child.get();
							m_Keep.push_back(child);
							return r;
						}
						return InferBodyItem(child.get());
					}
					if (scope == m_FuncItem)
						break;
				}
				auto fnRef = m_FuncItem->FindItem(s);
				if (!fnRef || !fnRef->IsFunctionItem())
					if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
						if (auto lex = defParent->FindItem(s); lex && lex->IsFunctionItem())
							fnRef = lex;
				if (!fnRef || !fnRef->IsFunctionItem())
					if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
						fnRef = pf;
				if (fnRef && fnRef->IsFunctionItem())
				{
					DefType r; r.kind = DefType::Kind::Func; r.fn = fnRef.get();
					m_Keep.push_back(fnRef);
					return r;
				}
			}
		}
		if (argExpr.IsRealList() && argExpr.Left().IsSymb() && argExpr.Left().GetSymbID() == t_ContainerLiteral)
			return {}; // §5.9 literal: members ('.'-rebound) are resolved and checked at reduction
		return InferExpr(refScope, argExpr);
	}

	// ---- operator signatures, batch A: described group records type applications ----
	//
	// The walker consumes AbstrOperGroup::GetSignatures() (OperSignature.h). Per
	// application it (1) filters the group's members by arity and by the argument
	// classes it knows -- a concrete class, a node's binding, or a node's (hard)
	// feasible set, each an over-approximation of the classes any successful
	// reduction can present, so elimination is sound; (2) applies the unique
	// surviving merged record: the shared domain variable with void broadcast and
	// one class node per record variable; and (3) derives the cross-position class
	// relations from the record's member TUPLES -- positions on which ALL tuples
	// agree are LINKED (hard: exactly the old shared-node semantics, so e.g.
	// mul(x:V, y:W) with independent rigids still errors), and positions all
	// tuples pin to one class are BOUND, but never onto a rigid ∀-variable
	// (support sets are soft, see ConstraintRec). Mixed survivor sets, undescribed
	// survivors, and arity mismatches DEFER -- FindOper's widening escape hatches
	// and per-application checking stay in charge, so a description can only ADD
	// judgments where the membership is unambiguous.

	// the witness classes of one inferred argument term; result = could not tell
	enum class WitnessKind : UInt8 { None, Concrete, Feasible };
	WitnessKind ArgWitnesses(TypeUnifier& u, const DefType& t, ValueClassSet& r)
	{
		if (t.kind != DefType::Kind::Data && t.kind != DefType::Kind::UnitVal)
			return WitnessKind::None;
		if (t.vc)
		{
			r.reset(); r.set(UInt32(t.vc->GetValueClassID()));
			return WitnessKind::Concrete;
		}
		if (t.vNode != NO_TYPE_VAR)
		{
			const auto& n = u.m_ValueNodes[u.FindV(t.vNode)];
			if (n.bound)
			{
				r.reset(); r.set(UInt32(n.bound->GetValueClassID()));
				return WitnessKind::Concrete;
			}
			if (!n.feasible.all())
			{
				r = n.feasible;
				return WitnessKind::Feasible;
			}
		}
		return WitnessKind::None;
	}

	// can witness class `w` present itself to a member position registered as
	// `argCls`? (§18.2: item-free class synthesis; Find, never FindCertain)
	// a known composition restricts the synthesis to that composition's class
	bool WitnessMatchesArgClass(const ValueClass* w, bool isUnitArg, const Class* argCls, ValueComposition knownComp)
	{
		auto uc = UnitClass::Find(w);
		if (!uc)
			return false;
		if (isUnitArg)
			return uc->IsDerivedFrom(argCls);
		static const ValueComposition s_Comps[3] = { ValueComposition::Single, ValueComposition::Polygon, ValueComposition::Sequence };
		for (auto comp : s_Comps)
		{
			if (knownComp != ValueComposition::Unknown && comp != knownComp)
				continue;
			auto vt = uc->GetValueType(comp);
			if (!vt)
				continue;
			auto dic = DataItemClass::Find(vt);
			if (dic && dic->IsDerivedFrom(argCls))
				return true;
		}
		return false;
	}

	bool MemberAcceptsArity(const AbstrOperGroup* og, const Operator* m, arg_index nrArgs)
	{
		arg_index ns = m->NrSpecifiedArgs(), req = ns - m->NrOptionalArgs();
		if (og->AllowExtraArgs())
			return nrArgs >= req;
		return req <= nrArgs && nrArgs <= ns;
	}

	// sound elimination on the REGISTERED classes (described or not).
	// Survives: no known argument class rules the member out.
	// EliminatedConcrete: some position with a CONCRETE class rejects it -- the
	// same classes reach reduction, so FindOper is certain to reject it there too.
	// EliminatedFeasible: only feasible-SET witnesses reject it -- symbolic
	// knowledge, so the no-candidate verdict must defer, not error (a rejecting
	// concrete position elsewhere still upgrades the member to Concrete: the
	// scan continues past a feasible rejection looking for one).
	enum class MemberVerdict : UInt8 { Survives, EliminatedConcrete, EliminatedFeasible };
	MemberVerdict ClassifyMember(TypeUnifier& u, const Operator* m, const std::vector<DefType>& argTerms)
	{
		auto verdict = MemberVerdict::Survives;
		arg_index ns = m->NrSpecifiedArgs();
		for (arg_index i = 0, ie = std::min<arg_index>(ns, arg_index(argTerms.size())); i != ie; ++i)
		{
			ValueClassSet w;
			WitnessKind wk = ArgWitnesses(u, argTerms[i], w);
			if (wk == WitnessKind::None)
				continue;
			auto argCls = m->GetArgClass(i);
			if (!argCls)
				continue;
			bool isUnitArg = argTerms[i].kind == DefType::Kind::UnitVal;
			ValueComposition knownComp = argTerms[i].kind == DefType::Kind::Data ? argTerms[i].vcomp : ValueComposition::Unknown;
			if (knownComp == ValueComposition::MultiPoint)
				knownComp = ValueComposition::Sequence; // folded onto one sequence class (§18.2)
			bool any = false;
			for (UInt32 v = 0; v != UInt32(ValueClassID::VT_Count) && !any; ++v)
				if (w.test(v))
					if (auto wc = ValueClass::FindByValueClassID(ValueClassID(v)))
						any = WitnessMatchesArgClass(wc, isUnitArg, argCls, knownComp);
			if (!any)
			{
				if (wk == WitnessKind::Concrete)
					return MemberVerdict::EliminatedConcrete;
				verdict = MemberVerdict::EliminatedFeasible;
			}
		}
		return verdict;
	}

	// K11b: consume an ArgContainer position -- the members a container argument
	// contributes are unified against the shared domain/values variables the
	// description declares, so a container that disagrees with another argument
	// bound to the same variable (e.g. discrete_alloc's allocUnit) is a
	// DEFINITION-time error instead of a reduction-time one.
	//
	// THE CONSUMED SET IS NAME-DIRECTED (review finding, reproduced). Operators read
	// a SUBSET of the container: discrete_alloc looks each suitability up by type
	// NAME (`GetConstSubTreeItemByID(gg->m_NameID)`), so a container carrying further
	// members -- a per-type weight, a regional helper -- is legitimate and already
	// exercised in tst (`source/Compacted/SuitabilityMaps` has 3 members for 2 type
	// names). An "every member" claim therefore FALSELY rejects working configs, as a
	// same-file control proved: the top-level call reduces while the function-body
	// call was rejected. So the claim applies to the NAMED members only, and only
	// when those names are definition-time knowable -- the `namesPos` string array,
	// evaluated exactly like the §12.7 for_each tranche evaluates its name arrays.
	// No evaluable name array ⇒ NO claim (defer), which is the honest verdict when
	// the member set is data-directed.
	//
	// Two argument shapes are typed: a structured/by-example CONTAINER PARAMETER
	// (K11a-4 already built its member map) and a definition-scope CONTAINER item
	// (members enumerated concretely here). Anything else -- expressions, generated
	// containers, closure captures -- defers exactly as before.
	void FunctionChecker::LinkContainerArg(const SignatureRecord::Pos& p, const DefType& argTerm, LispPtr argExpr,
		const TreeItem* refScope, const SharedStr& argSrc, LispPtr argsList,
		const std::function<SizeT(sig_var)>& VN, const std::function<SizeT(sig_var)>& DN)
	{
		if (p.domain == no_sig_var && p.values == no_sig_var)
			return; // a purely descriptive container position: nothing to link
		if (p.namesPos == arg_index(-1) || !refScope)
			return; // the consumed member set is unknown: claim nothing
		// the names array: CLOSED over the formals, or nothing is claimed
		LispPtr namesExpr;
		{
			arg_index j = 0;
			for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++j)
				if (j == p.namesPos)
				{
					namesExpr = a.Left();
					break;
				}
		}
		if (namesExpr.EndP())
			return;
		auto names = EvalClosedStrArray(refScope, namesExpr);
		if (!names || names->empty())
			return; // data-directed member set: defer, exactly as before K11b
		std::shared_ptr<const std::map<SharedStr, DefType, MemberPathLess>> members;
		bool fromExternal = false;
		if (argTerm.kind == DefType::Kind::Container && argTerm.members)
			members = argTerm.members;                       // a structured container parameter
		else if (argExpr.IsSymb())
		{
			// a definition-scope container reference: enumerate its declared members
			const TreeItem* local = nullptr; UInt32 paramIdx = 0;
			SharedTreeItem ext; ExtRefKind extKind = ExtRefKind::DefScopeExternal; SharedStr genSubPath;
			int code = 0;
			try {
				code = ResolveName(refScope, argExpr.GetSymbID(), &local, &paramIdx, &ext, &extKind, &genSubPath);
			}
			catch (...) {
				return; // an unresolvable argument is reported by its own walk
			}
			const TreeItem* c = nullptr;
			if (code == 2 && extKind == ExtRefKind::DefScopeExternal && ext)
				c = ext.get();
			else if (code == 1)
				c = local;                                    // a body-local container
			if (!c || !IsPlainContainer(c) || !c->_GetFirstSubItem() || !ExemplarMemberSetIsClosed(c))
				return;                                       // not an enumerable container: defer
			if (code == 2 && ext)
				m_Keep.push_back(ext);
			members = BuildConcreteContainerMembers(c);
			fromExternal = true;
		}
		if (!members)
			return;

		// A definition-scope container's members are CONCRETE, and definition-scope
		// externals deliberately DEFER (their types are checked per application) --
		// binding the operator's shared variable to a concrete member unit would pin
		// a rigid unit parameter and falsely reject a body that today type-checks
		// (verified: `attribute<V> x (cells) := SomeExternal;` is accepted precisely
		// because the external defers). So for an external argument only the
		// INTRA-container fact is claimed: the NAMED member attributes must agree with
		// EACH OTHER on one domain. A container PARAMETER's members carry variables,
		// so there the full cross-argument link runs -- the ∀ payoff.
		SharedStr refName; const DefType* refMember = nullptr;
		for (const auto& nm : *names)
		{
			auto it = members->find(nm);
			if (it == members->end())
				continue; // a named member the argument does not declare: reduction reports it
			const DefType& mt = it->second;
			if (mt.kind != DefType::Kind::Data)
				continue; // only member ATTRIBUTES carry the shared domain/values
			SharedStr memberSrc = mySSPrintF("member '{}' of {}", nm.c_str(), argSrc.c_str());
			if (fromExternal)
			{
				if (p.domain == no_sig_var || mt.dom == DefType::Dom::Unknown)
					continue;
				if (!refMember)
				{
					refName = nm; refMember = &mt;
					continue;
				}
				DefType a; a.kind = DefType::Kind::Data;
				a.dom = refMember->dom; a.dNode = refMember->dNode; a.domKeep = refMember->domKeep; a.domUnit = refMember->domUnit;
				DefType b; b.kind = DefType::Kind::Data;
				b.dom = mt.dom; b.dNode = mt.dNode; b.domKeep = mt.domKeep; b.domUnit = mt.domUnit;
				UnifyData(a, b, mySSPrintF("member '{}' of {}", refName.c_str(), argSrc.c_str()), memberSrc);
				continue;
			}
			if (p.domain != no_sig_var && mt.dom != DefType::Dom::Unknown)
			{
				DefType want; want.kind = DefType::Kind::Data;
				want.dom = DefType::Dom::Node; want.dNode = DN(p.domain);
				DefType got; got.kind = DefType::Kind::Data;
				got.dom = mt.dom; got.dNode = mt.dNode; got.domKeep = mt.domKeep; got.domUnit = mt.domUnit;
				UnifyData(got, want, memberSrc, argSrc);
			}
			if (p.values != no_sig_var && (mt.vc || mt.vUnit || mt.vNode != NO_TYPE_VAR))
			{
				DefType want; want.kind = DefType::Kind::Data;
				want.vNode = VN(p.values);
				DefType got; got.kind = DefType::Kind::Data;
				got.vc = mt.vc; got.vNode = mt.vNode; got.vKeep = mt.vKeep; got.vUnit = mt.vUnit;
				UnifyData(got, want, memberSrc, argSrc);
			}
		}
	}

	// §6.2 cross-record fallback. When several congruence records survive, the
	// walker used to defer wholesale: no single record's class tuples apply. But a
	// group's records often still AGREE on the DOMAIN SKELETON -- which positions are
	// attributes/units and which of them share a domain -- and that part holds no
	// matter which member reduction ultimately selects, so claiming it is sound.
	//
	// This is the gate the K11a-1b note called out: `add`/`+` splits into three
	// records (spolygon/ipolygon, dpolygon/fpolygon, and the scalar family), and
	// with classless arguments none can be eliminated. All three nevertheless say
	// "both arguments and the result live on ONE domain", so a body adding two
	// attributes over DIFFERENT domains is now rejected at the definition.
	//
	// The synthesized record claims ONLY that structure: every position gets a FRESH
	// values variable (no cross-position class identity, no metric or value-class
	// relations), the domain variables are the canonical ones all records share, and
	// a value composition is claimed only where every record agrees. The empty tuple
	// list makes ApplyOperRecord's soft support sets, class pins and agreement links
	// no-ops, so the application reduces to pure domain unification.
	std::optional<OperGroupSignatures::MergedRecord>
	FunctionChecker::BuildDomainSkeletonRecord(const OperGroupSignatures* sigs, const std::vector<Int32>& recordIdxs)
	{
		if (recordIdxs.size() < 2)
			return std::nullopt;
		for (Int32 ri : recordIdxs)
			if (ri < 0)
				return std::nullopt; // an UNDESCRIBED member survives: nothing is known

		// canonicalize one record: per position (kind, domain-slot, composition),
		// where a domain slot numbers the domain variables in first-seen order and
		// carries the variable's flags (void/generated domains are distinct claims).
		// K11 leftover (2026-07-29): Container positions are canonicalized too (their
		// shared member domain slot, whether a shared member VALUES var exists, and
		// the naming argument), so multi-record groups with a container argument can
		// still agree on the skeleton; Deferred/MetaValue positions canonicalize as
		// kind-only (no claim) instead of vetoing the whole skeleton.
		struct Slot { UInt32 id = UInt32(-1); UInt8 flags = 0; };
		struct PosSkel
		{
			SignatureRecord::PosKind kind{}; Slot dom; ValueComposition vc{};
			bool hasVal = false;                  // Container: a shared member VALUES var exists
			arg_index namesPos = arg_index(-1);   // Container: the naming argument
		};
		auto canon = [](const SignatureRecord& s, std::vector<PosSkel>& args, PosSkel& res, std::vector<SharedStr>& roles) -> bool
		{
			if (s.dynamicShape || s.resultDeferred || s.repeat.active || !s.resultMembers.empty() || !s.resultMemberSets.empty())
				return false; // deferred/variadic/composite shapes: not a plain skeleton
			std::map<sig_var, UInt32> seen;
			auto slotOf = [&](sig_var v) -> Slot
			{
				Slot r;
				if (v == no_sig_var || v >= s.NrVars())
					return r;
				auto [it, isNew] = seen.try_emplace(v, UInt32(seen.size()));
				if (isNew)
					roles.push_back(s.varRoles[v]);
				r.id = it->second; r.flags = s.varFlags[v];
				return r;
			};
			auto posOf = [&](const SignatureRecord::Pos& p) -> PosSkel
			{
				PosSkel ps; ps.kind = p.kind; ps.vc = p.vc;
				// an Attr's domain, and a Unit position's own identity variable
				if (p.kind == SignatureRecord::PosKind::Attr)
					ps.dom = slotOf(p.domain);
				else if (p.kind == SignatureRecord::PosKind::Unit)
					ps.dom = slotOf(p.values);
				else if (p.kind == SignatureRecord::PosKind::Container)
				{
					ps.dom = slotOf(p.domain);
					ps.hasVal = p.values != no_sig_var;
					ps.namesPos = p.namesPos;
				}
				return ps;
			};
			for (const auto& p : s.args)
				args.push_back(posOf(p));
			res = posOf(s.result);
			return true;
		};

		std::vector<PosSkel> refArgs; PosSkel refRes; std::vector<SharedStr> refRoles;
		if (!canon(sigs->records[recordIdxs[0]].shape, refArgs, refRes, refRoles))
			return std::nullopt;
		std::vector<bool> vcAgrees(refArgs.size(), true);
		bool resVcAgrees = true;
		for (SizeT k = 1; k != recordIdxs.size(); ++k)
		{
			std::vector<PosSkel> a; PosSkel r; std::vector<SharedStr> roles;
			if (!canon(sigs->records[recordIdxs[k]].shape, a, r, roles))
				return std::nullopt;
			if (a.size() != refArgs.size())
				return std::nullopt;
			for (SizeT i = 0; i != a.size(); ++i)
			{
				if (a[i].kind != refArgs[i].kind || a[i].dom.id != refArgs[i].dom.id || a[i].dom.flags != refArgs[i].dom.flags)
					return std::nullopt; // the skeletons disagree: nothing is shared
				if (a[i].kind == SignatureRecord::PosKind::Container
					&& (a[i].hasVal != refArgs[i].hasVal || a[i].namesPos != refArgs[i].namesPos))
					return std::nullopt; // container claims must agree exactly
				if (a[i].vc != refArgs[i].vc)
					vcAgrees[i] = false;
			}
			if (r.kind != refRes.kind || r.dom.id != refRes.dom.id || r.dom.flags != refRes.dom.flags)
				return std::nullopt;
			if (r.vc != refRes.vc)
				resVcAgrees = false;
		}

		// synthesize: canonical domain vars first, then one fresh values var per position
		OperGroupSignatures::MergedRecord mr;
		auto& shape = mr.shape;
		UInt32 nDom = UInt32(refRoles.size());
		auto addVar = [&](const SharedStr& role, UInt8 flags) -> sig_var
		{
			sig_var v = shape.NrVars();
			shape.varRoles.push_back(role);
			shape.varFlags.push_back(flags);
			shape.varFixedCls.push_back(nullptr);
			shape.varConstraints.push_back(TokenID());
			return v;
		};
		std::vector<UInt8> domFlags(nDom, 0);
		for (const auto& p : refArgs)
			if (p.dom.id != UInt32(-1))
				domFlags[p.dom.id] = p.dom.flags;
		if (refRes.dom.id != UInt32(-1))
			domFlags[refRes.dom.id] = refRes.dom.flags;
		for (UInt32 d = 0; d != nDom; ++d)
			addVar(refRoles[d], domFlags[d]);

		auto emitPos = [&](const PosSkel& ps, bool vcOk, CharPtr posName) -> SignatureRecord::Pos
		{
			SignatureRecord::Pos p;
			p.kind = ps.kind;
			p.vc = vcOk ? ps.vc : ValueComposition::Unknown;
			if (ps.kind == SignatureRecord::PosKind::Attr)
			{
				// A DISTINCT role name per position is essential: TypeUnifier keys
				// variables by (owner, instance, NAME), so same-named fresh variables
				// would collapse into ONE class node and falsely equate the positions'
				// value classes (that turned `cond ? 0 : 1` into a bool-vs-uint32
				// conflict between the condition and the result).
				p.values = addVar(SharedStr(posName), 0);
				p.domain = ps.dom.id == UInt32(-1) ? no_sig_var : sig_var(ps.dom.id);
			}
			else if (ps.kind == SignatureRecord::PosKind::Unit)
				p.values = ps.dom.id == UInt32(-1) ? no_sig_var : sig_var(ps.dom.id); // the unit's own identity
			else if (ps.kind == SignatureRecord::PosKind::Container)
			{
				// the agreed container claim: shared member domain slot, an intra-
				// container shared VALUES var when every record declares one (fresh --
				// no cross-position class identity), and the agreed naming argument
				p.domain = ps.dom.id == UInt32(-1) ? no_sig_var : sig_var(ps.dom.id);
				if (ps.hasVal)
					p.values = addVar(SharedStr(posName), 0);
				p.namesPos = ps.namesPos;
			}
			else
				p.kind = SignatureRecord::PosKind::None; // MetaValue & co: no claim
			return p;
		};
		for (SizeT i = 0; i != refArgs.size(); ++i)
			shape.args.push_back(emitPos(refArgs[i], vcAgrees[i], mySSPrintF("?a{}", UInt32(i)).c_str()));
		shape.result = emitPos(refRes, resVcAgrees, "?r");
		// no tuples and no members: ApplyOperRecord's class machinery becomes a no-op
		return mr;
	}

	DefType FunctionChecker::ApplyOperRecord(const OperGroupSignatures::MergedRecord& mr, const SharedStr& headName, const std::vector<DefType>& argTerms
		, const TreeItem* refScope, LispPtr argsList)
	{
		const auto& shape = mr.shape;
		UInt32 inst = m_NextInstance++;
		SharedStr src = mySSPrintF("operator '{}'", headName.c_str());

		UInt32 nv = shape.NrVars();
		std::vector<SizeT> valNode(nv, NO_TYPE_VAR), domNode(nv, NO_TYPE_VAR);
		std::vector<TokenID> roleTok(nv);
		for (UInt32 v = 0; v != nv; ++v)
			roleTok[v] = GetTokenID_mt(shape.varRoles[v].c_str());

		auto VN = [&](sig_var v) -> SizeT
		{
			if (valNode[v] == NO_TYPE_VAR)
			{
				valNode[v] = m_Unifier.ValueVar(nullptr, inst, roleTok[v], src, false, shape.varConstraints[v]);
				if (shape.varFixedCls[v])
					m_Unifier.BindValue(valNode[v], shape.varFixedCls[v], src);
				else
				{
					// soft support set: the union of the congruent members' classes at v
					ValueClassSet set; bool covered = !mr.tuples.empty();
					SharedStr setText; UInt32 nrClasses = 0;
					for (const auto& tuple : mr.tuples)
					{
						const ValueClass* mc = v < tuple.size() ? tuple[v] : nullptr;
						if (!mc)
						{
							covered = false; // some member leaves v unconstrained: no set
							break;
						}
						if (set.test(UInt32(mc->GetValueClassID())))
							continue;
						set.set(UInt32(mc->GetValueClassID()));
						if (nrClasses++)
							setText += ", ";
						setText += SharedStr(mc->GetName());
					}
					if (covered)
						m_Unifier.AddSoftConstraint(valNode[v], set, roleTok[v], src, setText);
				}
			}
			return valNode[v];
		};
		auto DN = [&](sig_var v) -> SizeT
		{
			if (domNode[v] == NO_TYPE_VAR)
				domNode[v] = m_Unifier.UnitVar(nullptr, inst, roleTok[v]);
			return domNode[v];
		};

		// the record variables used in a values role by any position
		std::vector<sig_var> posVars;
		auto notePosVar = [&](sig_var v)
		{
			if (v == no_sig_var)
				return;
			for (sig_var q : posVars)
				if (q == v)
					return;
			posVars.push_back(v);
		};
		for (const auto& p : shape.args)
			if (p.kind == SignatureRecord::PosKind::Attr || p.kind == SignatureRecord::PosKind::Unit)
				notePosVar(p.values);
		if (shape.repeat.active)
			notePosVar(shape.repeat.values);
		if (shape.result.kind == SignatureRecord::PosKind::Attr || shape.result.kind == SignatureRecord::PosKind::Unit)
			notePosVar(shape.result.values);
		for (const auto& rm : shape.resultMembers) // §12.7: member values participate like positions
			notePosVar(rm.values);
		for (sig_var v : posVars)
			VN(v); // create + attach soft sets before any linking

		// tuple agreement over a tuple subset: equal-class pairs and single-class pins
		auto agreeEqual = [&](const std::vector<const std::vector<const ValueClass*>*>& tuples, sig_var v, sig_var q) -> bool
		{
			if (tuples.empty())
				return false;
			for (auto t : tuples)
			{
				const ValueClass* cv = v < t->size() ? (*t)[v] : nullptr;
				const ValueClass* cq = q < t->size() ? (*t)[q] : nullptr;
				if (!cv || cv != cq)
					return false;
			}
			return true;
		};
		auto agreeClass = [&](const std::vector<const std::vector<const ValueClass*>*>& tuples, sig_var v) -> const ValueClass*
		{
			const ValueClass* c = nullptr;
			for (auto t : tuples)
			{
				const ValueClass* cv = v < t->size() ? (*t)[v] : nullptr;
				if (!cv || (c && c != cv))
					return nullptr;
				c = cv;
			}
			return c;
		};

		// batch B: a record variable used in BOTH a values role and a domain role
		// claims unit IDENTITY across those positions (the K2 pattern: lookup's E2
		// in org_rel's VALUES role and values' DOMAIN role; rlookup/invert result
		// values borrowing an argument's domain). Values-only variables stay
		// class-level: their runtime discharge is UnifyValues (class + metric),
		// where a key-identity claim would over-reject (S1, review finding)
		std::vector<bool> inDomainRole(nv, false);
		auto noteDomainRole = [&](sig_var v) { if (v != no_sig_var) inDomainRole[v] = true; };
		for (const auto& p : shape.args)
			if (p.kind == SignatureRecord::PosKind::Attr)
				noteDomainRole(p.domain);
		if (shape.repeat.active)
			noteDomainRole(shape.repeat.domain);
		if (shape.result.kind == SignatureRecord::PosKind::Attr)
			noteDomainRole(shape.result.domain);
		for (const auto& rm : shape.resultMembers) // §12.7: member domains are real domain roles
			noteDomainRole(rm.domain);

		std::vector<const std::vector<const ValueClass*>*> allTuples;
		for (const auto& t : mr.tuples)
			allTuples.push_back(&t);

		// pre-unification links: positions on which EVERY member agrees carry the
		// old shared-node semantics exactly (hard: rigid-rigid conflicts must error)
		SharedStr agreeSrc = mySSPrintF("the registered overloads of operator '{}'", headName.c_str());
		for (SizeT i = 0; i != posVars.size(); ++i)
			for (SizeT j = i + 1; j != posVars.size(); ++j)
				if (agreeEqual(allTuples, posVars[i], posVars[j]))
					m_Unifier.LinkValue(VN(posVars[i]), VN(posVars[j]), agreeSrc);

		// unify each argument against its described position
		auto posType = [&](const SignatureRecord::Pos& p) -> DefType
		{
			DefType r;
			if (p.kind == SignatureRecord::PosKind::Attr)
			{
				r.kind = DefType::Kind::Data;
				r.vcomp = p.vc;
				if (p.values != no_sig_var)
				{
					r.vNode = VN(p.values);
					if (inDomainRole[p.values])
						r.vuNode = DN(p.values); // K2: the SAME node the domain role uses
				}
				if (p.domain != no_sig_var)
				{
					if (shape.varFlags[p.domain] & SignatureRecord::VF_VoidDomain)
						r.dom = DefType::Dom::Void;
					else
					{
						r.dom = DefType::Dom::Node;
						r.dNode = DN(p.domain);
					}
				}
			}
			else if (p.kind == SignatureRecord::PosKind::Unit)
			{
				r.kind = DefType::Kind::UnitVal;
				r.vNode = VN(p.values);
				r.dom = DefType::Dom::Node;
				r.dNode = DN(p.values); // the unit's own identity, in the same var
			}
			return r; // MetaValue/Container/Deferred: Unknown (argument stays walked)
		};

		for (SizeT k = 0; k != argTerms.size(); ++k)
		{
			const SignatureRecord::Pos* p = nullptr;
			SignatureRecord::Pos repeatPos;
			if (k < shape.args.size())
				p = &shape.args[k];
			else if (shape.repeat.active && k >= shape.repeat.fromPos)
			{
				repeatPos.kind = SignatureRecord::PosKind::Attr;
				repeatPos.values = shape.repeat.values; repeatPos.domain = shape.repeat.domain; repeatPos.vc = shape.repeat.vc;
				p = &repeatPos;
			}
			if (!p || p->kind == SignatureRecord::PosKind::None)
				continue;
			SharedStr argSrc = mySSPrintF("argument {} of operator '{}'", k + 1, headName.c_str());
			// K11b: a described CONTAINER argument -- unify its actual members against
			// the shared domain/values variables the description declares
			if (p->kind == SignatureRecord::PosKind::Container)
			{
				LispPtr argExpr;
				if (refScope)
				{
					SizeT j = 0;
					for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++j)
						if (j == k)
						{
							argExpr = a.Left();
							break;
						}
				}
				LinkContainerArg(*p, argTerms[k], argExpr, refScope, argSrc, argsList, VN, DN);
				continue;
			}
			DefType posT = posType(*p);
			if (posT.kind == DefType::Kind::Unknown)
				continue;
			UnifyData(argTerms[k], posT, argSrc, src);
		}

		// narrow the tuples by what the arguments bound; an empty remainder means
		// no registered member matches the (concrete) classes -- reduction is bound
		// to fail on the same FindOper this record was derived from
		std::vector<const std::vector<const ValueClass*>*> compatible;
		for (auto t : allTuples)
		{
			bool ok = true;
			for (sig_var v : posVars)
			{
				if (valNode[v] == NO_TYPE_VAR)
					continue;
				const auto& n = m_Unifier.m_ValueNodes[m_Unifier.FindV(valNode[v])];
				if (!n.bound)
					continue;
				const ValueClass* cv = v < t->size() ? (*t)[v] : nullptr;
				if (cv && cv != n.bound)
				{
					ok = false;
					break;
				}
			}
			if (ok)
				compatible.push_back(t);
		}
		if (compatible.empty() && !allTuples.empty())
			throwErrorF("ExprParser", "{}: the argument types of operator '{}' do not match any of its registered overloads"
				, m_Unifier.FullName().c_str(), headName.c_str());

		// post-narrowing propagation: facts every REMAINING member agrees on.
		// Pins and narrowed links never touch rigid ∀-variables (soft support);
		// flexible nodes take them as ordinary bindings/links.
		for (sig_var v : posVars)
		{
			if (auto c = agreeClass(compatible, v))
			{
				const auto& n = m_Unifier.m_ValueNodes[m_Unifier.FindV(valNode[v])];
				if (!n.rigid && !n.bound)
					m_Unifier.BindValue(valNode[v], c, agreeSrc);
			}
		}
		for (SizeT i = 0; i != posVars.size(); ++i)
			for (SizeT j = i + 1; j != posVars.size(); ++j)
				if (agreeEqual(compatible, posVars[i], posVars[j]))
				{
					SizeT ri = m_Unifier.FindV(valNode[posVars[i]]), rj = m_Unifier.FindV(valNode[posVars[j]]);
					if (ri == rj)
						continue;
					if (m_Unifier.m_ValueNodes[ri].rigid && m_Unifier.m_ValueNodes[rj].rigid)
						continue; // narrowed-set knowledge stays soft on rigid pairs
					m_Unifier.LinkValue(valNode[posVars[i]], valNode[posVars[j]], agreeSrc);
				}

		// the result, in the same variables
		if (shape.dynamicShape || shape.resultDeferred)
			return {};
		DefType r = posType(shape.result);
		// §12.7 slSubItemCall tranche: typed sub-items of a composite result --
		// consumed by InferGeneratedMember when the body references INTO the
		// result (u/Values). Member values stay class-level unless the var is
		// also in a domain role (the batch-B K2 rule); member domains claim
		// identity through the same DN nodes the positions bound -- so
		// unique's Values rides the SAME existential node as the result unit.
		// The completeness flag licenses definition-time missing-member errors
		// (SubItemOperator is certain to reject the same reference) -- and a
		// complete-EMPTY set (the plain select_* groups) must attach too, or
		// its promised definition-time report never fires (review finding).
		if (!shape.resultMembers.empty() || shape.resultMembersComplete || !shape.resultMemberSets.empty())
		{
			auto members = std::make_shared<std::map<SharedStr, DefType, MemberPathLess>>();
			auto memberTypeOf = [&](sig_var values, sig_var domain, ValueComposition vc) -> DefType
			{
				DefType m; m.kind = DefType::Kind::Data; m.vcomp = vc;
				if (values != no_sig_var)
				{
					m.vNode = VN(values);
					if (inDomainRole[values])
						m.vuNode = DN(values);
				}
				if (domain != no_sig_var)
				{
					if (shape.varFlags[domain] & SignatureRecord::VF_VoidDomain)
						m.dom = DefType::Dom::Void;
					else
					{
						m.dom = DefType::Dom::Node;
						m.dNode = DN(domain);
					}
				}
				return m;
			};
			for (const auto& rm : shape.resultMembers)
				(*members)[rm.path] = memberTypeOf(rm.values, rm.domain, rm.vc);

			// NAME-DIRECTED result member families: one member per entry of the
			// declared names array -- discrete_alloc's shadow_prices/<type> and
			// total_allocated/<type>. The array must be definition-time evaluable
			// (the §12.7 / K11b closedness test); otherwise this family contributes
			// nothing, leaving the set exactly as incomplete as before.
			for (const auto& rms : shape.resultMemberSets)
			{
				if (rms.namesPos == arg_index(-1) || !refScope)
					continue;
				LispPtr namesExpr;
				{
					arg_index j = 0;
					for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++j)
						if (j == rms.namesPos)
						{
							namesExpr = a.Left();
							break;
						}
				}
				if (namesExpr.EndP())
					continue;
				auto names = EvalClosedStrArray(refScope, namesExpr);
				if (!names)
					continue; // data-directed: no claim, as before
				DefType mt = memberTypeOf(rms.values, rms.domain, rms.vc);
				for (const auto& nm : *names)
				{
					if (!nm.IsDefined() || nm.empty())
						continue;
					(*members)[rms.prefix + "/" + nm] = mt;
				}
			}
			r.members = std::move(members);
			r.membersComplete = shape.resultMembersComplete;
		}
		return r;
	}

	// §12.7: attempt definition-time K13 spec processing for a DynamicShape record
	// whose single string-valued ArgMetaValue position holds a spec CLOSED over
	// the formals. On success the surviving members' DescribeSpecSignature records
	// (merged; NO DynamicShape) are applied -- including the ruled honest ARITY
	// check: the derived position count IS the CalcNrArgs predicate CreateResult
	// applies, the sole exemption from §6.2's arity-always-defers rule. Every
	// other outcome returns nullopt and the caller defers exactly as today.
	std::optional<DefType> FunctionChecker::TrySpecProcessing(const AbstrOperGroup* og,
		const OperGroupSignatures::MergedRecord& mr, const std::vector<const Operator*>& survivors,
		const SharedStr& headName, const TreeItem* refScope, LispPtr argsList, const std::vector<DefType>& argTerms)
	{
		if (survivors.empty())
			return std::nullopt;
		// review finding: a trailing '...rest' symbol counts as ONE syntactic term
		// here but splices to its captured argument count at reduction -- both the
		// positional mapping and the arity verdict would misalign. Rest-having
		// functions defer the whole spec path (their effective arity is per
		// application)
		if (TreeItem_HasFunctionRestParam(m_FuncItem))
			return std::nullopt;
		SizeT metaPos = SizeT(-1);
		for (SizeT k = 0; k != mr.shape.args.size(); ++k)
			if (mr.shape.args[k].kind == SignatureRecord::PosKind::MetaValue)
			{
				if (metaPos != SizeT(-1))
					return std::nullopt; // several meta-directing positions: not this tranche
				metaPos = k;
			}
		if (metaPos == SizeT(-1))
			return std::nullopt;
		const ValueClass* mc = mr.shape.args[metaPos].metaCls;
		if (!mc || mc->GetValueClassID() != ValueClassID::VT_SharedStr)
			return std::nullopt; // non-string meta values (name arrays): await K11

		LispPtr specExpr; SizeT k = 0;
		for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++k)
			if (k == metaPos)
			{
				specExpr = a.Left();
				break;
			}
		if (specExpr.EndP())
			return std::nullopt;
		auto specValue = EvalClosedSpec(refScope, specExpr);
		if (!specValue)
			return std::nullopt;

		// derive the per-spec record from THIS application's survivors (review
		// finding: the survivor set varies per application with the argument
		// classes, so a cross-application memo keyed on the spec alone would
		// reuse the wrong tuples; derivation is cheap and the LispPtr application
		// memo already de-duplicates identical call sites)
		auto merged = std::make_shared<OperGroupSignatures::MergedRecord>();
		for (const Operator* m : survivors)
		{
			SignatureRecorder rec;
			// a throw here is the member's OWN spec validation (ParseDijkstraString/
			// CheckFlags -- the very predicates CreateResult applies first), reached
			// only for a CLEANLY EVALUATED closed spec: reporting it at definition
			// is honest (the §12.7 ruling's sanctioned upgrade from the v1 deferral)
			bool described = m->DescribeSpecSignature(rec, specValue->c_str());
			if (!described)
				return std::nullopt;
			if (merged->members.empty())
			{
				merged->shape = rec.rec;
				merged->shape.memberClasses.clear();
			}
			else if (!merged->shape.SameShape(rec.rec))
				return std::nullopt; // incongruent per-spec records across survivors: defer
			merged->tuples.push_back(std::move(rec.rec.memberClasses));
			merged->members.push_back(m);
		}
		const auto& derived = merged;

		// the ruled arity exemption -- strictly OUTSIDE every defer-catch above
		if (argTerms.size() != derived->shape.args.size())
			throwErrorF("ExprParser", "{}: number of given arguments to operator '{}' doesn't match the specification '{}': {} arguments given (including the specification), but {} expected"
				, m_Unifier.FullName().c_str(), headName.c_str(), specValue->c_str()
				, argTerms.size(), derived->shape.args.size());

		return ApplyOperRecord(*derived, headName, argTerms, refScope, argsList); // K11b: spec records may carry container positions too
	}

	// §12.7 for_each tranche: definition-time processing of a container-
	// GENERATING meta application (a dont_cache_result group). The group's
	// member(s) describe their argument LAYOUT (MetaMemberLayout); when the
	// name array -- and, for for_each_ind, the field spec directing the layout --
	// is CLOSED over the checked function's formals, it is EVALUATED at
	// definition scan (the ruling: storage-backed sources included) and the
	// application types as a Container carrying the pseudo-expanded member set.
	// Member types come from the layout's unit positions: a direct unit
	// argument types every member uniformly (a formal unit parameter
	// contributes its unifier node -- the K2 bridge; a closed external unit its
	// concrete identity), a (container, name-array) pair resolves a unit per
	// member inside a closed external container. An unresolvable unit defers
	// that MEMBER's type, never the member set. Every other failure -- open
	// arguments, evaluation failure, duplicate names, heterogeneous layouts --
	// returns nullopt and the caller defers exactly as before the tranche.
	std::optional<DefType> FunctionChecker::TryMetaContainerProcessing(const AbstrOperGroup* og, TokenID headID,
		const TreeItem* refScope, LispPtr argsList, const std::vector<DefType>& argTerms)
	{
		// a trailing '...rest' splices at reduction: positions misalign (§12.7 review rule)
		if (TreeItem_HasFunctionRestParam(m_FuncItem))
			return std::nullopt;
		if (!og->GetFirstMember())
			return std::nullopt;

		// for a layout directed by the first argument's value (for_each_ind),
		// that value must itself be closed and evaluable
		std::optional<SharedStr> specValue;
		if (og->HasDynamicArgPolicies())
		{
			if (argsList.EndP())
				return std::nullopt;
			specValue = EvalClosedSpec(refScope, argsList.Left());
			if (!specValue)
				return std::nullopt;
		}

		// every member must describe the SAME layout (each for_each group holds one).
		// A throw is the member's OWN spec validation (ScanFirstArg -- the predicate
		// CreateResult applies first), reached only for a CLEANLY EVALUATED closed
		// spec: reporting it at definition is honest (the §12.7 ruling's sanctioned
		// upgrade from the v1 deferral)
		MetaMemberLayout layout;
		bool anyDescribed = false;
		for (const Operator* m = og->GetFirstMember(); m; m = m->GetNextGroupMember())
		{
			MetaMemberLayout ml;
			bool described = m->DescribeMetaSignature(ml, specValue ? specValue->c_str() : nullptr);
			if (!described)
				return std::nullopt; // an undescribed member could serve the application: defer
			if (anyDescribed && !(ml == layout))
				return std::nullopt;
			layout = ml;
			anyDescribed = true;
		}
		if (!anyDescribed || layout.namesPos == no_meta_pos)
			return std::nullopt;

		// arity: outside the group's accepted range, a same-named function may
		// serve the call -- defer (§6.2). Within it, a spec-derived width is
		// CreateResult's own predicate: its violation is the ruled honest error
		// (for_each_ind's own message shape), strictly outside every defer-catch.
		arg_index nrGiven = arg_index(argTerms.size());
		if (!og->AcceptsArity(nrGiven))
			return std::nullopt;
		if (nrGiven != layout.nrArgs)
		{
			if (!specValue)
				return std::nullopt; // layout-static groups: arity always defers (§6.2)
			SharedStr headName(headID.AsStrRange());
			throwErrorF("ExprParser", "{}: number of given arguments to operator '{}' doesn't match the specification '{}': {} arguments given (including the specification), but {} expected"
				, m_Unifier.FullName().c_str(), headName.c_str(), specValue->c_str()
				, nrGiven, layout.nrArgs);
		}

		auto argExprAt = [&](arg_index p) -> LispPtr
		{
			arg_index k = 0;
			for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++k)
				if (k == p)
					return a.Left();
			return {};
		};

		auto names = EvalClosedStrArray(refScope, argExprAt(layout.namesPos));
		if (!names)
			return std::nullopt;

		// one unit-providing source per layout role; FragAt yields a UnitVal
		// fragment per member, or Unknown where nothing is knowable
		struct UnitSource
		{
			DefType uniform;                                     // context-only mode
			SharedTreeItem container;                            // pair mode
			std::optional<std::vector<SharedStr>> perMemberNames;

			DefType FragAt(SizeT i) const
			{
				if (container && perMemberNames)
				{
					if (i >= perMemberNames->size())
						return {};
					try
					{
						auto u = container->FindItem((*perMemberNames)[i]);
						if (u && IsUnit(u.get()) && !u->InTemplate())
						{
							DefType f; f.kind = DefType::Kind::UnitVal;
							f.vc = AsUnit(u.get())->GetValueType();
							f.dom = DefType::Dom::Concrete; f.domKeep = u; f.domUnit = AsUnit(u.get());
							return f;
						}
					}
					catch (...) {}
					return {}; // an unresolvable unit name defers this member's type
				}
				return uniform;
			}
		};
		auto resolveUnitSource = [&](arg_index pos, arg_index namesPos) -> UnitSource
		{
			UnitSource s;
			if (pos == no_meta_pos || pos >= nrGiven)
				return s;
			try
			{
				if (namesPos == no_meta_pos)
				{
					if (argTerms[pos].kind == DefType::Kind::UnitVal)
						s.uniform = argTerms[pos]; // formal unit parameters ride their unifier node here
					else if (LispPtr ue = argExprAt(pos); ue.IsSymb())
					{
						if (auto vc = ValueClass::FindByScriptName(ue.GetSymbID()))
						{
							// the DEFAULT unit of a value-class name ('float64'):
							// class pinned, identity left unclaimed
							s.uniform.kind = DefType::Kind::UnitVal;
							s.uniform.vc = vc;
							return s;
						}
						// a def-scope external unit: closed by construction (§12.7)
						const TreeItem* local = nullptr; UInt32 pi = 0;
						SharedTreeItem ext; ExtRefKind ek = ExtRefKind::DefScopeExternal;
						if (ResolveName(refScope, ue.GetSymbID(), &local, &pi, &ext, &ek) == 2
							&& ek == ExtRefKind::DefScopeExternal && ext && IsUnit(ext.get()) && !ext->InTemplate())
						{
							s.uniform.kind = DefType::Kind::UnitVal;
							s.uniform.vc = AsUnit(ext.get())->GetValueType();
							s.uniform.dom = DefType::Dom::Concrete; s.uniform.domKeep = ext; s.uniform.domUnit = AsUnit(ext.get());
						}
					}
					return s;
				}
				// pair mode: a CLOSED external container + a closed per-member name array
				LispPtr ce = argExprAt(pos);
				if (!ce.IsSymb() || namesPos >= nrGiven)
					return s;
				const TreeItem* local = nullptr; UInt32 pi = 0;
				SharedTreeItem ext; ExtRefKind ek = ExtRefKind::DefScopeExternal;
				if (ResolveName(refScope, ce.GetSymbID(), &local, &pi, &ext, &ek) != 2
					|| ek != ExtRefKind::DefScopeExternal || !ext || ext->IsFunctionItem() || ext->InTemplate())
					return s;
				ext->UpdateMetaInfo(); // its own rule may generate the named units
				auto nm = EvalClosedStrArray(refScope, argExprAt(namesPos));
				if (!nm)
					return s;
				s.container = ext;
				s.perMemberNames = std::move(nm);
			}
			catch (...)
			{
				s.container = {};
				s.perMemberNames.reset(); // defer the member types, keep the member set
			}
			return s;
		};

		UnitSource duSrc = resolveUnitSource(layout.domainPos, layout.domainNamesPos);
		UnitSource vuSrc = resolveUnitSource(layout.valuesPos, layout.valuesNamesPos);
		UnitSource unSrc = resolveUnitSource(layout.unitPos, layout.unitNamesPos);

		auto memberTypeAt = [&](SizeT i) -> DefType
		{
			switch (layout.memberKind)
			{
			case MetaMemberLayout::MemberKind::Data:
			{
				DefType m; m.kind = DefType::Kind::Data; m.vcomp = layout.vcomp;
				DefType du = duSrc.FragAt(i);
				if (du.kind == DefType::Kind::UnitVal)
				{
					if (du.vc && du.vc->GetValueClassID() == ValueClassID::VT_Void)
						m.dom = DefType::Dom::Void;
					else
					{
						m.dom = du.dom; m.domUnit = du.domUnit; m.domKeep = du.domKeep; m.dNode = du.dNode;
					}
				}
				DefType vu = vuSrc.FragAt(i);
				if (vu.kind == DefType::Kind::UnitVal)
				{
					m.vc = vu.vc; m.vNode = vu.vNode;
					m.vuNode = vu.dNode; m.vUnit = vu.domUnit; m.vKeep = vu.domKeep;
				}
				return m;
			}
			case MetaMemberLayout::MemberKind::Unit:
			{
				DefType m; m.kind = DefType::Kind::UnitVal;
				DefType un = unSrc.FragAt(i);
				if (un.kind == DefType::Kind::UnitVal)
					m.vc = un.vc; // class only: each generated unit's IDENTITY is fresh per holder
				return m;
			}
			default:
				return {}; // TemplateCopy / plain items: member types per application
			}
		};

		auto members = std::make_shared<std::map<SharedStr, DefType, MemberPathLess>>();
		for (SizeT i = 0; i != names->size(); ++i)
		{
			const SharedStr& nm = (*names)[i];
			if (!nm.IsDefined() || nm.empty())
				continue; // skipped rows, exactly as ForEach_CreateResult
			if (!members->emplace(nm, memberTypeAt(i)).second)
				return std::nullopt; // duplicate generated names (case-insensitively, like the tree): not modeled
		}

		DefType r;
		r.kind = DefType::Kind::Container;
		r.members = std::move(members);
		r.membersComplete = true;
		return r;
	}

	// §12.7: type a reference INTO a rule-computed member set -- 'r/foo' where
	// r's rule is a meta application (the pseudo-expanded for_each container)
	// or a cacheable composite whose record describes its sub-items
	// (u := unique(x); u/Values -- the slSubItemCall tranche). An exact hit
	// (case-insensitive, like the engine's item lookup) yields the member's
	// type, a path prefix of a deeper member is an intermediate container (no
	// claim), a path BELOW a member defers (members may carry deeper
	// sub-structure), and a complete-set miss is reported honestly -- sound
	// because meta rules reject every inline member access and described-
	// complete cache results make SubItemOperator certain to reject the same
	// reference per application. Everything else defers as before.
	DefType FunctionChecker::InferGeneratedMember(TokenID sym, const TreeItem* genItem, const SharedStr& subPath)
	{
		DefType ct = InferBodyItem(genItem);
		if (!ct.members)
			return {};
		if (auto it = ct.members->find(subPath); it != ct.members->end())
			return it->second;
		auto ciEq = [](char x, char y) { return MemberPathLess::Fold(x) == MemberPathLess::Fold(y); };
		SizeT sn = subPath.ssize();
		for (const auto& [path, mt] : *ct.members)
		{
			SizeT pn = path.ssize();
			if (pn > sn && *(path.begin() + sn) == DELIMITER_CHAR
				&& std::equal(subPath.begin(), subPath.send(), path.begin(), ciEq))
				return {}; // an intermediate container on the way to a deeper member
			if (sn > pn && *(subPath.begin() + pn) == DELIMITER_CHAR
				&& std::equal(path.begin(), path.send(), subPath.begin(), ciEq))
				return {}; // a path BELOW a member: members may carry deeper
				           // sub-structure the member set makes no claim about
		}
		if (!ct.membersComplete)
			return {};
		if (ct.members->empty())
			throwErrorF("ExprParser", "'{}': the calculation rule of '{}' generates no members, so '{}' cannot exist (body of function '{}')"
				, SharedStr(sym.AsStrRange()).c_str(), genItem->GetFullName().c_str()
				, subPath.c_str(), m_FuncItem->GetFullName().c_str());
		SharedStr listed; UInt32 nrListed = 0;
		for (const auto& [path, mt] : *ct.members)
		{
			if (nrListed == 10)
			{
				listed += ", ...";
				break;
			}
			if (nrListed++)
				listed += ", ";
			listed += path;
		}
		throwErrorF("ExprParser", "'{}': the calculation rule of '{}' generates member(s) {}; '{}' is not among them (body of function '{}')"
			, SharedStr(sym.AsStrRange()).c_str(), genItem->GetFullName().c_str()
			, listed.c_str(), subPath.c_str(), m_FuncItem->GetFullName().c_str());
	}

	DefType FunctionChecker::InferOperatorApplication(const AbstrOperGroup* og, TokenID headID, const TreeItem* refScope, LispPtr argsList)
	{
		std::vector<DefType> argTerms;
		for (LispPtr a = argsList; !a.EndP(); a = a.Right())
			argTerms.push_back(InferExpr(refScope, a.Left()));

		if (!og->MustCacheResult())
		{
			// §12.7 for_each tranche: a container-GENERATING meta application
			// whose meta-directing arguments are CLOSED over the formals is
			// processed at definition scan (the ruling); every failure inside
			// falls through to the wholesale deferral, byte-identical to before
			if (auto containerType = TryMetaContainerProcessing(og, headID, refScope, argsList, argTerms))
				return *containerType;
			return {}; // meta/selection groups: fluid effective arity, per-application checking
		}

		auto sigs = og->GetSignatures();
		if (!sigs)
			return {}; // no member describes itself: defer, as before the description layer

		arg_index nrArgs = arg_index(argTerms.size());
		Int32 theRecord = -2; // -2: no class survivor yet; -1: mixed/undescribed -> defer
		bool anyAritySurvivor = false, anyFeasibleOnlyElimination = false;
		std::vector<const Operator*> survivors; // §12.7: the members a spec-record may derive from
		std::vector<Int32> survivorRecords;     // §6.2 cross-record fallback: the DISTINCT records that survive
		for (const auto& me : sigs->members)
		{
			if (!MemberAcceptsArity(og, me.oper, nrArgs))
				continue;
			anyAritySurvivor = true;
			auto mv = ClassifyMember(m_Unifier, me.oper, argTerms);
			if (mv != MemberVerdict::Survives)
			{
				if (mv == MemberVerdict::EliminatedFeasible)
					anyFeasibleOnlyElimination = true;
				continue;
			}
			survivors.push_back(me.oper);
			if (std::find(survivorRecords.begin(), survivorRecords.end(), me.recordIdx) == survivorRecords.end())
				survivorRecords.push_back(me.recordIdx);
			if (theRecord != -1)
			{
				if (me.recordIdx < 0)
					theRecord = -1;
				else if (theRecord == -2)
					theRecord = me.recordIdx;
				else if (theRecord != me.recordIdx)
					theRecord = -1;
			}
		}
		if (!anyAritySurvivor)
			return {}; // arity outside every member: defer (a same-named function or FindOper's own widening may serve)

		SharedStr headName(headID.AsStrRange()); // materialized: no TokenStr may span the unification calls
		if (theRecord == -2)
		{
			// every member rejected the known argument classes. Members rejected by a
			// concrete class fail at reduction with certainty; a member rejected only
			// through a feasible SET is symbolic knowledge, so the verdict defers
			// (soft support: the ∀-variable is not rejected).
			if (anyFeasibleOnlyElimination)
				return {};
			throwErrorF("ExprParser", "{}: the argument types of operator '{}' do not match any of its registered overloads"
				, m_Unifier.FullName().c_str(), headName.c_str());
		}
		if (theRecord < 0)
		{
			// §6.2 cross-record fallback: several congruence classes survive, so no
			// single record's VALUE claims apply -- but they may still AGREE on the
			// DOMAIN skeleton, and whichever member reduction picks, that part holds.
			// This un-gates the combining operators: `add`/`+` splits into three
			// records (two polygon families + the scalar family), all of which say
			// "both arguments and the result share ONE domain", so
			// `pcount(nw/F1) + pcount(nw/F2)` over different node units is now a
			// DEFINITION-time conflict instead of an instantiation-time one.
			if (auto skeleton = BuildDomainSkeletonRecord(sigs, survivorRecords))
				return ApplyOperRecord(*skeleton, headName, argTerms, refScope, argsList); // K11 leftover: container positions link here too
			return {}; // no agreed skeleton (or an undescribed member survives): defer
		}

		const auto& mr = sigs->records[theRecord];
		// §12.7: a DynamicShape record with a definition-time-knowable spec is
		// upgraded to the spec-derived concrete record; every failure inside
		// falls through to the DynamicShape deferral, byte-identical to today
		if (mr.shape.dynamicShape)
			if (auto specType = TrySpecProcessing(og, mr, survivors, headName, refScope, argsList, argTerms))
				return *specType;
		return ApplyOperRecord(mr, headName, argTerms, refScope, argsList); // K11b: enable ArgContainer linking
	}

	DefType FunctionChecker::InferExpr(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.EndP())
			return {};
		if (!expr.IsRealList())
		{
			if (expr.IsSymb())
			{
				TokenID sym = expr.GetSymbID();
				if (sym == t_Hole || token::isConst(sym) || ValueClass::FindByScriptName(sym))
					return {};
				const TreeItem* local = nullptr; UInt32 paramIdx = 0;
				SharedStr genSubPath;
				ExtRefKind extKind = ExtRefKind::DefScopeExternal;
				switch (ResolveName(refScope, sym, &local, &paramIdx, nullptr, &extKind, &genSubPath))
				{
				case 0: return ParamType(paramIdx);
				case 1: return local ? InferBodyItem(local) : DefType{};
				case 2:
					// K11a-2: structured-parameter member access. Code 2 is OVERLOADED
					// (K11a-3 review finding): prelude refs, closure captures and
					// def-scope externals also return 2 WITHOUT touching paramIdx/
					// genSubPath -- only a genuine ParamMember may reach the member map
					// (the stale defaults falsely hit parameter 0 with an empty path)
					if (extKind == ExtRefKind::ParamMember)
						return InferParamMember(paramIdx, genSubPath);
					return {}; // prelude/capture/external: checked per application
				case 3: return local ? InferGeneratedMember(sym, local, genSubPath) : DefType{}; // §12.7: rule-generated member access
				default: return {}; // imports/externals: their types are checked per application
				}
			}
			return {}; // numeric / string literals: void-domain constants, class per application
		}

		TokenID headID = expr.Left().GetSymbID();
		if (headID == token::sourceDescr)
			return InferExpr(refScope, expr.Right().Left());
		if (headID == token::arrow || headID == token::scope || headID == token::subitem)
			throwErrorF("ExprParser", "the '{}' construct is not yet supported inside inlined function bodies"
				"; bind the function application to a container to use the instantiating form"
				, headID.GetStr().c_str());

		// §5.10 applied call result: type the application when the inner value's
		// signature is known; residual arity is verified at reduction
		if (headID == t_ApplyValue)
		{
			DefType fnVal = InferArg(refScope, expr.Right().Left());
			return InferApplication(refScope, fnVal, expr.Right().Right(), "(...)");
		}

		const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
		if (og->IsTemplateCall() && !ValueClass::FindByScriptName(headID)) // value-type heads (float64(x)) are conversions, not function calls
		{
			if (headID == t_Map)
				throwErrorF("ExprParser", "map(...) can only appear as a whole calculation rule, not as a sub-expression");
			// the head name is materialized BEFORE the recursive walk: a TokenStr
			// temporary holds the token registry's shared lock, and the walk below
			// parses body expressions (token creation needs the exclusive lock)
			SharedStr headName(headID.AsStrRange());
			for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
				if (m_Params[i]->GetID() == headID)
					return InferApplication(refScope, ParamType(i), expr.Right(), headName.c_str());

			// a direct function/import call: resolve, then type the application
			auto callee = m_FuncItem->FindItem(SharedStr(headID.AsStrRange()));
			if (!callee || !callee->IsFunctionItem())
				if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
					if (auto lex = defParent->FindItem(SharedStr(headID.AsStrRange())); lex && lex->IsFunctionItem())
						callee = lex;
			if (!callee || !callee->IsFunctionItem())
				if (auto pf = FindPreludeFunction(headID); pf && pf->IsFunctionItem())
					callee = pf; // prelude: implicit outermost namespace for call heads
			if (!callee)
				if (auto env = FindEnclosingFunctionMember(headID))
				{
					// an enclosing function's parameter or local applied as a function:
					// bound through the closure environment at reduction; a declared
					// signature (an exemplar-cloned parameter) still types the call
					DefType envVal; envVal.kind = DefType::Kind::Func;
					envVal.fn = env->IsFunctionItem() ? env.get() : nullptr;
					m_Keep.push_back(env);
					return InferApplication(refScope, envVal, expr.Right(), headName.c_str());
				}
			if (!callee)
				throwErrorF("ExprParser", "'{}': unknown operator or function in body of function '{}'"
					, headName.c_str(), m_FuncItem->GetFullName().c_str());
			if (!callee->IsFunctionItem())
				throwErrorF("ExprParser", "'{}': template instantiations are not supported inside function bodies"
					, headName.c_str());
			DefType calleeVal; calleeVal.kind = DefType::Kind::Func; calleeVal.fn = callee.get();
			m_Keep.push_back(callee);
			return InferApplication(refScope, calleeVal, expr.Right(), headName.c_str());
		}

		// a value-class head is a conversion: the class is the head's, the domain
		// follows the (single) argument
		if (auto convVC = ValueClass::FindByScriptName(headID))
		{
			DefType argT; UInt32 n = 0;
			for (LispPtr a = expr.Right(); !a.EndP(); a = a.Right(), ++n)
				argT = InferExpr(refScope, a.Left());
			if (n != 1)
				return {};
			DefType r; r.kind = DefType::Kind::Data; r.vc = convVC;
			if (argT.kind == DefType::Kind::Data)
			{
				r.dom = argT.dom; r.domUnit = argT.domUnit; r.domKeep = argT.domKeep; r.dNode = argT.dNode;
				r.vcomp = argT.vcomp; // a value conversion preserves the geometric composition
			}
			return r;
		}

		// built-in operator: the group's described signature records (batch A)
		// type the application; groups without described members walk their
		// arguments and defer the result. Memoized per (refScope, interned expr)
		// so repeated applications -- crucially fresh-unit ones like unique(a) /
		// select(c) -- share ONE result node (batch D, §6.1). An error throws
		// (never cached); a cache hit skips re-walking the (identical) arguments,
		// whose own checks already ran on the first occurrence
		auto memoKey = std::make_pair(refScope, LispRef(expr));
		if (auto it = m_ApplTypes.find(memoKey); it != m_ApplTypes.end())
			return it->second;
		DefType applResult = InferOperatorApplication(og, headID, refScope, expr.Right());
		m_ApplTypes.emplace(std::move(memoKey), applResult);
		return applResult;
	}

	DefType FunctionChecker::InferBodyItem(const TreeItem* refItem)
	{
		if (auto it = m_ItemTypes.find(refItem); it != m_ItemTypes.end())
			return it->second;
		if (!m_InProgress.insert(refItem).second)
			return {}; // cycle guard; true circularity is caught by the reduction
		DefType inferred;
		SharedStr exprStr = refItem->GetExpr();
		if (!exprStr.empty())
		{
			if (AbstrCalculator::MustEvaluate(exprStr.c_str()))
				throwErrorF("ExprParser", "'{}': leading-'=' string indirection is not supported inside function bodies"
					, refItem->GetFullName().c_str());
			auto calc = AbstrCalculator::ConstructFromStr(refItem, exprStr, CalcRole::Calculator);
			auto refScope = refItem->GetTreeParent();
			inferred = InferExpr(refScope.get(), RewriteExpr(calc->GetLispExprOrg()));
		}
		DefType declared = DeclaredItemType(refItem);
		if (declared.kind != DefType::Kind::Unknown && inferred.kind != DefType::Kind::Unknown)
		{
			SharedStr itemName(refItem->GetID().AsStrRange()); // TokenStr must not span UnifyData (token-registry lock)
			SharedStr ruleSrc = mySSPrintF("the calculation rule of '{}'", itemName.c_str());
			SharedStr declSrc = mySSPrintF("the declared type of '{}'", itemName.c_str());
			UnifyData(inferred, declared, ruleSrc, declSrc);
		}
		DefType itemType = declared.kind != DefType::Kind::Unknown ? declared : inferred;
		m_InProgress.erase(refItem);
		m_ItemTypes[refItem] = itemType;
		return itemType;
	}

	void CheckFunctionDefinition(const TreeItem* funcItem)
	{
		if (TreeItem_IsFunctionDefinitionChecked(funcItem))
		{
			// The definition was already checked. A wrong definition is a persistent error whose
			// verdict is recorded ON THE FUNCTION ITEM (below) -- re-raise it so this application
			// fails too, WITHOUT re-running the (failing) check at every application.
			if (funcItem->WasFailed(FailType::MetaInfo))
				funcItem->ThrowFail();
			return;
		}
		if (TreeItem_IsFunctionVariantSet(funcItem))
			return; // each variant is checked at its own application

		// §12.7: closed-spec evaluation can UpdateMetaInfo definition-scope items
		// whose expressions apply the function CURRENTLY being checked, and the
		// checked-flag is set only on completion -- guard against re-entry (the
		// outer invocation completes the verdict). Meta-thread-only, like the
		// whole checker, so a plain static set suffices
		static std::set<const TreeItem*> s_CheckInProgress;
		if (!s_CheckInProgress.insert(funcItem).second)
			return;
		struct Eraser
		{
			std::set<const TreeItem*>* s; const TreeItem* f;
			~Eraser() { s->erase(f); }
		} eraser{ &s_CheckInProgress, funcItem };
		try
		{
			TokenID resultName = TreeItem_GetFunctionResultName(funcItem);
			auto resultChild = funcItem->GetConstSubTreeItemByID(resultName);
			if (!resultChild)
				throwErrorF("ExprParser", "'{}': designated result '{}' not found"
					, funcItem->GetFullName().c_str(), resultName.GetStr().c_str());
			if (!resultChild->GetExpr().empty()) // signature-only functions and nested-function results have no body expression here
			{
				FunctionChecker chk;
				chk.m_FuncItem = funcItem;
				chk.m_Unifier.m_ApplItem = funcItem;
				chk.m_Unifier.m_Phase = "the definition of ";
				chk.m_DeclSource = mySSPrintF("declared by function '{}'", funcItem->GetFullName().c_str());
				UInt32 nrParams = TreeItem_GetFunctionParamCount(funcItem);
				const TreeItem* p = funcItem->_GetFirstSubItem();
				for (UInt32 i = 0; i < nrParams && p; ++i, p = p->GetNextItem())
					chk.m_Params.push_back(p);
				chk.InferBodyItem(resultChild.get());
			}
		}
		catch (...)
		{
			// Record the verdict ON THE FUNCTION DEFINITION and cache it: a wrong definition
			// becomes a failed item (so it shows as failed, whether the check was triggered by
			// an application or by the audit), and every subsequent application re-raises the
			// same verdict (above) rather than re-running the failing check. CatchFail captures
			// the in-flight exception with its context; the re-throw fails the caller too.
			funcItem->CatchFail(FailType::MetaInfo);
			TreeItem_SetFunctionDefinitionChecked(funcItem);
			throw;
		}
		TreeItem_SetFunctionDefinitionChecked(funcItem);
	}

} // anonymous namespace

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
					holder->throwItemErrorF("map: partial-application argument '{}' not found", ae.GetSymbID().GetStr().c_str());
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
		holder->throwItemErrorF("map: '{}' is not a function", fHead.GetSymbID().GetStr().c_str());
	if (SizeT(TreeItem_GetFunctionParamCount(funcItem.get())) != fixedArgs.size())
		holder->throwItemErrorF("map: function '{}' takes {} parameter(s), but the (partial) application supplies {} including the '_' element"
			, funcItem->GetFullName().c_str(), UInt32(TreeItem_GetFunctionParamCount(funcItem.get())), UInt32(fixedArgs.size()));

	auto srcItem = ac->FindItem(srcExpr.GetSymbID());
	if (!srcItem)
		holder->throwItemErrorF("map: source '{}' not found", srcExpr.GetSymbID().GetStr().c_str());
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
			child = CreateDataItem(holder, c->GetID(), rdi->GetAbstrDomainUnit(), rdi->GetAbstrValuesUnit(), rdi->GetValueComposition());
		}
		else if (IsUnit(res.get()))
			child = AsUnit(res.get())->GetUnitClass()->CreateUnit(holder, c->GetID());
		else
			child = holder->CreateItem(c->GetID()); // a container/other result: keep the plain follower

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
					, og->GetName()
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
					auto msg = SharedStr(symbID.AsStrRange());
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
				, og->GetName()
				, resultHolder->GetCurrentObjClass()->GetName()
				, oper->GetResultClass()->GetName()
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

auto DeriveSubItem(const AbstrCalculator* ac, SubstitutionBuffer& substBuff, LispPtr subItemExprTail) -> MetaInfo
{
	assert(ac);
	LispRef contextExpr = subItemExprTail.Left();
	SharedTreeItem container;
	if (contextExpr.IsList())
	{
		MG_CHECK(contextExpr.Left().IsSymb());
		if (contextExpr.Left().GetSymbID() == token::subitem)
		{
			auto result = DeriveSubItem(ac, substBuff, contextExpr.Right());
			if (substBuff.avs == AVS_SuspendedOrFailed)
				return {};
			if (result.index() == 2)
				container = std::get<2>(result);
			else
				contextExpr = std::get<1>(result);
		}
	}
	if (!container)
	{
		if (!contextExpr.IsSymb())
		{
			auto args = ac->SubstituteArgs(substBuff, LispRef(contextExpr, subItemExprTail.Right()), AbstrOperGroup::FindName(token::subitem), 0, SharedStr());
			if (substBuff.avs == AVS_SuspendedOrFailed)
				return {};

			return LispRef(LispRef(token::subitem), std::move(args));
		}
		container = ac->FindOrVisitItem(substBuff, contextExpr.GetSymbID());
		if (substBuff.avs == AVS_SuspendedOrFailed)
			return {};
	}
	if (!container)
		throwErrorD("ExprParser", "left operand of SubExpr doesn't resolve to a configurartion item");
	assert(!container->IsCacheItem());
	container->UpdateMetaInfo();
	registerSupplier(substBuff, container.get());

	auto subItemNameExpr = subItemExprTail.Right().Left();
	auto holder = ac->m_Holder.lock();
	if (!holder)
		throwTaskCanceled();
	AbstrCalculatorRef calculator = AbstrCalculator::ConstructFromLispRef(holder.get(), subItemNameExpr, CalcRole::Other);
	auto res = CalledCalcHandle(calculator.get(), DataArray<SharedStr>::GetStaticClass());
	auto subItemPath = GetCurrValue<SharedStr>(AsDataItem(res), 0);
	auto subItem = FindSubItem(container.get(), subItemPath);
	assert(subItem);
	assert(!subItem->IsCacheItem());
	subItem->UpdateMetaInfo();

	if (substBuff.optionalVisitor && Test(substBuff.svf, SupplierVisitFlag::NamedSuppliers))
		if (substBuff.optionalVisitor->Visit(subItem.get()) == AVS_SuspendedOrFailed)
			substBuff.avs = AVS_SuspendedOrFailed;

	return subItem;
}

LispRef AbstrCalculator::SubstituteExpr_impl(SubstitutionBuffer& substBuff, LispRef localExpr, metainfo_policy_flags mpf) const
{
	LispRef& bufferValue = substBuff.BufferedLispRef(mpf, localExpr);

	if (bufferValue == LispRef())
	{
		if (localExpr.IsRealList()) // operator call or calculation scheme instantiation
		{
			MG_CHECK(localExpr.Left().IsSymb());

			LispRef head = localExpr.Left();
			if (head->GetSymbID() == token::scope)
			{
				auto leftExpr = localExpr.Right().Left();
				if (!leftExpr.IsSymb())
					throwErrorF("ExprParser", "Scope operator: Left operand should be a name, but '{}' given.", AsFLispSharedStr(leftExpr, FormattingFlags::ThousandSeparator).c_str());
				SharedTreeItem scopeItem = FindItem(leftExpr.GetSymbID());
				if (!scopeItem)
					throwErrorF("ExprParser", "Scope operator: container '{}' not found", leftExpr.GetSymbID().GetStr().c_str());

				tmp_swapper<SharedTreeItem> swap(m_SearchContext, scopeItem);
				SubstitutionBuffer localBuffer;
				localBuffer.optionalVisitor = substBuff.optionalVisitor;
				bufferValue = SubstituteExpr_impl(localBuffer, localExpr.Right().Right().Left(), mpf);
				goto exit;
			}
			if (head->GetSymbID() == token::arrow)
			{
				LispRef indexExpr = localExpr.Right().Left();
				if (!indexExpr.IsSymb())
					throwErrorF("Calculation Rule Parser", "named DataItem expected as left operand of the arrow operator: try defining an attribute with calculation rule '{}'"
					, AsString(indexExpr.AsLispPtr())
					);

				auto indexItem = FindOrVisitItem(substBuff, indexExpr.GetSymbID());
				if (substBuff.avs == AVS_SuspendedOrFailed)
					return {};
				if (!indexItem.get())
					throwErrorF("Calculation Rule Parser", "reference '{}' not found (as left operand of the arrow operator)"
					, AsString(indexExpr.GetSymbID())
					);
				if (!IsDataItem(indexItem.get()))
					throwErrorF("Calculation Rule Parser", "DataItem expected as left operand of the arrow operator; '{}' refers to a {}"
					, AsString(indexExpr.GetSymbID())
					, AsString(indexItem->GetDynamicClass()->GetID())
					);

				auto avu = AbstrValuesUnit( AsDataItem(indexItem.get()) );
				if (!avu)
				{
					auto formalDomainUnit = SharedStr(AsDataItem(indexItem.get())->DomainUnitToken());
					auto formalValuesUnit = SharedStr(AsDataItem(indexItem.get())->ValuesUnitToken());
					throwErrorF("Calculation Rule Parser", "DataItem with a specified formal domain and values-unit expected as left operand of the arrow operator."
						"\nHint: '{}' is specified with formal domain '{}' and values-unit '{}'. Check that these refer to unit definitions seen from the current context."
						, AsString(indexExpr.GetSymbID()), formalDomainUnit.c_str(), formalValuesUnit.c_str()
					);
				}
				indexExpr = slSupplierExprImpl(substBuff, indexItem.get(), mpf); // now process left before re-assigning search context

				tmp_swapper<SharedTreeItem> swap(m_SearchContext, make_shared_tree(avu, existing_obj{}));
				SubstitutionBuffer localBuffer; localBuffer.svf = substBuff.svf; localBuffer.optionalVisitor = substBuff.optionalVisitor;
				auto arrowedExpr = SubstituteExpr_impl(localBuffer, localExpr.Right().Right().Left(), mpf);
				if (localBuffer.avs == AVS_SuspendedOrFailed)
				{
					substBuff.avs = AVS_SuspendedOrFailed;
					return {};
				}

				bufferValue =
					ExprList(token::lookup
						, indexExpr
						, std::move(arrowedExpr)
					);
				goto exit;
			}
			if (head->GetSymbID() == token::subitem)
			{
				auto metaInfo = DeriveSubItem(this, substBuff, localExpr.Right());
				if (substBuff.avs == AVS_SuspendedOrFailed)
					return {};

				if (metaInfo.index() == 2)
					bufferValue = std::get<2>(metaInfo)->GetCheckedKeyExpr();
				else
					bufferValue = std::get<1>(metaInfo);
				goto exit;
			}
			if (head.GetSymbID() == token::sourceDescr)
			{
				LispPtr sourceRef = localExpr.Right().Left();
				dms_assert(sourceRef.IsSymb());
				bufferValue = slSupplierExpr(substBuff, sourceRef, mpf);
				goto exit;
			}

			// §5.9: 'apply F(args)' as a sub-expression == the bare call (a value); unwrap.
			// 'instantiate' / 'apply T' produce/instantiate a container and are root-only.
			if (head.GetSymbID() == t_ApplyItem || head.GetSymbID() == t_InstantiateItem)
			{
				LispRef innerCall = localExpr.Right().Left();
				MG_CHECK(innerCall.IsRealList() && innerCall.Left().IsSymb());
				if (head.GetSymbID() == t_InstantiateItem)
					throwErrorF("ExprParser", "'instantiate' produces a container and can only appear as a whole calculation rule, not as a sub-expression");
				auto callee = FindOrVisitItem(substBuff, innerCall.Left().GetSymbID());
				if (substBuff.avs == AVS_SuspendedOrFailed)
					return {};
				if (!callee || !callee->IsFunctionItem())
					throwErrorF("ExprParser", "'apply' on a template can only appear as a whole calculation rule; '{}' is not a function"
						, innerCall.Left().GetSymbID().GetStr().c_str());
				bufferValue = SubstituteExpr_impl(substBuff, innerCall, mpf); // apply F == bare F
				goto exit;
			}

			// §5.10 applied call result: reduce the inner expression to a function value and
			// bind the outer arguments; the final result must be data in this position
			if (head.GetSymbID() == t_ApplyValue)
			{
				if (substBuff.optionalVisitor)
				{
					// visit-only pass: record the suppliers of the sub-expressions
					for (LispPtr a = localExpr.Right(); !a.EndP(); a = a.Right())
					{
						SubstituteExpr_impl(substBuff, LispRef(a.Left()), metainfo_policy_flags::subst_allowed);
						if (substBuff.avs == AVS_SuspendedOrFailed)
							return {};
					}
					goto exit; // bufferValue stays empty
				}
				{
					auto avHolder = m_Holder.lock();
					if (!avHolder)
						throwTaskCanceled();
					auto avResolveData = [&](LispPtr e) { return SubstituteExpr_impl(substBuff, LispRef(e), metainfo_policy_flags::subst_allowed); };
					auto avFindItem = [&](TokenID t) -> SharedTreeItem { return FindItem(t); };
					CallArg r = ResolveCallerArg(localExpr, avResolveData, avFindItem, &substBuff, avHolder);
					if (substBuff.avs == AVS_SuspendedOrFailed)
						return {};
					if (r.binding)
						throwErrorF("ExprParser", "a function value can only be applied with '(...)', passed as an argument, or returned as a result");
					bufferValue = r.key;
				}
				goto exit;
			}

			// §5.9: a container literal only exists as a function argument, destructured by
			// ResolveCallerArg. It reaches here only on the visit-only supplier walk: recurse
			// into the domain and member values (with '.' rebound to the domain) to register
			// their suppliers, then yield nothing (the literal itself materializes no item).
			if (head.GetSymbID() == t_ContainerLiteral)
			{
				LispPtr domainExpr = localExpr.Right().Left();
				if (!(domainExpr.IsSymb() && domainExpr.GetSymbID() == t_NoDomain))
				{
					SubstituteExpr_impl(substBuff, LispRef(domainExpr), mpf);
					if (substBuff.avs == AVS_SuspendedOrFailed) return {};
				}
				for (LispPtr m = localExpr.Right().Right(); !m.EndP(); m = m.Right())
				{
					LispRef value = ReplaceDot(m.Left().Right().Right().Left(), domainExpr);
					SubstituteExpr_impl(substBuff, value, mpf);
					if (substBuff.avs == AVS_SuspendedOrFailed) return {};
				}
				goto exit; // bufferValue stays empty
			}

			// following code duplicates partly the code in AbstrCalculator::SubstituteExpr
			const AbstrOperGroup* og = AbstrOperGroup::FindName(head.GetSymbID());
			assert(og);

			// arity-aware head dispatch: when NO operator member can accept this argument
			// count (FindOper would throw), a same-named function may serve the foreign
			// arity (prelude add/mul/or/and folds, log(x,base), median(a,b,c), ...)
			bool arityFallback = false;
			if (!og->IsTemplateCall())
			{
				UInt32 nrCallArgs = 0;
				for (LispPtr argPtr = localExpr.Right(); argPtr.IsRealList(); argPtr = argPtr.Right())
					++nrCallArgs;
				if (!og->AcceptsArity(nrCallArgs))
				{
					auto fnProbe = FindOrVisitItem(substBuff, head.GetSymbID());
					if (substBuff.avs == AVS_SuspendedOrFailed)
						return {};
					if (!fnProbe || !fnProbe->IsFunctionItem())
						if (auto pf = FindPreludeFunction(head.GetSymbID()))
							fnProbe = pf;
					arityFallback = fnProbe && fnProbe->IsFunctionItem();
				}
			}
			if (og->IsTemplateCall() || arityFallback)
			{
				if (head.GetSymbID() == t_Map)
					throwErrorF("ExprParser", "map(function, container) produces a container and can only appear as a whole calculation rule (e.g. 'container out := map(F, src);'), not as a sub-expression");

				auto templateItem = FindOrVisitItem(substBuff, head.GetSymbID());
				if (substBuff.avs == AVS_SuspendedOrFailed)
					return {};

				// the prelude is the implicit outermost namespace for call heads; a
				// non-callable shadow does not capture a call head either
				if (!templateItem || !templateItem->IsTemplate())
					if (auto pf = FindPreludeFunction(head.GetSymbID()))
						templateItem = pf;

				if (!templateItem)
					throwErrorF("ExprParser", "'{}': unknown function"
						, head.GetSymbStr().c_str()
					);

				if (templateItem->IsFunctionItem())
				{
					// function application as an expression: substitute the arguments in
					// the caller's scope, then beta-reduce the function body around them
					registerSupplier(substBuff, templateItem.get());

					if (substBuff.optionalVisitor)
					{
						// visit-only pass: record the argument suppliers, discard the result
						for (LispPtr argPtr = localExpr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
						{
							SubstituteExpr_impl(substBuff, LispRef(argPtr.Left()), metainfo_policy_flags::subst_allowed);
							if (substBuff.avs == AVS_SuspendedOrFailed)
								return {};
						}
						return {};
					}

					auto holder = m_Holder.lock();
					if (!holder)
						throwTaskCanceled();

					// WP3.4 definition-time check runs after variant resolution (a variant set
					// itself has no body); see below.
					auto resolveData = [&](LispPtr e) { return SubstituteExpr_impl(substBuff, LispRef(e), metainfo_policy_flags::subst_allowed); };
					auto findItem = [&](TokenID t) -> SharedTreeItem { return FindItem(t); };

					std::vector<CallArg> callArgs;
					for (LispPtr argPtr = localExpr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
					{
						callArgs.push_back(ResolveCallerArg(argPtr.Left(), resolveData, findItem, &substBuff, holder));
						if (substBuff.avs == AVS_SuspendedOrFailed)
							return {};
					}

					// §5.7: a variant set dispatches to the matching variant by argument type
					SharedTreeItem callee = templateItem;
					if (TreeItem_IsFunctionVariantSet(templateItem.get()))
					{
						auto variant = ResolveVariant(templateItem.get(), callArgs, holder);
						callee = make_shared_tree(variant, existing_obj{});
						registerSupplier(substBuff, variant);
					}

					CheckFunctionDefinition(callee.get()); // validate the (chosen) function body once

					FunctionBinding merged = MergeBinding(*MakeAllHoles(callee), callArgs);
					if (merged.NrHoles() != 0)
						throwErrorF("ExprParser", "'{}': a partial application can only be passed as an argument, not bound to an item"
							, head.GetSymbStr().c_str());
					bufferValue = ReduceMerged(merged, nullptr, &substBuff, holder);
					goto exit;
				}

				if (!templateItem->IsTemplate())
					throwErrorF("ExprParser", "'{}': found item '{}' is not defined as template"
						, head.GetSymbStr().c_str()
						, templateItem->GetFullName().c_str()
					);

				throwErrorF("ExprParser", "'{}': template instantiations (of '{}') not allowed as sub-expressions"
					, head.GetSymbStr().c_str()
					, templateItem->GetFullName().c_str()
				);
			}
			if (!og->MustCacheResult())
			{
				throwErrorF("ExprParser", "'{}': meta function call not allowed as sub-expressions"
					, head.GetSymbStr().c_str()
				);
			}

			// rewrite to at least make a context independent expr.
			auto args = SubstituteArgs(substBuff, localExpr.Right(), og, 0, {});
			if (substBuff.optionalVisitor)
				return {};

			localExpr = RewriteExprTop(
				LispRef(
					localExpr.Left() // ref to built-in operator
					, std::move(args)
				)
			);

			if (og->CanResultToConfigItem())
			{
				DataControllerRef dc = GetOrCreateDataController(localExpr);
				auto supplier = dc->MakeResult();
				if (!supplier)
				{
					dms_assert(dc->WasFailed(FailType::MetaInfo));
					auto holder = m_Holder.lock();
					if (!holder)
						throwTaskCanceled();
					holder->ThrowFail(dc.get());
				}

				if (!supplier->IsCacheItem())
				{
					localExpr = slSupplierExprImpl(substBuff, supplier.get(), mpf);
				}
			}
			bufferValue = localExpr;
			goto exit;
		}
		if (localExpr.IsSymb()) // reference to other tree-items, or value type as default unit
		{
			TokenID symbID = localExpr.GetSymbID();
			if (ValueClass::FindByScriptName(symbID))
				bufferValue = List(localExpr); // unitName -> [UnitName []] ofwel unitName().
			else
				bufferValue = slSupplierExpr(substBuff, localExpr, mpf);
			goto exit;
		}
		bufferValue = localExpr; // NumbDC, StrnDC, UI64DC
	}
exit:
	return bufferValue;
}

MetaInfo AbstrCalculator::SubstituteExpr(SubstitutionBuffer& substBuff, LispPtr localExpr) const
{
	DBG_START("ExprCalculator", "SubstituteExpr", false);
	DBG_TRACE(("localExpr = {}", AsString(localExpr).c_str()));

	if (localExpr.IsRealList()) // operator call or calculation scheme instantiation
	{
		assert(localExpr.Left().IsSymb());

		LispRef head = localExpr.Left();
		TokenID exprHeadID = head.GetSymbID();
		if (exprHeadID == token::arrow || exprHeadID == token::scope || exprHeadID == t_ApplyValue)
			goto skipTemplInst;

		// §5.9 explicit application forms
		if (exprHeadID == t_InstantiateItem || exprHeadID == t_ApplyItem)
		{
			LispRef innerCall = localExpr.Right().Left(); // (X args)
			MG_CHECK(innerCall.IsRealList() && innerCall.Left().IsSymb());
			TokenID calleeID = innerCall.Left().GetSymbID();
			auto callee = FindOrVisitItem(substBuff, calleeID);
			if (substBuff.avs == AVS_SuspendedOrFailed)
				return {};
			if (!callee || !callee->IsTemplate()) // functions are IsTemplate too
				throwErrorF("ExprParser", "'{}': '{}' is not a function or template"
					, exprHeadID == t_ApplyItem ? "apply" : "instantiate", calleeID.GetStr().c_str());

			if (exprHeadID == t_InstantiateItem)
			{
				// materialize all body items into the holder (copy-instantiation)
				registerSupplier(substBuff, callee.get());
				return MetaFuncCurry{ .fullLispExpr = innerCall, .applyItem = callee.get() };
			}
			// apply: the result value. For a function this is the bare call (inline).
			// For a template (decision 3): apply the template as an ad-hoc function -- bind
			// the provided arguments to its first N sub-items and beta-reduce its CI-unique
			// 'result' sub-item. Body names resolve nearest-scope within the template, then
			// through the template's own scope (definition scope, ancestors included) --
			// matching what a copy-instantiation without call-site fallback would see. Two
			// applies merge exactly iff their substituted keys coincide, so a body that
			// captures definition-scope names keys on what it captured.
			if (!callee->IsFunctionItem())
			{
				registerSupplier(substBuff, callee.get());

				if (substBuff.optionalVisitor)
				{
					// visit-only pass: record the argument suppliers, discard the result
					for (LispPtr a = innerCall.Right(); !a.EndP(); a = a.Right())
					{
						SubstituteExpr_impl(substBuff, LispRef(a.Left()), metainfo_policy_flags::subst_allowed);
						if (substBuff.avs == AVS_SuspendedOrFailed)
							return {};
					}
					return {};
				}

				auto holder = m_Holder.lock();
				if (!holder)
					throwTaskCanceled();

				auto resolveData = [&](LispPtr e) { return SubstituteExpr_impl(substBuff, LispRef(e), metainfo_policy_flags::subst_allowed); };
				auto findItem = [&](TokenID t) -> SharedTreeItem { return FindItem(t); };

				FunctionApplication appl;
				appl.m_FuncItem = callee.get();
				appl.m_SubstBuff = &substBuff;
				appl.m_ErrorHolder = holder;
				for (LispPtr a = innerCall.Right(); !a.EndP(); a = a.Right())
				{
					appl.PushArg(ResolveCallerArg(a.Left(), resolveData, findItem, &substBuff, holder));
					if (substBuff.avs == AVS_SuspendedOrFailed)
						return {};
				}
				return appl.Reduce();
			}
			localExpr = innerCall;
			head = localExpr.Left();
			exprHeadID = head.GetSymbID();
		}

		const AbstrOperGroup* og = AbstrOperGroup::FindName(exprHeadID);
		assert(og);
		if (og->IsTemplateCall())
		{
			if (exprHeadID == t_Map)
			{
				// map(function, container): register the function and source as suppliers,
				// then defer to InstantiateMap (populates the holder container)
				auto mapHolder = m_Holder.lock();
				if (mapHolder && (IsDataItem(mapHolder.get()) || IsUnit(mapHolder.get())))
					throwErrorF("ExprParser", "map(function, container) produces a container; bind it to a container item (e.g. 'container out := map(F, src);')");
				LispPtr margs = localExpr.Right();
				// register the mapped function as a supplier; for a partial application
				// map(F(k, _), src) register its head F and every item argument (k), skipping '_'
				if (!margs.EndP())
				{
					LispPtr fa = margs.Left();
					if (fa.IsSymb())
					{
						if (auto f = FindOrVisitItem(substBuff, fa.GetSymbID()))
							registerSupplier(substBuff, f.get());
					}
					else
						for (LispPtr e = fa; !e.EndP(); e = e.Right())
							if (e.Left().IsSymb() && e.Left().GetSymbID() != GetTokenID_st("_"))
								if (auto it = FindOrVisitItem(substBuff, e.Left().GetSymbID()))
									registerSupplier(substBuff, it.get());
				}
				if (!margs.EndP() && !margs.Right().EndP() && margs.Right().Left().IsSymb())
					if (auto s = FindOrVisitItem(substBuff, margs.Right().Left().GetSymbID()))
						registerSupplier(substBuff, s.get());
				if (substBuff.avs == AVS_SuspendedOrFailed)
					return {};
				if (substBuff.optionalVisitor)
					return {};
				return MetaFuncCurry{ .fullLispExpr = localExpr, .isMapCall = true };
			}

			auto templateItem = FindOrVisitItem(substBuff, head.GetSymbID());
			if (substBuff.avs == AVS_SuspendedOrFailed)
				return {};

			// the prelude is the implicit outermost namespace for call heads; a
			// non-callable shadow does not capture a call head either
			if (!templateItem || !templateItem->IsTemplate())
				if (auto pf = FindPreludeFunction(head.GetSymbID()))
					templateItem = pf;

			if (!templateItem)
				throwErrorF("ExprParser", "'{}': unknown operator and no template or function was found with this name"
					, head.GetSymbStr().c_str()
				);

			if (!templateItem->IsTemplate())
				throwErrorF("ExprParser", "'{}': found item '{}' is not defined as template"
					, head.GetSymbStr().c_str()
					, templateItem->GetFullName().c_str()
				);

			if (templateItem->IsFunctionItem())
			{
				if (TreeItem_IsFunctionVariantSet(templateItem.get()))
					goto skipTemplInst; // a variant set has no own params; arity is checked per variant at dispatch

				UInt32 nrDeclaredParams = TreeItem_GetFunctionParamCount(templateItem.get());
				bool hasRestP = TreeItem_HasFunctionRestParam(templateItem.get());
				UInt32 nrProvidedArgs = 0;
				for (LispPtr argPtr = localExpr.Right(); argPtr.IsRealList(); argPtr = argPtr.Right())
					++nrProvidedArgs;
				if (hasRestP ? nrProvidedArgs < nrDeclaredParams : nrProvidedArgs != nrDeclaredParams)
					throwErrorF("ExprParser", "'{}': function '{}' expects {}{} argument(s); {} provided"
						, head.GetSymbStr().c_str()
						, templateItem->GetFullName().c_str()
						, nrDeclaredParams
						, hasRestP ? " or more" : ""
						, nrProvidedArgs
					);

				// §5.9: a function application is always a value -- inline it by
				// beta-reduction, regardless of the holder. Binding to a container holder
				// is an error (no holder-driven instantiation): use 'instantiate F(…)' to
				// materialize the calculation steps, or bind to a typed item for the value.
				{
					auto holder = m_Holder.lock();
					if (holder && !IsDataItem(holder.get()) && !IsUnit(holder.get()) && !holder->IsCacheItem())
						throwErrorF("ExprParser", "'{}': a function application yields its result value; bind it to an attribute/parameter/unit, or use 'instantiate {}(…)' to materialize the calculation steps as items"
							, head.GetSymbStr().c_str(), head.GetSymbStr().c_str());
				}
				goto skipTemplInst;
			}

			// calculation scheme: isTempl, dont-subst templ
			registerSupplier(substBuff, templateItem.get());
			return MetaFuncCurry{ .fullLispExpr = localExpr, .applyItem = templateItem.get() };
		}
		if (!og->MustCacheResult())
			return MetaFuncCurry{ .fullLispExpr = localExpr, .og = og };
	}
skipTemplInst:
	return SubstituteExpr_impl(substBuff, localExpr, metainfo_policy_flags::is_root_expr);
}

auto AbstrCalculator::GetMetaInfo() const -> MetaInfo
{
	dms_check_not_debugonly;
	if (!m_HasSubstituted)
	{
		dms_assert(IsMetaThread());
		m_HasSubstituted = true; // before any throw

		MG_SIGNAL_ON_UPDATEMETAINFO


//		auto lispRefOrg = GetLispExprOrg();
		if (IsSourceRef())
		{
			auto sourceItem = GetSourceItem();
			sourceItem->UpdateMetaInfo();
			m_LispExprSubst = sourceItem;
		}
		else
		{
			SubstitutionBuffer substBuff(true);

			m_LispExprSubst = SubstituteExpr(substBuff, RewriteExpr(GetLispExprOrg()));

			// process registered suppliers
			std::vector<SharedTreeItem> supplierArrayCopy; supplierArrayCopy.swap(m_NamedSuppliers);
			UInt32 count = substBuff.m_SupplierSet.size();
			m_NamedSuppliers.resize(count);
			for (auto& tvPair : substBuff.m_SupplierSet)
			{
				assert(tvPair.second > 0);
				assert(tvPair.second <= count);
				m_NamedSuppliers[tvPair.second - 1] = make_shared_tree(tvPair.first, existing_obj{});
			}
		}
	}
	return m_LispExprSubst;
}

// *****************************************************************************
// Section:     C-API interface for AbstrCalculator
// *****************************************************************************

#include "TreeItemProps.h"
#include "TicInterface.h"

TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_PropertyValue(TreeItem* context, AbstrPropDef* propDef)
{
	DMS_CALL_BEGIN

		dms_assert(!SuspendTrigger::DidSuspend());
		SuspendTrigger::Resume();

		return IString::Create(TreeItemPropertyValue(context, propDef));

	DMS_CALL_END
	return nullptr;
}

// =============================================================================
// Opt-in "check all function definitions" audit -- see TicInterface.h. Config load
// is left untouched (the ordinary checker fires only on application); this walks the
// PARSE-created structure with the raw _GetFirstSubItem accessor, so NO whole-tree
// meta-update is forced -- only the reachable-from-result slice of each checked
// function is parsed, exactly as at an application. `insideUncheckableScope` tracks
// whether we are lexically inside a scope whose functions can be typed only on their
// instantiated/applied copy (which the application-time checker already covers), so
// checking them cold here would falsely fail (S1): an ordinary function's BODY (its
// nested closures capture the enclosing environment) and a TEMPLATE container (its
// functions reference template siblings that stay inert/`InTemplate` until
// instantiation). A variant SET and a plain container are transparent, so variant
// members and container-nested functions stay checkable.
// =============================================================================
static void CheckFunctionDefinitionsInSubtree(const TreeItem* item, bool insideUncheckableScope
	, FunctionCheckReporterFunc reporter, ClientHandle clientHandle, UInt32& nrFailed)
{
	for (const TreeItem* c = item->_GetFirstSubItem(); c; c = c->GetNextItem())
	{
		bool isFn         = c->IsFunctionItem();
		bool isVariantSet = isFn && TreeItem_IsFunctionVariantSet(c);
		bool isOrdinaryFn = isFn && !isVariantSet;

		if (isOrdinaryFn && !insideUncheckableScope)
		{
			bool ok = true;
			SharedStr msg;
			try
			{
				CheckFunctionDefinition(c);
			}
			catch (...)
			{
				ok = false;
				// CheckFunctionDefinition has already recorded the verdict ON 'c' (it becomes a
				// failed item and is not re-checked); read that reason for the audit report.
				auto fr = c->GetFailReason();
				msg = fr ? fr->Why() : SharedStr("function definition check failed");
			}
			if (!ok)
				++nrFailed;
			if (reporter)
				reporter(clientHandle, c, ok, ok ? "" : msg.c_str());
		}

		// Propagate the flag to c's children. An ordinary function's children are its
		// body; a TEMPLATE container's functions are template-internal (a function/variant
		// set is ITSELF a template -- SetIsFunction implies SetIsTemplate -- so exclude those
		// via !isFn to keep variant members and plain-template-less scopes checkable). A
		// variant SET and a plain container pass the flag through unchanged.
		bool isTemplateContainer = !isFn && c->IsTemplate();
		bool childInsideUncheckableScope = (isOrdinaryFn || isTemplateContainer) ? true : insideUncheckableScope;
		CheckFunctionDefinitionsInSubtree(c, childInsideUncheckableScope, reporter, clientHandle, nrFailed);
	}
}

TIC_CALL UInt32 CheckAllFunctionDefinitions(const TreeItem* root, FunctionCheckReporterFunc reporter, ClientHandle clientHandle)
{
	dms_assert(IsMetaThread());
	UInt32 nrFailed = 0;
	if (root)
		CheckFunctionDefinitionsInSubtree(root, false, reporter, clientHandle, nrFailed);
	return nrFailed;
}

