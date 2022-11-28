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

#include "AbstrCalculator.h"

#include "RtcInterface.h"
#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "dbg/Debug.h"
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


// *****************************************************************************
// Section:    AbstrValuesUnit for arrow, move to generic display location
// *****************************************************************************

const AbstrUnit* AbstrValuesUnit(const AbstrDataItem* adi)
{
	dms_assert(adi);
	while (true)
	{
		auto au = adi->GetAbstrValuesUnit();
		if (!au->IsDefaultUnit())
			return au;
		adi = AsDataItem( adi->GetCurrRefItem() );
		if (!adi)
			return nullptr;
	}
}

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

TIC_CALL void AbstrCalculator::WriteHtmlExpr(OutStreamBase& stream) const 
{ 
	stream.WriteValue(GetExpr().c_str());
}

//----------------------------------------------------------------------
// Definition of public helper funcs
//----------------------------------------------------------------------

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
/*
auto GetItemRefForSubItem(const TreeItem* sourceItem, SharedStr relPath) -> item_ref_type
{
	if (sourceItem->IsCacheItem())
	{
		return slSubItemCall(sourceItem->)
	}
	auto end = relPath.send();
	auto begin = relPath.begin();
	while (true)
	{
		dms_assert(!sourceItem->IsCacheItem());
		if (begin == end)
			return { sourceItem, TokenID() };
		auto delimPos = begin;
		while (delimPos != end && *delimPos != DELIMITER_CHAR)
			++delimPos;
		auto subItem = sourceItem->GetConstSubTreeItemByID(GetTokenID_mt(begin, delimPos));
		if (subItem->IsCacheItem())
			return item_ref_type{ sourceItem, GetTokenID_mt( begin, end ) };
		sourceItem = subItem;
		begin = delimPos; 
		if (begin != end)
		{
			dms_assert(*begin == DELIMITER_CHAR);
			++begin;
		}
	}
}
*/
SharedTreeItem FindSubItem(const TreeItem* sourceItem, SharedStr relPath)
{
	auto end = relPath.send();
	auto begin = relPath.begin();
	while (true)
	{
		dms_assert(!sourceItem->IsCacheItem());
		if (begin == end)
			return sourceItem;
		auto delimPos = begin;
		while (delimPos != end && *delimPos != DELIMITER_CHAR)
			++delimPos;
		auto subItem = sourceItem->GetConstSubTreeItemByID(GetTokenID_mt(begin, delimPos));
		if (!subItem)
			throwErrorF("FindSubItem", "Cannot find %s from %s", SharedStr(begin, delimPos), sourceItem->GetFullName().c_str());
		MG_CHECK(!subItem->IsCacheItem());
		sourceItem = subItem;
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
		throwErrorF("GetDC", "KeyExpr expected but called with: %s\nMeta functions and template instantiations are not supported here."
			, AsFLispSharedStr(std::get<0>(metaInfo).GetAsLispRef()).c_str()
		);
	if (metaInfo.index() == 2) // follow source
		return GetOrCreateDataController(std::get<2>(metaInfo)->GetCheckedKeyExpr());
	return GetOrCreateDataController(std::get<1>(metaInfo));
}

auto MakeResult(const AbstrCalculator* calculator)->make_result_t
{
	auto dc = GetDC(calculator);
	dc->MakeResult();
	//	auto resFuture = dc->CalcResult();

	return dc; // return owner of potential future.
}

auto CalcResult(const AbstrCalculator* calculator, const Class* cls, Explain::Context* context)->calc_result_t
{
	auto dc = MakeResult(calculator);
	dms_assert(dc);
	dms_assert(dc->GetOld() || dc->WasFailed(FR_MetaInfo));
	if (dc->WasFailed(FR_MetaInfo))
		return dc;
	CheckResultingTreeItem(dc->GetOld(), cls);
	auto result = dc->CalcResult(context);
	if (SuspendTrigger::DidSuspend())
		return {};
	if (!result)
	{
		assert(dc->WasFailed(FR_Data));
		dc->ThrowFail();
	}
	return result;
}

void CheckResultingTreeItem(const TreeItem* refItem, const Class* desiredResultingClass)
{
	dms_assert(refItem);

	if (desiredResultingClass && !refItem->GetDynamicObjClass()->IsDerivedFrom(desiredResultingClass))
	{
		throwErrorF("CheckResult", "calculation will result in a %s, which is not castable to the type %s of the result item",
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
	else
	{
		dms_assert(og);
		ApplyAsMetaFunction(target, ac, og, fullLispExpr.Right());
	}
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
	:	m_Holder(context)
	,	m_CalcRole(cr)
{
	if (context)
		m_SearchContext = GetSearchContext(context, cr);

	if (!m_SearchContext)
		m_SearchContext = SessionData::Curr()->GetConfigRoot();
	dms_assert(m_SearchContext);
}

AbstrCalculator::AbstrCalculator(const TreeItem* context, LispPtr lispRefOrg, CalcRole cr)
	: m_Holder(context)
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
	dms_assert(m_SearchContext);
}


AbstrCalculator::~AbstrCalculator()
{}

bool AbstrCalculator::CheckSyntax () const
{
	return true;
}

const TreeItem* AbstrCalculator::SearchContext() const
{
	return m_SearchContext;
}

static TokenID thisToken = GetTokenID_st("this");

const TreeItem* AbstrCalculator::FindItem(TokenID itemRef) const
{
	dms_assert(!itemRef.empty());
	dms_assert(m_Holder);

	MG_SIGNAL_ON_UPDATEMETAINFO

		if (itemRef == thisToken)
			return m_Holder;

	SharedStr itemRefStr(itemRef.AsStrRange());

	return SearchContext()->FindItem(itemRefStr);
}

BestItemRef AbstrCalculator::FindBestItem(TokenID itemRef) const
{
	dms_assert(!itemRef.empty());
	dms_assert(m_Holder);

	MG_SIGNAL_ON_UPDATEMETAINFO

		if (itemRef == thisToken)
			return { nullptr, {} };

	SharedStr itemRefStr(itemRef.AsStrRange());

	return SearchContext()->FindBestItem(itemRefStr);
}

auto AbstrCalculator::GetSourceItem() const -> SharedTreeItem  // directly referred persistent object.
{
	dms_assert(IsMetaThread());
	dms_assert(IsSourceRef());

	TokenID supplRefID = GetLispExprOrg().GetSymbID();
	auto foundItem = FindItem(supplRefID);
	if (!foundItem)
	{
		auto errMsg = mySSPrintF("cannot find %s", supplRefID.GetStr().c_str());
		throwDmsErrD(errMsg.c_str());
	}
	return foundItem;
}

bool AbstrCalculator::IsSourceRef() const
{
	return GetLispExprOrg().IsSymb() 
		&& !ValueClass::FindByScriptName(GetLispExprOrg().GetSymbID());
}

AcConstructor* s_Constructor = nullptr;

AcConstructor* AbstrCalculator::GetConstructor()
{
	dms_assert(s_Constructor); // CalcualtionRule parsing module must be registered
	return s_Constructor;
}

void AbstrCalculator::SetConstructor(AcConstructor* constructor)
{
	s_Constructor = constructor;
}

bool AbstrCalculator::HasTemplSource() const
{
	auto metaInfo = GetMetaInfo();
	return metaInfo.index() == 0 && std::get<MetaFuncCurry>(metaInfo).applyItem != nullptr;
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
	dms_assert(std::get<MetaFuncCurry>(metaInfo).og);
	return std::get<MetaFuncCurry>(metaInfo).og->HasTemplArg();
}

const TreeItem* AbstrCalculator::GetForEachTemplSource() const
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
	SubstitutionBuffer substBuffer;
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
	MG_CHECK(false);
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
	DBG_TRACE(("lispExpr %s", AsFLispSharedStr(lispExpr).c_str()));

	return new DC_Ptr(context, lispExpr, cr);
}

AbstrCalculatorRef AbstrCalculator::ConstructFromDBT(AbstrDataItem* context, const AbstrCalculator* src)
{
	return GetConstructor()->ConstructDBT(context, src);
}

SharedStr AbstrCalculator::GetAsFLispExprOrg() const
{
	return AsFLispSharedStr(GetLispExprOrg());
}

SharedStr AbstrCalculator::GetAsFLispExpr() const
{
	auto metaInfo = GetMetaInfo();
	return AsFLispSharedStr(GetAsLispRef(metaInfo));
}

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
	UInt32 nrEvals = CountIndirections(exprPtr.first);
	if (!nrEvals)
		return {};

	exprPtr.first += nrEvals;

	dms_assert(IsMetaThread());
	dms_assert(nrEvals); // else MustEvaluate would have returned false; PRECONDITION

	dms_assert(!MustEvaluate(exprPtr.begin()));
	FencedInterestRetainContext irc;
	SuspendTrigger::FencedBlocker lockSuspend;

	SharedStr resultStr(exprPtr);
	dms_assert(!MustEvaluate(resultStr.begin()));
	if (!context->InTemplate())
		while (nrEvals-- && !resultStr.empty())
		{
			AbstrCalculatorRef calculator = ConstructFromDirectStr(context, resultStr, CalcRole::Other);
			assert(calculator);
			auto res = CalcResult(calculator, DataArray<SharedStr>::GetStaticClass());
			assert(res);
			if (WasInFailed(res.get_ptr()))
				return calculator->FindErrorneousItem();

			auto resItem = res->GetOld();

			irc.Add(res.get_ptr());
			irc.Add(resItem);

			const AbstrDataItem* resDataItem = AsDataItem(resItem);
			dms_assert(resDataItem);


			if (WasInFailed(resDataItem))
				return calculator->FindErrorneousItem();
			resultStr = GetValue<SharedStr>(resDataItem, 0);

			UInt32 nrNewEvals = CountIndirections(resultStr.c_str());
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
	UInt32 nrEvals = CountIndirections(exprPtr);
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
	FencedInterestRetainContext irc;
	SuspendTrigger::FencedBlocker lockSuspend;

	SharedStr resultStr(expr);
	if (!context->InTemplate())
	while (nrEvals-- && !resultStr.empty())
	{
		AbstrCalculatorRef calculator = ConstructFromDirectStr(context, resultStr, cr);
		auto res = CalcResult(calculator, DataArray<SharedStr>::GetStaticClass());
		dms_assert(res);
		if (res->WasFailed(FR_Data))
			res->ThrowFail();

		auto resItem = res->GetOld();

		irc.Add(res.get_ptr());
		irc.Add(resItem);

		const AbstrDataItem* resDataItem = AsDataItem(resItem);
		dms_assert(resDataItem || res->WasFailed(FR_Data));

		if (res->WasFailed(FR_Data))
			res->ThrowFail();

		dms_assert(resDataItem);
		if (resDataItem->WasFailed(FR_Data)) resDataItem->ThrowFail();
		if (resDataItem->WasFailed()) context->Fail(resDataItem);
		resultStr = GetValue<SharedStr>(resDataItem, 0);

		UInt32 nrNewEvals = CountIndirections( resultStr.c_str() );
		if (nrNewEvals)
			resultStr.erase(0, nrNewEvals);
		nrEvals += nrNewEvals;
	}
	return resultStr;
}

ActorVisitState AbstrCalculator::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (Test(svf, SupplierVisitFlag::NamedSuppliers)) {
		if (IsSourceRef())
		{
			auto sourceItem = GetSourceItem();
			if (visitor.Visit(sourceItem) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
		else
		{
			CreateSupplierSet();
			for (SizeT i = 0; i < m_SupplierArray.size(); ++i)
			{
				dms_assert(!m_SupplierArray[i]->IsCacheItem());
				dms_assert(m_SupplierArray[i]);
				dms_assert(m_SupplierArray[i]->GetRefCount());
				if (visitor.Visit(m_SupplierArray[i]) == AVS_SuspendedOrFailed)
					return AVS_SuspendedOrFailed;
			}
		}
	}
	return AVS_Ready;
}

ActorVisitState AbstrCalculator::VisitImplSuppl(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* context, WeakStr expr, CalcRole cr)
{
	if (expr.empty())
		return AVS_Ready;

	CharPtr exprPtr = expr.c_str();
	UInt32 nrEvals = CountIndirections(exprPtr);
	if (!nrEvals)
		return AVS_Ready;

	CDebugContextHandle   dContext("AbstrCalculator", exprPtr, false);
	TreeItemContextHandle checkPtr(context, "Context");

	dms_assert(nrEvals);
	FencedInterestRetainContext irc;
	SuspendTrigger::FencedBlocker lockSuspend;

	SharedStr resultStr(exprPtr+nrEvals); // creates a new copy of exprPtr
	while (!resultStr.empty())
	{
		AbstrCalculatorRef calculator = ConstructFromDirectStr(const_cast<TreeItem*>(context), resultStr, cr);

		auto res = CalcResult(calculator, DataArray<SharedStr>::GetStaticClass());
		irc.Add(res.get_ptr());
		irc.Add(res->GetOld());

		const AbstrDataItem* resDataItem = AsDataItem(res->GetOld());
		dms_assert(resDataItem || res->WasFailed(FR_Data));

		if (calculator->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
		res->VisitSuppliers(svf, visitor);

		if (!--nrEvals)
			break;
		
		resultStr = GetValue<SharedStr>(resDataItem, 0);

		UInt32 nrNewEvals = CountIndirections( resultStr.c_str() );
		if (nrNewEvals)
			resultStr.erase(0, nrNewEvals);
		nrEvals += nrNewEvals;
	}
	return AVS_Ready;
}

const TreeItem* AbstrCalculator::GetSearchContext(const TreeItem* holder, CalcRole cr)
{
	dms_assert(holder);
	const TreeItem* searchContext = holder->GetTreeParent();
	if (searchContext && cr == CalcRole::ArgCalc)
		searchContext = searchContext->GetTreeParent();
	if (!searchContext)
		searchContext = SessionData::Curr()->GetConfigRoot();

	dms_assert(searchContext);
	return searchContext;
}

BestItemRef AbstrCalculator::FindErrorneousItem() const
{
	if (m_BestGuessErrorSuppl.first && m_BestGuessErrorSuppl.first->WasFailed())
		return m_BestGuessErrorSuppl;
	if (IsSourceRef())
	{
		TokenID supplRefID = GetLispExprOrg().GetSymbID();
		return FindBestItem(supplRefID);
	}
	for (auto ti : m_SupplierArray)
	{
		if (ti && WasInFailed(ti))
			return { ti, {} };
	}
	return { nullptr, {} };
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
	const TreeItem* supplier = FindItem(supplRefID);
	if (!supplier || supplier->IsCacheItem())
	{
		if (!m_BestGuessErrorSuppl.first)
		{
			auto x = FindBestItem(supplRefID);
			if (x.first && x.first->WasFailed())
				m_BestGuessErrorSuppl = x;

			auto msg = mySSPrintF("Unknown identifier '%s'", supplRefID.GetStr());
			m_Holder->Fail(msg, FR_MetaInfo);
		}
		return supplRef;
	}
	return slSupplierExprImpl(substBuff, supplier, mpf);
}

void registerSupplier(SubstitutionBuffer& substBuff, const TreeItem* supplier)
{
	// register an sequential ordinal for each first occurence of a supplier
	auto& countref = substBuff.m_SupplierSet[supplier];
	if (!countref)
		countref = substBuff.m_SupplierSet.size();
}

LispRef AbstrCalculator::slSupplierExprImpl(SubstitutionBuffer& substBuff, const TreeItem* supplier, metainfo_policy_flags mpf) const
{
	DBG_START("ExprCalculator", "slSupplierExprImpl", false);
	//	DBG_TRACE(("expr     %s", m_Expression.c_str()));
	//	DBG_TRACE(("supplRef %s", AsString(supplRef).c_str()) );

	dms_assert(supplier); // PRECONDITION
	if (m_Holder->DoesContain(supplier))
	{
		if (m_CalcRole == CalcRole::Calculator)
			m_Holder->ThrowFail("Calulation rule would create a circular dependency", FR_MetaInfo);
	}
	else
		supplier->UpdateMetaInfo();
	if (supplier->InTemplate() && !(mpf & metainfo_policy_flags::subst_never))
	{
		auto msg = mySSPrintF("Calulation rule would create a dependency on %s which is (part of) a template", supplier->GetFullName());
		m_Holder->ThrowFail(msg, FR_MetaInfo);
	}

	if (m_Holder != supplier)
		registerSupplier(substBuff, supplier);

	if (mpf & metainfo_policy_flags::subst_never || !supplier->IsPassor() && !supplier->HasCalculator() && !IsDataItem(supplier) && !IsUnit(supplier))
		return CreateLispTree(supplier, mpf & metainfo_policy_flags::suppl_tree);

	LispRef result = (m_CalcRole == CalcRole::Checker && m_Holder == supplier) ? supplier->GetKeyExprImpl() : supplier->GetCheckedKeyExpr();

#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace, "result=%s", AsString(result).c_str());
	dms_assert(IsExpr(result));
#endif
	if (result.EndP())
		supplier->throwItemError("SubstitutionError");

	dms_assert(!result.EndP());
	return result;
}

LispRef AbstrCalculator::SubstituteArgs(SubstitutionBuffer& substBuff, LispPtr localArgs, const AbstrOperGroup* og, arg_index argNr, SharedStr firstArgValue) const
{
	dms_assert(og);
	if (localArgs.EndP())
		return localArgs;
	dms_assert(localArgs.IsList());

	metainfo_policy_flags mpf = arg2metainfo_polcy(og->GetArgPolicy(argNr, firstArgValue.begin()));
	LispRef left = SubstituteExpr_impl(substBuff, localArgs.Left(),  mpf);
	if (argNr == 0 && og->HasDynamicArgPolicies())
	{
		auto dc = GetOrCreateDataController(left);
		FutureData fd = dc->CalcCertainResult();
		firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(fd->GetOld())))->GetIndexedValue(0);
	}

	LispRef right = SubstituteArgs(substBuff, localArgs.Right(), og, argNr + 1, firstArgValue);

	return (right == localArgs.Right() && left == localArgs.Left())
		? localArgs
		: LispRef(left, right);
}

void InstantiateTemplate(TreeItem* holder, const TreeItem* applyItem, LispPtr templCallArgList)
{
	// only config items can become template instantiations
	dms_assert(holder); 

	if (holder->WasFailed(FR_MetaInfo))
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
	ArgRefs argSeq; argSeq.reserve(LispListPtr(metaCallArgs).Length());
	SharedStr firstArgValue;
	for (LispListPtr cursor = metaCallArgs; !cursor.EndP(); cursor = cursor.Tail(), ++currArg)
	{
		auto oap = og->GetArgPolicy(currArg, firstArgValue.begin());
		bool mustCalcArg = FuncDC::MustCalcArg(oap, false);

		ArgRef argRef;
		if (!mustCalcArg) 
		{ // DomainContainer and ValuesContainer and SubsetContainer
			if (oap == oper_arg_policy::calc_as_result)
			{
				// skip condition argument for select_xxxx meta functions
				assert(currArg == 1); // only this one
				assert(cursor.Tail().EndP()); // no next args, argSeq must remain consistent with the first args..
				continue;
			}
			TokenID symbID = cursor.Left().GetSymbID();
			if (auto vc = ValueClass::FindByScriptName(symbID))
				argRef.emplace<SharedTreeItem>(UnitClass::Find(vc)->CreateDefault()); // unitName -> [UnitName []] ofwel unitName().
			else
			{
				auto foundItem = ac->FindItem(symbID);
				if (!foundItem)
				{
					auto msg = SharedStr(symbID.AsStrRange());
					holder->Fail(mySSPrintF("Cannot find %s", msg), FR_MetaInfo);
				}
				else
					argRef.emplace<SharedTreeItem>(foundItem);
			}
			dms_assert(argRef.index() == 1 && std::get<1>(argRef).has_ptr() || holder->WasFailed(FR_MetaInfo));
			dms_assert(!SuspendTrigger::DidSuspend()); // POSTCONDITION of argIter->m_DC->MakeResult();
		}
		else if (mustCalcArg)
		{
			FutureData dc = GetOrCreateDataController(std::get<1>(ac->SubstituteExpr(substBuff, cursor.Left()))); // what about non-substitited stuff?
			dms_assert(dc);
			FutureData fd = dc->CalcResultWithValuesUnits();
			dms_assert(!fd || fd->GetInterestCount());
			dms_assert(!SuspendTrigger::DidSuspend());
			dms_assert(!fd || CheckCalculatingOrReady(fd->GetOld()->GetCurrRangeItem()) || fd->WasFailed(FR_Data));
			dms_assert(fd || dc->WasFailed(FR_Data));
			if (dc->WasFailed(FR_Data))
				holder->Fail(dc.get_ptr(), FR_MetaInfo);
			argRef.emplace<FutureData>(std::move(fd));
			if (currArg == 0 && og->HasDynamicArgPolicies())
				firstArgValue = const_array_cast<SharedStr>(DataReadLock(AsDataItem(dc->GetOld())))->GetIndexedValue(0);
		}
		auto argItem = GetItem(argRef);
		if (argItem) {
			argItem->UpdateMetaInfo();
			dms_assert(argItem->m_State.GetProgress() >= PS_MetaInfo || argItem->WasFailed(FR_MetaInfo));
			if (argItem->WasFailed(mustCalcArg))
				holder->Fail(argItem, FR_MetaInfo);
		}

		dms_assert(argItem || holder->WasFailed(FR_MetaInfo)); // POSTCONDITION of argIter->m_DC->GetResult();

		if (holder->WasFailed(FR_MetaInfo))
			return {};
		dms_assert(argItem && !argItem->WasFailed(FR_MetaInfo));
		argSeq.push_back(argRef);
	}
	return argSeq;
}


bool ApplyMetaFunc_impl(TreeItem* holder, const AbstrCalculator* ac, const AbstrOperGroup* og, LispPtr metaCallArgs)
{
	dms_assert(ac);
	bool result;
	try {
		auto args = ApplyMetaFunc_GetArgs(holder, ac, og, metaCallArgs);
		dms_assert(!SuspendTrigger::DidSuspend());
		if (!args)
		{
			dms_assert(holder->WasFailed(FR_MetaInfo));
			return false;
		}

		auto oper = og->FindOperByArgs(*args);
		MG_CHECK(oper);
		dms_assert(!SuspendTrigger::DidSuspend());

		LifetimeProtector<TreeItemDualRef> resultHolder; resultHolder->SetTmp(holder);

		oper->CreateResultCaller(*resultHolder, *args, nullptr, metaCallArgs);

		bool resultingFlag = !resultHolder->WasFailed(FR_MetaInfo);

		if (!holder->GetDynamicObjClass()->IsDerivedFrom(oper->GetResultClass()))
		{
			auto msg = mySSPrintF("result of %s is of type %s, expected type: %s"
				, og->GetName()
				, resultHolder->GetCurrentObjClass()->GetName()
				, oper->GetResultClass()->GetName()
			);
			holder->Fail(msg, FR_MetaInfo);
		}
		if (resultHolder->WasFailed(FR_MetaInfo))
		{
			holder->Fail(*resultHolder, FR_MetaInfo);
			resultingFlag = false;
		}

		dms_assert(resultingFlag != (SuspendTrigger::DidSuspend() || resultHolder->WasFailed()));
		return resultingFlag;
	}
	catch (...)
	{
		holder->CatchFail(FR_MetaInfo);
		return false;
	}
	if (!result)
	{
		dms_assert(SuspendTrigger::DidSuspend() || holder->WasFailed(FR_MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
		return false;
	}

	dms_assert(!SuspendTrigger::DidSuspend() && !holder->WasFailed(FR_MetaInfo));  // if we asked for MetaInfo and only DataProcesing failed, we should at least get a result
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
	if (holder->WasFailed(FR_MetaInfo))
		return;
	if (holder->GetIsInstantiated())
		return;

	LispContextHandle operContext("ApplyMetaFunc_impl", LispRef(LispRef(og->GetNameID()), metaCallArgs));

#if defined(MG_DEBUG_DCDATA)
	DBG_START("ApplyMetaFunc_impl", "", false);
	DBG_TRACE(("metaCallExpr=%s", AsFLispSharedStr(metaCallArgs)));
#endif

//	if (holder->GetDynamicClass() != TreeItem::GetStaticClass())
//		holder->throwItemError("Only containers can be the sink for Meta Functions");

	StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
	InterestRetainContextBase base;
//	UpdateMarker::ChangeSourceLock changeStamp(holder, "ApplyMetaFunction");
//	Actor::UpdateLock lock(holder, actor_flag_set::AF_UpdatingMetaInfo);
	SuspendTrigger::FencedBlocker lockSuspend;

	bool resultFlag = ApplyMetaFunc_impl(holder, ac, og, metaCallArgs);

	dms_assert(resultFlag != holder->WasFailed(FR_MetaInfo));

	dms_assert(holder->GetIsInstantiated() || holder->WasFailed(FR_MetaInfo));
	holder->SetIsInstantiated(); // REMOVE if the above assert is PROVEN.
}

auto DeriveSubItem(const AbstrCalculator* ac, SubstitutionBuffer& substBuff, LispPtr subItemExprTail) -> MetaInfo
{
	dms_assert(ac);
	LispRef contextExpr = subItemExprTail.Left();
	SharedTreeItem container;
	if (contextExpr.IsList())
	{
		MG_CHECK(contextExpr.Left().IsSymb());
		if (contextExpr.Left().GetSymbID() == token::subitem)
		{
			auto result = DeriveSubItem(ac, substBuff, contextExpr.Right());
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
			return LispRef(LispRef(token::subitem), ac->SubstituteArgs(substBuff,  LispRef(contextExpr, subItemExprTail.Right()), AbstrOperGroup::FindName(token::subitem), 0, SharedStr()));
		}
		container = ac->FindItem(contextExpr.GetSymbID());
	}
	if (!container)
		throwErrorD("ExprParser", "left operand of SubExpr doesn't resolve to a configurartion item");
	dms_assert(!container->IsCacheItem());
	container->UpdateMetaInfo();
	registerSupplier(substBuff, container);
	auto subItemNameExpr = subItemExprTail.Right().Left();
	AbstrCalculatorRef calculator = AbstrCalculator::ConstructFromLispRef(ac->GetHolder(), subItemNameExpr, CalcRole::Other);
	auto res = CalcResult(calculator, DataArray<SharedStr>::GetStaticClass());
	auto subItemPath = GetCurrValue<SharedStr>(AsDataItem(res), 0);
	auto subItem = FindSubItem(container, subItemPath);
	dms_assert(subItem);
	dms_assert(!subItem->IsCacheItem());
	subItem->UpdateMetaInfo();
	return subItem;
}

LispRef AbstrCalculator::SubstituteExpr_impl(SubstitutionBuffer& substBuff, LispRef localExpr, metainfo_policy_flags mpf) const
{
	LispRef& bufferValue = substBuff.BufferedLispRef(mpf, localExpr);

	if (bufferValue == LispRef())
	{
		DBG_START("ExprCalculator", "SubstituteExpr_impl", false);
		DBG_TRACE(("localExpr = %s", AsString(localExpr).c_str()));

		if (localExpr.IsRealList()) // operator call or calculation scheme instantiation
		{
			dms_assert(localExpr.Left().IsSymb());

			LispRef head = localExpr.Left();
			if (head->GetSymbID() == token::scope)
			{
				auto leftExpr = localExpr.Right().Left();
				if (!leftExpr.IsSymb())
					throwErrorF("ExprParser", "Scope operator: Left operand should be a name, but '%s' given.", AsFLispSharedStr(leftExpr).c_str());
				SharedPtr<const TreeItem> scopeItem = FindItem(leftExpr.GetSymbID());
				if (!scopeItem)
					throwErrorF("ExprParser", "Scope operator: container '%s' not found", leftExpr.GetSymbID().GetStr().c_str());

				tmp_swapper<SharedPtr<const TreeItem>> swap(m_SearchContext, scopeItem);
				SubstitutionBuffer localBuffer;
				bufferValue = SubstituteExpr_impl(localBuffer, localExpr.Right().Right().Left(), mpf);
				goto exit;
			}
			if (head->GetSymbID() == token::arrow)
			{
				LispRef indexExpr = localExpr.Right().Left();
				if (!indexExpr.IsSymb())
					throwErrorD("ExprParser", "DataItem expected as left operand of arrow");

				SharedPtr<const TreeItem> indexItem = FindItem(indexExpr.GetSymbID());
				if (!IsDataItem(indexItem.get_ptr()))
					throwErrorD("ExprParser", "DataItem expected as left operand of arrow");

				indexExpr = slSupplierExprImpl(substBuff, indexItem, mpf); // now process left before re-assigning search context

				auto avu = AbstrValuesUnit( AsDataItem(indexItem.get_ptr()) );
				dms_assert(avu);

				tmp_swapper<SharedPtr<const TreeItem>> swap(m_SearchContext, avu);
				SubstitutionBuffer localBuffer;
				bufferValue =
					ExprList(token::lookup
						, indexExpr
						, SubstituteExpr_impl(localBuffer, localExpr.Right().Right().Left(), mpf)
					);
				goto exit;
			}
			if (head->GetSymbID() == token::subitem)
			{
				auto metaInfo = DeriveSubItem(this, substBuff, localExpr.Right());
				if (metaInfo.index() == 2)
					bufferValue = std::get<2>(metaInfo)->GetCheckedKeyExpr();
				else
					bufferValue = std::get<1>(metaInfo);
				goto exit;
			}
//			if (head->GetSymbID() == token::DomainUnit)
//			{
//				xxx;
//			}
//			dms_assert(head.GetSymbID() != token::sourceDescr); // TODO G8: Clean-up the mess
			if (head.GetSymbID() == token::sourceDescr)
			{
				LispPtr sourceRef = localExpr.Right().Left();
				dms_assert(sourceRef.IsSymb());
				bufferValue = slSupplierExpr(substBuff, sourceRef, mpf);
				goto exit;
			}

			// following code duplicates partly the code in AbstrCalculator::SubstituteExpr
			const AbstrOperGroup* og = AbstrOperGroup::FindName(head.GetSymbID()); 
			dms_assert(og);
			if (og->IsTemplateCall())
			{
				auto templateItem = FindItem(head.GetSymbID());
				if (!templateItem)
					throwErrorF("ExprParser", "'%s': unknown function"
						, head.GetSymbStr().c_str()
					);

				if (!templateItem->IsTemplate())
					throwErrorF("ExprParser", "'%s': found item '%s' is not defined as template"
						, head.GetSymbStr().c_str()
						, templateItem->GetFullName().c_str()
					);

				throwErrorF("ExprParser", "'%s': template instantiations (of '%s') not allowed as sub-expressions"
					, head.GetSymbStr().c_str()
					, templateItem->GetFullName().c_str()
				);
			}
			if (!og->MustCacheResult())
			{
				throwErrorF("ExprParser", "'%s': meta function call not allowed as sub-expressions"
					, head.GetSymbStr().c_str()
				);
			}

			// rewrite to at least make a context independent expr.
			DBG_TRACE(("beforeRewrite: %s", AsString(localExpr).c_str()));
			localExpr = RewriteExprTop(
				LispRef(
					localExpr.Left() // ref to built-in operator
					, SubstituteArgs(substBuff, localExpr.Right(), og, 0, {})
				)
			);

			if (og->CanResultToConfigItem())
			{
				DBG_TRACE(("afterRewrite: %s", AsString(localExpr).c_str()));

				DataControllerRef dc = GetOrCreateDataController(localExpr);
				auto supplier = dc->MakeResult();
				if (!supplier)
				{
					dms_assert(dc->WasFailed(FR_MetaInfo));
					m_Holder->ThrowFail(dc);
				}

				if (!supplier->IsCacheItem())
				{
					localExpr = slSupplierExprImpl(substBuff, supplier, mpf);
					DBG_TRACE(("result that can result to ConfigItem: %s", AsString(localExpr).c_str()));
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
	DBG_TRACE(("localExpr = %s", AsString(localExpr).c_str()));

	if (localExpr.IsRealList()) // operator call or calculation scheme instantiation
	{
		dms_assert(localExpr.Left().IsSymb());

		LispRef head = localExpr.Left();
		TokenID exprHeadID = head.GetSymbID();
		if (exprHeadID == token::arrow)
			goto skipTemplInst;

		const AbstrOperGroup* og = AbstrOperGroup::FindName(exprHeadID);
		dms_assert(og);
		if (og->IsTemplateCall())
		{
			auto templateItem = FindItem(head.GetSymbID());
			if (!templateItem)
				throwErrorF("ExprParser", "'%s': unknown operator  and no template or function was found with this name"
					, head.GetSymbStr().c_str()
				);

			if (!templateItem->IsTemplate())
				throwErrorF("ExprParser", "'%s': found item '%s' is not defined as template"
					, head.GetSymbStr().c_str()
					, templateItem->GetFullName().c_str()
				);

			// calculation scheme: isTempl, dont-subst templ
			registerSupplier(substBuff, templateItem);
			return MetaFuncCurry{ .fullLispExpr = localExpr, .applyItem = templateItem };
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
		MG_SIGNAL_ON_UPDATEMETAINFO

		auto lispRefOrg = GetLispExprOrg();
		if (IsSourceRef())
		{
			auto sourceItem = GetSourceItem();
			sourceItem->UpdateMetaInfo();
			m_LispExprSubst = sourceItem;
		}
		else
		{
			ErrMsgPtr p;
			SubstitutionBuffer substBuff;
			try {
				m_LispExprSubst = SubstituteExpr(substBuff, RewriteExpr(GetLispExprOrg()));
			}
			catch (const DmsException& x)
			{
				p = x.AsErrMsg();
			}
			// process registered suppliers
			TreeItemCRefArray supplierArrayCopy; supplierArrayCopy.swap(m_SupplierArray);
			UInt32 count = substBuff.m_SupplierSet.size();
			m_SupplierArray.resize(count);
			for (auto& tvPair : substBuff.m_SupplierSet)
			{
				dms_assert(tvPair.second > 0);
				dms_assert(tvPair.second <= count);
				m_SupplierArray[tvPair.second - 1] = tvPair.first;
			}
#if defined(DMS_COUNT_SUPPLIERS)
			if (GetInterestCount())
			{
				IncArrayInterestCount(m_SupplierArray);
				DecArrayInterestCount(supplierArrayCopy);
			}
#endif
			if (p)
				throw DmsException(p);
		}
		dms_assert(!m_HasSubstituted); // not allowed to call twice when this results in MetaInfo
		m_HasSubstituted = true;
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

extern "C"
TIC_CALL const AbstrCalculator* DMS_CONV DMS_TreeItem_CreateParseResult(TreeItem* context, CharPtr expr, bool contextIsTarget)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle tich(context, TreeItem::GetStaticClass(), "DMS_TreeItem_CreateParseResult");
		MG_PRECONDITION(expr);
		MG_PRECONDITION(*expr);

		SharedStr exprPtr(expr);
		dms_assert(!exprPtr.empty());

		AbstrCalculatorRef result = AbstrCalculator::ConstructFromStr(context, exprPtr, contextIsTarget ? CalcRole::Calculator : CalcRole::Other);
		result->IncRef(); // must be complemented by a call to DMS_ParseResult_Release

		return result;

	DMS_CALL_END
	return nullptr;
}

extern "C"
TIC_CALL const AbstrCalculator* DMS_CONV DMS_TreeItem_GetParseResult(const TreeItem* context)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle tich(context, TreeItem::GetStaticClass(), "DMS_TreeItem_GetParseResult");

		static SharedPtr<const AbstrCalculator> s_ResultHolder; // TODO G8: weg ermee
		if (context->HasCalculator())
		{
			s_ResultHolder = context->GetCalculator();
			return s_ResultHolder.get();
		}

	DMS_CALL_END
	return nullptr;
}

extern "C"
TIC_CALL void DMS_CONV DMS_ParseResult_Release(AbstrCalculator* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ParseResult", "Release", true);
		MG_PRECONDITION(self);
		self->Release();

	DMS_CALL_END
}

extern "C"
TIC_CALL bool         DMS_CONV DMS_ParseResult_CheckSyntax(AbstrCalculator* self)
{
	DMS_CALL_BEGIN

		ObjectContextHandle oh(self, "DMS_ParseResult_CheckSyntax");
		MG_PRECONDITION(self);
		return self->CheckSyntax();

	DMS_CALL_END
	return false;
}

extern "C"
TIC_CALL bool         DMS_CONV DMS_ParseResult_HasFailed              (AbstrCalculator* self)
{
	DMS_CALL_BEGIN

		MG_PRECONDITION(self);
		return self->GetHolder()->IsFailed();

	DMS_CALL_END
	return true;
}


extern "C"
TIC_CALL IStringHandle DMS_CONV DMS_ParseResult_GetFailReasonAsIString(AbstrCalculator* self)
{
	DMS_CALL_BEGIN

		MG_PRECONDITION(self);
	
		return IString::Create(self->GetHolder()->GetFailReason()->Why());

	DMS_CALL_END
	return IString::Create(GetLastErrorMsgStr());
}

extern "C"
TIC_CALL CharPtr  DMS_CONV DMS_ParseResult_GetAsSLispExpr(AbstrCalculator* self, bool afterRewrite)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ParseResult", "GetAsFLispExpr", true);
		MG_PRECONDITION(self);

		static SharedStr result;

		result = afterRewrite
			? self->GetAsFLispExpr()
			: self->GetAsFLispExprOrg();
		return result.c_str();

	DMS_CALL_END
	return "";
}

extern "C"
TIC_CALL const LispObj* DMS_CONV DMS_ParseResult_GetAsLispObj(AbstrCalculator* self, bool afterRewrite)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ParseResult", "GetAsSLispObj", true);
		MG_PRECONDITION(self);

		static LispRef resultHolder;
		
		if (afterRewrite)
			resultHolder = GetAsLispRef(self->GetMetaInfo());
		else
			resultHolder = self->GetLispExprOrg();

		return resultHolder.get();

	DMS_CALL_END
	return nullptr;
}

extern "C"
TIC_CALL const TreeItem* DMS_CONV DMS_ParseResult_CreateResultingTreeItem(AbstrCalculator* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ParseResult", "CreateResultingTreeItem", true);
		MG_PRECONDITION(self);

		static make_result_t resultHolder;
		resultHolder = MakeResult(self); // TODO G8: REMOVE dangling result
		return resultHolder->GetOld();

	DMS_CALL_END
	return nullptr;
}

extern "C"
TIC_CALL bool DMS_CONV DMS_ParseResult_CheckResultingTreeItem(AbstrCalculator* self, TreeItem* resultHolder)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ParseResult", "CheckResultingTreeItem", true);
		MG_PRECONDITION(self);

		auto result = CalcResult(self, resultHolder ? resultHolder->GetDynamicObjClass() : nullptr);
		if (resultHolder)
			resultHolder->Unify(result->GetOld(), "Formal ResultHolder", "Calculation Result"); // works like _CheckResulItem but throws and doesn't fail resultHolder.
		return true;

	DMS_CALL_END
	return false;
}

extern "C"
TIC_CALL const TreeItem* DMS_CONV DMS_ParseResult_CalculateResultingData(AbstrCalculator* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_ParseResult", "CalculateResultingDataEx", true);
		MG_PRECONDITION(self);

		static FutureData resultHolder; // TODO G8.5
		resultHolder = CalcResult(self, nullptr);
		resultHolder->GetOld();

	DMS_CALL_END
	return nullptr;
}

