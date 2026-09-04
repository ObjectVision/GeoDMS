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
#include "HofClosure.h"

#include <algorithm>
#include <bitset>
#include <functional>
#include <tuple>
#include <map>
#include <memory>
#include <set>

// the typed-HOF layer, split by role over HofClosure/HofTypeUnifier/HofApplication/
// HofTypeChecker/HofOperSignatureInfer.cpp; this file drives it from SubstituteExpr
using namespace hof;

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

	SharedStr itemRefStr(itemRef.AsStrRangeLock());
	return SearchContext()->ResolveItemPath(itemRefStr);
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

	SharedStr itemRefStr(itemRef.AsStrRangeLock());

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

	SharedStr itemRefStr(supplRefID.AsStrRangeLock());
	if (Test(svf, SupplierVisitFlag::ImplSuppliers))
		return SearchContext()->FindAndVisitItem(itemRefStr, svf, visitor);
	auto searchResult = SearchContext()->ResolveItemPath(itemRefStr);
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

		auto phaseNumber = dc->GetPhaseNumber();
		// A failed determination is handled by the normal calculation path below, which can
		// report the underlying error. Only a successfully assigned fenced phase is the
		// indirect-expression error diagnosed here.
		if (phaseNumber != failed_phase_number && phaseNumber > 1)
		{
			const TreeItem* phaseResult = nullptr;
			calculator->VisitSuppliers(SupplierVisitFlag::NamedSuppliers,
				MakeDerivedProcVisitor([&phaseResult, phaseNumber](const Actor* supplier)
					{
						if (supplier->GetPhaseNumber() != phaseNumber)
							return;
						if (auto supplierItem = dynamic_cast<const TreeItem*>(supplier))
							phaseResult = supplierItem;
					}
				)
			);
			context->throwItemErrorF(
				"Indirect expression '{}' for {} requires PhaseContainer result {} (phase {}). "
				"Results supplied through a PhaseContainer cannot be used in indirect expressions.",
				resultStr,
				context->GetFullName().c_str(),
				phaseResult ? phaseResult->GetFullName().c_str() : "<indirect supplier chain>",
				phaseNumber);
		}

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

// An indirect expression is not scheduled: it has to be evaluated before the meta-info of the item
// carrying it can be, so it is calculated in place, under interest, and then dropped again. Whatever
// it read is dropped with it, and the next evaluation of the same indirection recalculates all of
// it. Where a PhaseContainer is among those suppliers that means re-running the whole fence:
// measured on a configuration whose indirect expression reads one fenced member, EVERY phase in it
// ran and reported four times over and one source item was calculated three times, against once when
// the expression read a non-fenced value instead.
//
// Keeping the data of what the expression reads is the cheaper strategy, and the engine can do it
// rather than every configuration having to annotate its indirections. Only the items the expression
// refers to DIRECTLY: their values are what a later evaluation needs again, whereas retaining the
// transitive supplier tree would hold far more resident than the evaluation ever touched. Units are
// left alone -- KeepData is about data, and a unit's range comes back with its definition.
static void KeepDataOfDirectlyReferredItems(const AbstrCalculator* calculator)
{
	assert(calculator);
	calculator->VisitSuppliers(SupplierVisitFlag::NamedSuppliers,
		MakeDerivedProcVisitor([](const Actor* suppl)
			{
				auto supplItem = dynamic_cast<const TreeItem*>(suppl);
				if (supplItem && IsDataItem(supplItem))
					const_cast<TreeItem*>(supplItem)->SetKeepDataState(true);
			}
		)
	);
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

		// before the evaluation below, so what it reads is still held when this indirection is
		// evaluated again -- see KeepDataOfDirectlyReferredItems
		KeepDataOfDirectlyReferredItems(calculator.get());

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
					throwErrorF("ExprParser", "Scope operator: container '{}' not found", leftExpr.GetSymbID());

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
						, innerCall.Left().GetSymbID());
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
					bufferValue = ReduceMerged(merged, nullptr, &substBuff, holder
						, (mpf & metainfo_policy_flags::is_root_expr) ? &substBuff.m_StructuredFunctionResult : nullptr);
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
					, exprHeadID == t_ApplyItem ? "apply" : "instantiate", calleeID);

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
	auto reduced = SubstituteExpr_impl(substBuff, localExpr, metainfo_policy_flags::is_root_expr);
	if (substBuff.m_StructuredFunctionResult)
		return MetaFuncCurry{ .fullLispExpr = std::move(reduced), .structuredResult = std::move(substBuff.m_StructuredFunctionResult) };
	if (reduced.IsRealList() && reduced.Left().IsSymb())
	{
		const AbstrOperGroup* reducedGroup = AbstrOperGroup::FindName(reduced.Left().GetSymbID());
		if (!reducedGroup->MustCacheResult() && reducedGroup->AllowsAsFunctionResult())
			return MetaFuncCurry{ .fullLispExpr = std::move(reduced), .og = reducedGroup };
	}
	return reduced;
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

