// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "AbstrCalculator.h"

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
#include "utl/mySPrintF.h"
#include "utl/SplitPath.h"
#include "xct/DmsException.h"
#include "xml/XmlOut.h"

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
#include "SessionData.h"
#include "SupplCache.h"
#include "UnitClass.h"

#include "LispContextHandle.h"
#include "TreeItemContextHandle.h"
#include "TreeItemClass.h"
#include "MoreDataControllers.h"
#include "DataArrayValue.h"

#include <algorithm>
#include <functional>
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

void MetaFuncCurry::operator ()(TreeItem* target, const AbstrCalculator* ac) const
{
	if (isMapCall)
		InstantiateMap(target, ac, fullLispExpr);
	else if (applyItem)
		InstantiateTemplate(target, applyItem, fullLispExpr.Right());
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
		if (!m_BestGuessErrorSuppl.first)
		{
			auto x = FindBestItem(supplRefID);
			if (x.first && !x.first->IsCacheItem() && x.first->WasFailed())
				m_BestGuessErrorSuppl = x;

			auto errMsg = MakeUnknownIdentifierErrorMsg(supplRefID.AsSharedStr(), x);
			auto holder = m_Holder.lock();
			if (!holder)
				throwTaskCanceled();
			holder->Fail(errMsg, FailType::MetaInfo);
		}
		return supplRef;
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
	// keys — never unresolved symbols — capture is hygienic by construction.
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
		std::map<const TreeItem*, LispRef> m_Reductions;
		std::set<const TreeItem*>      m_InProgress;

		void PushArg(const CallArg& a) { m_ArgKeys.push_back(a.key); m_ArgItems.push_back(a.item); m_ArgBindings.push_back(a.binding); m_ArgLiterals.push_back(a.literal); }

		LispRef Reduce();
		CallArg ReduceValue(); // §5.10: like Reduce, but a function-typed result yields a closure binding
		bool ResolveEnvSymbol(TokenID symbID, SharedTreeItem* foundItemPtr, LispRef* keyPtr, std::shared_ptr<FunctionBinding>* bindingPtr); // §5.10 closure-env lookup
		LispRef ReduceBodyItem(const TreeItem* bodyItem);
		LispRef SubstituteBodyExpr(const TreeItem* refScope, LispPtr expr);
		LispRef ResolveBodySymbol(const TreeItem* refScope, TokenID symbID, SharedTreeItem* foundItemPtr);
		CallArg ResolveBodyArg(const TreeItem* refScope, LispPtr argExpr);
		SharedTreeItem ResolveBodyHeadFunction(const TreeItem* refScope, TokenID headID, std::shared_ptr<FunctionBinding>* paramBinding);
	};

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

	// fill the holes of `b` with `holeFills` left-to-right; the counts must match
	FunctionBinding MergeBinding(const FunctionBinding& b, const std::vector<CallArg>& holeFills)
	{
		if (holeFills.size() != b.NrHoles())
			throwErrorF("ExprParser", "'{}': function expects {} argument(s); {} provided"
				, b.funcItem->GetFullName().c_str(), b.NrHoles(), holeFills.size());
		FunctionBinding r; r.funcItem = b.funcItem; r.env = b.env;
		UInt32 c = 0;
		for (const auto& slot : b.slots)
			r.slots.push_back(slot.isHole ? holeFills[c++] : slot);
		return r;
	}

	// §5.10: reduce a fully-bound application to its VALUE — a data key, or a closure
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
	// declared values-unit token — a value-type name — over resolving the unit)
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
			if (ancestor->m_FuncItem == m_FuncItem && ancestor->m_Env == m_Env)
				// same function AND same closure environment: §5.10 allows distinct
				// closures of one nested function within a single reduction chain
				throwErrorF("ExprParser", "'{}': recursive function application is not supported"
					, m_FuncItem->GetFullName().c_str());

		// §5.9 'apply T(args)': a plain template applied as an ad-hoc function — its params
		// are its first N sub-items with N = the number of provided arguments (the template
		// binding rule), and its designated result is the CI-unique 'result' sub-item
		bool isPlainTemplate = !m_FuncItem->IsFunctionItem();

		UInt32 nrParams = isPlainTemplate ? m_ArgKeys.size() : TreeItem_GetFunctionParamCount(m_FuncItem);
		if (m_ArgKeys.size() != nrParams)
			throwErrorF("ExprParser", "'{}': function expects {} argument(s); {} provided"
				, m_FuncItem->GetFullName().c_str(), nrParams, m_ArgKeys.size());
		if (isPlainTemplate)
		{
			UInt32 nrChildren = 0;
			for (const TreeItem* c = m_FuncItem->_GetFirstSubItem(); c; c = c->GetNextItem())
				++nrChildren;
			if (nrChildren < nrParams)
				throwErrorF("ExprParser", "'apply' on template '{}': {} argument(s) provided but the template has only {} sub-item(s)"
					, m_FuncItem->GetFullName().c_str(), nrParams, nrChildren);
		}

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

				// WP4.1: enforce the type application 'sig<V, D>' — the bound function's
				// CONCRETE positions constrain this application's type variables, shared
				// with (and checked against) the data-argument bindings below
				if (auto sigTypeArgs = TreeItem_GetFunctionParamSigTypeArgs(m_FuncItem, i))
					if (auto sigVars = TreeItem_GetFunctionTypeVars(declaredSig.get()); sigVars && sigTypeArgs->size() == sigVars->size())
						sigConstraints.push_back({ i, declaredSig, m_ArgBindings[i]->funcItem, sigVars, sigTypeArgs });
			}
		}

		// generic type variables: check constraint satisfaction and per-variable
		// consistency of the actual arguments' value classes; §5.10 Stage 2: domain
		// variables bind the arguments' DOMAIN UNITS and must agree across parameters
		std::map<TokenID, std::pair<const ValueClass*, const TreeItem*>> varBindings;
		std::map<TokenID, std::tuple<SharedTreeItem, const AbstrUnit*, const TreeItem*>> domainBindings; // keep-alive, unit, first param
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
				{
					auto& db = domainBindings[gpVar];
					if (std::get<1>(db) && !std::get<1>(db)->UnifyDomain(du, "", "", UnifyMode(UM_AllowVoidRight)))
						throwErrorF("ExprParser", "'{}': inconsistent instantiation of domain variable '{}': the domains of the arguments for parameters '{}' and '{}' differ"
							, m_FuncItem->GetFullName().c_str(), gpVar.GetStr().c_str()
							, std::get<2>(db)->GetID().GetStr().c_str(), gpParam->GetID().GetStr().c_str());
					if (!std::get<1>(db))
						db = { argResult, du, gpParam };
				}
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
			if (!MatchesGenericConstraint(vt, gpConstraint))
				throwErrorF("ExprParser", "'{}': argument of type {} for parameter '{}' does not satisfy '{}: {}'"
					, m_FuncItem->GetFullName().c_str()
					, vt->GetName().c_str()
					, gpParam->GetID().GetStr().c_str()
					, gpVar.GetStr().c_str(), gpConstraint.GetStr().c_str());
			auto& binding = varBindings[gpVar];
			if (binding.first && binding.first != vt)
				throwErrorF("ExprParser", "'{}': inconsistent instantiation of type variable '{}': {} (parameter '{}') vs {} (parameter '{}')"
					, m_FuncItem->GetFullName().c_str()
					, gpVar.GetStr().c_str()
					, binding.first->GetName().c_str(), binding.second->GetID().GetStr().c_str()
					, vt->GetName().c_str(), gpParam->GetID().GetStr().c_str());
			if (!binding.first)
				binding = { vt, gpParam };
		}

		// WP4.1: merge signature-instantiation constraints — for each 'sig<V, D>'-typed
		// parameter, the bound function's CONCRETE positions (declared value classes,
		// resolvable declared domains) instantiate the applied variables; generic
		// positions of the bound function constrain nothing
		auto outerVars = TreeItem_GetFunctionTypeVars(m_FuncItem);
		for (const auto& sc : sigConstraints)
		{
			std::map<TokenID, TokenID> sig2outer;
			for (SizeT k = 0; k != sc.sigVars->size(); ++k)
				sig2outer[(*sc.sigVars)[k].first] = (*sc.typeArgs)[k];

			const TreeItem* viaParam = m_Params[sc.paramIndex];
			auto constrainPos = [&](const TreeItem* sigPos, const TreeItem* fnPos)
			{
				if (!sigPos || !fnPos || !IsDataItem(sigPos) || !IsDataItem(fnPos))
					return;

				auto itV = sig2outer.find(AsDataItem(sigPos)->ValuesUnitToken());
				if (itV != sig2outer.end())
					if (auto vc = ParamValueClass(fnPos))
					{
						if (outerVars)
							for (const auto& ov : *outerVars)
								if (ov.first == itV->second && !MatchesGenericConstraint(vc, ov.second))
									throwErrorF("ExprParser", "'{}': function '{}' bound to parameter '{}' instantiates '{}' as {}, which does not satisfy '{}: {}'"
										, m_FuncItem->GetFullName().c_str(), sc.boundFn->GetFullName().c_str()
										, viaParam->GetID().GetStr().c_str(), itV->second.GetStr().c_str()
										, vc->GetName().c_str(), itV->second.GetStr().c_str(), ov.second.GetStr().c_str());
						auto& b = varBindings[itV->second];
						if (b.first && b.first != vc)
							throwErrorF("ExprParser", "'{}': inconsistent instantiation of type variable '{}': {} (from function '{}' bound to parameter '{}') vs {} (via parameter '{}')"
								, m_FuncItem->GetFullName().c_str(), itV->second.GetStr().c_str()
								, vc->GetName().c_str(), sc.boundFn->GetFullName().c_str(), viaParam->GetID().GetStr().c_str()
								, b.first->GetName().c_str(), b.second->GetID().GetStr().c_str());
						if (!b.first)
							b = { vc, viaParam };
					}

				auto itD = sig2outer.find(AsDataItem(sigPos)->DomainUnitToken());
				if (itD != sig2outer.end())
				{
					TokenID fnDU = AsDataItem(fnPos)->DomainUnitToken();
					if (fnDU && !IsGenericVarOf(sc.boundFn.get(), fnDU))
						if (auto defP = sc.boundFn->GetTreeParent())
							if (auto u = defP->FindItem(SharedStr(fnDU.AsStrRange())); u && IsUnit(u.get()))
							{
								auto& db = domainBindings[itD->second];
								if (std::get<1>(db) && !std::get<1>(db)->UnifyDomain(AsUnit(u.get()), "", "", UnifyMode(UM_AllowVoidRight)))
									throwErrorF("ExprParser", "'{}': inconsistent instantiation of domain variable '{}': the domain declared by function '{}' (bound to parameter '{}') differs from the domain bound via parameter '{}'"
										, m_FuncItem->GetFullName().c_str(), itD->second.GetStr().c_str()
										, sc.boundFn->GetFullName().c_str(), viaParam->GetID().GetStr().c_str()
										, std::get<2>(db)->GetID().GetStr().c_str());
								if (!std::get<1>(db))
									db = { u, AsUnit(u.get()), viaParam };
							}
				}
			};

			const TreeItem* sp = sc.sig->_GetFirstSubItem();
			const TreeItem* fp = sc.boundFn->_GetFirstSubItem();
			for (UInt32 k = 0, n = TreeItem_GetFunctionParamCount(sc.sig.get()); k != n && sp && fp; ++k, sp = sp->GetNextItem(), fp = fp->GetNextItem())
				constrainPos(sp, fp);
			constrainPos(
				sc.sig->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(sc.sig.get())).get(),
				sc.boundFn->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(sc.boundFn.get())).get());
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

		// §5.10: a function-typed result yields a closure — the nested function plus
		// this application's bound parameters, captured by value
		if (resultChild->IsFunctionItem())
		{
			auto env = std::make_shared<ClosureEnv>();
			env->funcItem = m_FuncItem;
			env->args.reserve(nrParams);
			for (UInt32 i = 0; i != nrParams; ++i)
			{
				CallArg a;
				a.key = m_ArgKeys[i]; a.item = m_ArgItems[i];
				a.binding = m_ArgBindings[i]; a.literal = m_ArgLiterals[i];
				env->args.push_back(std::move(a));
			}
			env->next = m_Env;

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
			if (og->IsTemplateCall())
			{
				// a function application in a data (body-expression) position: resolve the
				// head to a function value (a function-valued parameter's binding, or a
				// plain import), fill its holes with the call arguments, and reduce; a
				// residual (partially applied) result cannot stand in a data position.
				std::shared_ptr<FunctionBinding> paramBinding;
				auto headFn = ResolveBodyHeadFunction(refScope, headID, &paramBinding);
				FunctionBinding calleeBinding = paramBinding ? *paramBinding : *MakeAllHoles(headFn);

				std::vector<CallArg> holeFills;
				for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
					holeFills.push_back(ResolveBodyArg(refScope, argPtr.Left()));

				FunctionBinding merged = MergeBinding(calleeBinding, holeFills);
				if (merged.NrHoles() != 0)
					throwErrorF("ExprParser", "'{}': a partial application can only be passed as an argument, not used as a value"
						, headID.GetStr().c_str());
				return ReduceMerged(merged, this, m_SubstBuff, m_ErrorHolder);
			}
			if (!og->MustCacheResult())
				throwErrorF("ExprParser", "'{}': meta function call is not supported inside function bodies"
					, headID.GetStr().c_str());

			// ordinary operator application: substitute the arguments
			std::vector<LispRef> substArgs;
			for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
				substArgs.push_back(SubstituteBodyExpr(refScope, argPtr.Left()));

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
		// including the function item — matching the resolution order of the instantiated form
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
						// §5.9 parameter bound to a container literal: reduce a bare use to the
						// domain and 'param/member' to the named member value — no arg item exists
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
					target = FindSubItem(child.get(), SharedStr(CharPtrRange(slash + 1, e)));
					if (!target)
						throwErrorF("ExprParser", "'{}': not found in body of function '{}'"
							, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
				}
				if (foundItemPtr)
					*foundItemPtr = nullptr; // reduced local: no item identity to bind member access to
				return ReduceBodyItem(target.get());
			}
			if (atFuncRoot)
				break;
		}

		// §5.10: the captured closure environment — the enclosing applications' bound
		// parameters — is lexically nearer than any import or definition-scope item
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
		// visible from the point of definition; the call site stays invisible)
		auto found = m_FuncItem->FindItem(fullStr);
		if (!found)
			if (auto defParent = m_FuncItem->GetTreeParent())
				found = defParent->FindItem(fullStr);
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
			throwErrorF("ExprParser", "'{}': reference to (part of) a template or function from body of function '{}'"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		found->UpdateMetaInfo();
		if (m_SubstBuff)
			registerSupplier(*m_SubstBuff, found.get());
		if (foundItemPtr)
			*foundItemPtr = found;
		return found->GetCheckedKeyExpr();
	}

	// resolve a body-call head to the function being applied; sets *paramBinding when the
	// head is a function-valued parameter (so its pre-bound slots participate).
	SharedTreeItem FunctionApplication::ResolveBodyHeadFunction(const TreeItem* /*refScope*/, TokenID headID, std::shared_ptr<FunctionBinding>* paramBinding)
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
		if (!callee)
			throwErrorF("ExprParser", "'{}': unknown operator or function in body of function '{}'"
				, headID.GetStr().c_str(), m_FuncItem->GetFullName().c_str());
		if (!callee->IsFunctionItem())
			throwErrorF("ExprParser", "'{}': template instantiations are not supported inside function bodies"
				, headID.GetStr().c_str());
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
				FunctionBinding calleeBinding = pb ? *pb : *MakeAllHoles(headFn);
				std::vector<CallArg> sub;
				for (LispPtr a = argExpr.Right(); !a.EndP(); a = a.Right())
					sub.push_back(ResolveBodyArg(refScope, a.Left()));
				FunctionBinding merged = MergeBinding(calleeBinding, sub);
				if (merged.NrHoles() == 0)
					return ReduceMergedValue(merged, this, m_SubstBuff, m_ErrorHolder); // §5.10: data key OR closure binding
				CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
			}
		}
		CallArg a; a.key = SubstituteBodyExpr(refScope, argExpr); return a;
	}

	// WP3.4: definition-time scope/shape validation. A lightweight, argument-independent
	// walk of the body reachable from the designated result: every identifier must
	// resolve (parameter, local, or import), operator/function heads must be known, and
	// direct function calls must have the right arity. It deliberately does NOT check
	// types/units/metrics/generics or member existence (those need actual arguments and
	// are verified per application by the reduction). Runs once per function.
	struct FunctionChecker
	{
		const TreeItem*              m_FuncItem = nullptr;
		std::vector<const TreeItem*> m_Params;
		std::set<const TreeItem*>    m_InProgress;

		void CheckBodyItem(const TreeItem* refItem);
		void CheckExpr(const TreeItem* refScope, LispPtr expr);
		// 0=parameter, 1=local (via *local), 2=import/external; throws on unknown
		int ResolveName(const TreeItem* refScope, TokenID sym, const TreeItem** local);
	};

	int FunctionChecker::ResolveName(const TreeItem* refScope, TokenID sym, const TreeItem** local)
	{
		if (local) *local = nullptr;
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
				for (auto p : m_Params)
					if (p == child.get())
						return 0; // parameter (member access is not verified at definition time)
				const TreeItem* target = child.get();
				if (slash != e)
				{
					auto t = FindSubItem(child.get(), SharedStr(CharPtrRange(slash + 1, e)));
					if (!t)
						throwErrorF("ExprParser", "'{}': not found in body of function '{}'"
							, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
					target = t.get();
				}
				if (local) *local = target;
				return 1;
			}
			if (atFuncRoot)
				break;
		}

		auto found = m_FuncItem->FindItem(fullStr);
		if (!found)
			if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
				found = defParent->FindItem(fullStr);
		if (!found && slash == e)
			if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
				return 2; // prelude: implicit outermost namespace, also for function references
		if (!found)
			throwErrorF("ExprParser", "'{}': unknown identifier in body of function '{}' (visible are: parameters, local items, 'using' imports, and the definition scope)"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		if (!found->IsFunctionItem() && found->InTemplate())
			throwErrorF("ExprParser", "'{}': reference to (part of) a template or function from body of function '{}'"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		return 2;
	}

	void FunctionChecker::CheckExpr(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.EndP())
			return;
		if (!expr.IsRealList())
		{
			if (expr.IsSymb())
			{
				TokenID sym = expr.GetSymbID();
				if (sym == t_Hole || token::isConst(sym) || ValueClass::FindByScriptName(sym))
					return;
				const TreeItem* local = nullptr;
				if (ResolveName(refScope, sym, &local) == 1 && local)
					CheckBodyItem(local);
			}
			return; // numeric / string / uint literals
		}

		TokenID headID = expr.Left().GetSymbID();
		if (headID == token::sourceDescr)
		{
			CheckExpr(refScope, expr.Right().Left());
			return;
		}
		if (headID == token::arrow || headID == token::scope || headID == token::subitem)
			throwErrorF("ExprParser", "the '{}' construct is not yet supported inside inlined function bodies"
				"; bind the function application to a container to use the instantiating form"
				, headID.GetStr().c_str());

		// §5.10 applied call result: check the sub-expressions; arity of the application
		// is argument-dependent (the inner value's residual params) — verified at reduction
		if (headID == t_ApplyValue)
		{
			for (LispPtr a = expr.Right(); !a.EndP(); a = a.Right())
				CheckExpr(refScope, a.Left());
			return;
		}

		const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
		if (og->IsTemplateCall() && !ValueClass::FindByScriptName(headID)) // value-type heads (float64(x)) are conversions, not function calls
		{
			if (headID == t_Map)
				throwErrorF("ExprParser", "map(...) can only appear as a whole calculation rule, not as a sub-expression");
			bool isParam = false;
			for (auto p : m_Params)
				if (p->GetID() == headID) { isParam = true; break; }
			if (!isParam)
			{
				// a direct function/import call: validate that it is a function of the right arity
				auto callee = m_FuncItem->FindItem(SharedStr(headID.AsStrRange()));
				if (!callee || !callee->IsFunctionItem())
					if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
						if (auto lex = defParent->FindItem(SharedStr(headID.AsStrRange())); lex && lex->IsFunctionItem())
							callee = lex;
				if (!callee || !callee->IsFunctionItem())
					if (auto pf = FindPreludeFunction(headID); pf && pf->IsFunctionItem())
						callee = pf; // prelude: implicit outermost namespace for call heads
				if (!callee)
					throwErrorF("ExprParser", "'{}': unknown operator or function in body of function '{}'"
						, headID.GetStr().c_str(), m_FuncItem->GetFullName().c_str());
				if (!callee->IsFunctionItem())
					throwErrorF("ExprParser", "'{}': template instantiations are not supported inside function bodies"
						, headID.GetStr().c_str());
				UInt32 nrArgs = 0; bool anyHole = false;
				for (LispPtr a = expr.Right(); !a.EndP(); a = a.Right())
				{
					++nrArgs;
					if (a.Left().IsSymb() && a.Left().GetSymbID() == t_Hole) anyHole = true;
				}
				UInt32 nrCalleeParams = TreeItem_GetFunctionParamCount(callee.get());
				if (!anyHole && nrArgs != nrCalleeParams)
					throwErrorF("ExprParser", "'{}': function '{}' expects {} argument(s); {} provided"
						, headID.GetStr().c_str(), callee->GetFullName().c_str(), nrCalleeParams, nrArgs);
			}
		}
		for (LispPtr a = expr.Right(); !a.EndP(); a = a.Right())
			CheckExpr(refScope, a.Left());
	}

	void FunctionChecker::CheckBodyItem(const TreeItem* refItem)
	{
		if (!m_InProgress.insert(refItem).second)
			return; // cycle guard; true circularity is caught by the reduction
		SharedStr exprStr = refItem->GetExpr();
		if (!exprStr.empty())
		{
			if (AbstrCalculator::MustEvaluate(exprStr.c_str()))
				throwErrorF("ExprParser", "'{}': leading-'=' string indirection is not supported inside function bodies"
					, refItem->GetFullName().c_str());
			auto calc = AbstrCalculator::ConstructFromStr(refItem, exprStr, CalcRole::Calculator);
			auto refScope = refItem->GetTreeParent();
			CheckExpr(refScope.get(), RewriteExpr(calc->GetLispExprOrg()));
		}
		m_InProgress.erase(refItem);
	}

	void CheckFunctionDefinition(const TreeItem* funcItem)
	{
		if (TreeItem_IsFunctionDefinitionChecked(funcItem))
			return;
		TokenID resultName = TreeItem_GetFunctionResultName(funcItem);
		auto resultChild = funcItem->GetConstSubTreeItemByID(resultName);
		if (!resultChild)
			throwErrorF("ExprParser", "'{}': designated result '{}' not found"
				, funcItem->GetFullName().c_str(), resultName.GetStr().c_str());
		if (!resultChild->GetExpr().empty()) // signature-only functions have no body to check
		{
			FunctionChecker chk;
			chk.m_FuncItem = funcItem;
			UInt32 nrParams = TreeItem_GetFunctionParamCount(funcItem);
			const TreeItem* p = funcItem->_GetFirstSubItem();
			for (UInt32 i = 0; i < nrParams && p; ++i, p = p->GetNextItem())
				chk.m_Params.push_back(p);
			chk.CheckBodyItem(resultChild.get());
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

// WP3.3: map(function, container) — populate `holder` with one child per data-item /
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
	if (!fExpr.IsSymb())
		holder->throwItemError("map: the first argument must be the name of a function");
	if (!srcExpr.IsSymb())
		holder->throwItemError("map: the second argument must be the name of a container");

	auto funcItem = ac->FindItem(fExpr.GetSymbID());
	if (!funcItem || !funcItem->IsFunctionItem())
		holder->throwItemErrorF("map: '{}' is not a function", fExpr.GetSymbID().GetStr().c_str());
	if (TreeItem_GetFunctionParamCount(funcItem.get()) != 1)
		holder->throwItemErrorF("map: function '{}' must take exactly one parameter to be mapped over a container"
			, funcItem->GetFullName().c_str());

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
		CallArg a; a.key = c->GetCheckedKeyExpr(); a.item = make_shared_tree(c, existing_obj{});
		appl.PushArg(a);
		LispRef key = appl.Reduce();

		auto child = holder->CreateItem(c->GetID());
		child->SetCalculator(AbstrCalculator::ConstructFromLispRef(child.get(), key, CalcRole::Calculator));
	}
	holder->SetIsInstantiated();
}

