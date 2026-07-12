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
#include <map>
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


void MetaFuncCurry::operator ()(TreeItem* target, const AbstrCalculator* ac) const
{
	if (applyItem)
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

	struct FunctionApplication
	{
		const TreeItem*                m_FuncItem = nullptr;
		const FunctionApplication*     m_Parent = nullptr; // enclosing application (nested calls only): recursion detection follows THIS chain, so unrelated re-entry through UpdateMetaInfo of externals cannot raise false 'recursive' errors
		SubstitutionBuffer*            m_SubstBuff = nullptr; // caller's buffer: body-resolved externals register as suppliers of the calling item
		SharedTreeItem                 m_ErrorHolder; // caller item, for failure attribution
		std::vector<LispRef>           m_ArgKeys;
		std::vector<SharedTreeItem>    m_ArgItems;    // per arg: the referenced item iff the argument was a plain reference (enables member access), else null
		std::vector<const TreeItem*>   m_Params;      // the first N sub-items of m_FuncItem
		std::map<const TreeItem*, LispRef> m_Reductions;
		std::set<const TreeItem*>      m_InProgress;

		LispRef Reduce();
		LispRef ReduceBodyItem(const TreeItem* bodyItem);
		LispRef SubstituteBodyExpr(const TreeItem* refScope, LispPtr expr);
		LispRef ResolveBodySymbol(const TreeItem* refScope, TokenID symbID, SharedTreeItem* foundItemPtr);
	};

	LispRef FunctionApplication::Reduce()
	{
		assert(m_FuncItem && m_FuncItem->IsFunctionItem());
		assert(m_ErrorHolder);

		for (const FunctionApplication* ancestor = m_Parent; ancestor; ancestor = ancestor->m_Parent)
			if (ancestor->m_FuncItem == m_FuncItem)
				throwErrorF("ExprParser", "'{}': recursive function application is not supported"
					, m_FuncItem->GetFullName().c_str());

		UInt32 nrParams = TreeItem_GetFunctionParamCount(m_FuncItem);
		if (m_ArgKeys.size() != nrParams)
			throwErrorF("ExprParser", "'{}': function expects {} argument(s); {} provided"
				, m_FuncItem->GetFullName().c_str(), nrParams, m_ArgKeys.size());

		m_Params.clear(); m_Params.reserve(nrParams);
		const TreeItem* child = m_FuncItem->_GetFirstSubItem();
		for (UInt32 i = 0; i != nrParams; ++i, child = child->GetNextItem())
		{
			MG_CHECK(child); // guaranteed by the parser: params are the first nrParams sub-items
			m_Params.push_back(child);
			m_Reductions[child] = m_ArgKeys[i];
		}

		TokenID resultName = TreeItem_GetFunctionResultName(m_FuncItem);
		auto resultChild = m_FuncItem->GetConstSubTreeItemByID(resultName);
		if (!resultChild)
			throwErrorF("ExprParser", "'{}': designated result '{}' not found"
				, m_FuncItem->GetFullName().c_str(), resultName.GetStr().c_str());

		return ReduceBodyItem(resultChild.get());
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

			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			assert(og);
			if (og->IsTemplateCall())
			{
				// nested application of another function, resolved in the function's strict scope
				auto callee = m_FuncItem->FindItem(SharedStr(headID.AsStrRange()));
				if (!callee)
					throwErrorF("ExprParser", "'{}': unknown operator or function in body of function '{}'"
						, headID.GetStr().c_str(), m_FuncItem->GetFullName().c_str());
				if (!callee->IsFunctionItem())
					throwErrorF("ExprParser", "'{}': template instantiations are not supported inside function bodies"
						, headID.GetStr().c_str());

				FunctionApplication nested;
				nested.m_FuncItem = callee.get();
				nested.m_Parent = this;
				nested.m_SubstBuff = m_SubstBuff;
				nested.m_ErrorHolder = m_ErrorHolder;
				for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
				{
					LispPtr argExpr = argPtr.Left();
					SharedTreeItem argItem;
					LispRef argKey;
					if (argExpr.IsSymb() && !token::isConst(argExpr.GetSymbID()) && !ValueClass::FindByScriptName(argExpr.GetSymbID()))
						argKey = ResolveBodySymbol(refScope, argExpr.GetSymbID(), &argItem);
					else
						argKey = SubstituteBodyExpr(refScope, argExpr);
					nested.m_ArgKeys.push_back(std::move(argKey));
					nested.m_ArgItems.push_back(std::move(argItem));
				}
				return nested.Reduce();
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
						if (slash == e)
						{
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

		// imports and externals: resolve through the function's strict search space
		auto found = m_FuncItem->FindItem(fullStr);
		if (!found)
			throwErrorF("ExprParser", "'{}': unknown identifier in body of function '{}' (visible are: parameters, local items, and 'using' imports)"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
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

			// following code duplicates partly the code in AbstrCalculator::SubstituteExpr
			const AbstrOperGroup* og = AbstrOperGroup::FindName(head.GetSymbID());
			assert(og);
			if (og->IsTemplateCall())
			{
				auto templateItem = FindOrVisitItem(substBuff, head.GetSymbID());
				if (substBuff.avs == AVS_SuspendedOrFailed)
					return {};

				if (!templateItem)
					throwErrorF("ExprParser", "'{}': unknown function"
						, head.GetSymbStr().c_str()
					);

				if (templateItem->IsFunctionItem())
				{
					// function application as an expression: substitute the arguments in
					// the caller's scope, then beta-reduce the function body around them
					registerSupplier(substBuff, templateItem.get());

					FunctionApplication appl;
					appl.m_FuncItem = templateItem.get();
					for (LispPtr argPtr = localExpr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
					{
						LispPtr argExpr = argPtr.Left();
						SharedTreeItem argItem;
						if (argExpr.IsSymb() && !token::isConst(argExpr.GetSymbID())
							&& !ValueClass::FindByScriptName(argExpr.GetSymbID())
							&& !substBuff.optionalVisitor)
							argItem = FindItem(argExpr.GetSymbID()); // enables member access through structured parameters
						LispRef argKey = SubstituteExpr_impl(substBuff, LispRef(argExpr), metainfo_policy_flags::subst_allowed);
						if (substBuff.avs == AVS_SuspendedOrFailed)
							return {};
						appl.m_ArgKeys.push_back(std::move(argKey));
						appl.m_ArgItems.push_back(std::move(argItem));
					}
					if (substBuff.optionalVisitor)
						return {};

					appl.m_SubstBuff = &substBuff;
					appl.m_ErrorHolder = m_Holder.lock();
					if (!appl.m_ErrorHolder)
						throwTaskCanceled();
					bufferValue = appl.Reduce();
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
		if (exprHeadID == token::arrow || exprHeadID == token::scope)
			goto skipTemplInst;

		const AbstrOperGroup* og = AbstrOperGroup::FindName(exprHeadID);
		assert(og);
		if (og->IsTemplateCall())
		{
			auto templateItem = FindOrVisitItem(substBuff, head.GetSymbID());
			if (substBuff.avs == AVS_SuspendedOrFailed)
				return {};

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

				// a function application bound to a typed holder (attribute/parameter/unit)
				// is an expression: inline it by beta-reduction. Binding to a container
				// keeps the instantiating (template-like) form, giving access to all
				// body items of the instance.
				auto holder = m_Holder.lock();
				if (holder && (IsDataItem(holder.get()) || IsUnit(holder.get())))
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