#include "Operator.h"
#include "MoreDataControllers.h"

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
				// skip condition argument for select_with_attr_xxxx meta functions
				assert(currArg == 1); // only this one
				assert(cursor.Right().EndP()); // no next args, argSeq must remain consistent with the first args..
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
			if (og->IsTemplateCall())
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
			// For a template (decision 3): apply the template as an ad-hoc function — bind
			// the provided arguments to its first N sub-items and beta-reduce its CI-unique
			// 'result' sub-item. Body names resolve nearest-scope within the template, then
			// through the template's own scope (definition scope, ancestors included) —
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
				if (!margs.EndP() && margs.Left().IsSymb())
					if (auto f = FindOrVisitItem(substBuff, margs.Left().GetSymbID()))
						registerSupplier(substBuff, f.get());
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
				UInt32 nrProvidedArgs = 0;
				for (LispPtr argPtr = localExpr.Right(); argPtr.IsRealList(); argPtr = argPtr.Right())
					++nrProvidedArgs;
				if (nrProvidedArgs != nrDeclaredParams)
					throwErrorF("ExprParser", "'{}': function '{}' expects {} argument(s); {} provided"
						, head.GetSymbStr().c_str()
						, templateItem->GetFullName().c_str()
						, nrDeclaredParams
						, nrProvidedArgs
					);

				// §5.9: a function application is always a value — inline it by
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

