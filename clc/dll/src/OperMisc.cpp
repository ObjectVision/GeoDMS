// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// Small clc operator TUs, merged (2026-08): AnyAll, Sort, Checker, SubItem,
// ExprCalculator, Loop, Ramp.

// ==== from AnyAll.cpp ====

#include "OperAccUniNum.h"

// INSTANTIATION

namespace 
{
	CommonOperGroup cogAny("any");
	CommonOperGroup cogAll("all");

	OperAccTotUniNum<any_total> anyTotal(&cogAny, true);
	OperAccTotUniNum<all_total> allTotal(&cogAll, true);

	OperAccPartUniDirect<any_partial > anyPart(&cogAny, true);
	OperAccPartUniDirect<all_partial > allPart(&cogAll, true);
}


// ==== from Sort.cpp ====

#include "mci/CompositeCast.h"
#include "set/DataCompare.h"
#include "utl/TypeListOper.h"
#include "RtcTypeLists.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

// *****************************************************************************
//                         Sort
// *****************************************************************************

// REMOVE, TODO: AbstrSortOperator

template <class V>
class SortOperator : public UnaryOperator
{
	typedef DataArray<V> ArgumentType;
	typedef DataArray<V> ResultType;

public:
	// Override Operator
	SortOperator(AbstrOperGroup* gr)
		:	UnaryOperator(gr, ResultType::GetStaticClass(), ArgumentType::GetStaticClass()) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* adi = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(adi);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(adi->GetAbstrDomainUnit(), adi->GetAbstrValuesUnit(), COMPOSITION(V));
			resultHolder->m_StatusFlags.SetHasSortedValues();
		}

		if (mustCalc)
		{
			const ArgumentType *  di = const_array_cast<V>(adi);
			dms_assert(di);


			DataReadLock arg1Lock(adi);
			auto unsortedData = di->GetDataRead();

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResultType* result = mutable_array_cast<V>(resLock);
			auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
			dms_assert(resData.size() == unsortedData.size());
			fast_copy(unsortedData.begin(), unsortedData.end(), resData.begin());

			std::sort(resData.begin(), resData.end(), DataLessThanCompare<V>());

			resLock.Commit();
		}
		return true;
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{

	CommonOperGroup cog_sort("sort");

	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, SortOperator> sortOperators(&cog_sort);
} // end anonymous namespace




// ==== from Checker.cpp ====

// *****************************************************************************
//										CheckOperator
// *****************************************************************************

#include "dbg/SeverityType.h"
#include "utl/StrFormat.h"
#include "LispTreeType.h"

#include "MoreDataControllers.h"
#include "OperGroups.h"
#include "TicPropDefConst.h"
#include "TreeItemClass.h"

CommonOperGroup sog_Check(token::integrity_check, oper_policy::existing|oper_policy::dynamic_result_class);

#if defined(MG_DEBUG)
// #1182 observability: counts distinct integrity_check DC instantiations; the trace line makes
// per-condition totals grep-able from a /L log to compare guard-dedup effectiveness.
static std::atomic<UInt32> gd_NrIntegrityCheckDCs = 0;
#endif

struct CheckOperator : public BinaryOperator
{
	using Arg2Type = DataArray<Bool>;

	CheckOperator()
		: BinaryOperator(&sog_Check,
			TreeItem::GetStaticClass(),
			TreeItem::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		const TreeItem* arg1 = args[0];
		assert(arg1);
//		dms_assert(arg1->IsCacheItem());
		if (!resultHolder) {
			assert(!mustCalc);
			resultHolder = arg1;
			resultHolder->m_State.Set(actor_flag_set::AF_IntegrityChecked);
			resultHolder.m_State.Set(actor_flag_set::AF_IntegrityChecked);
#if defined(MG_DEBUG)
			if (auto funcDC = dynamic_cast<FuncDC*>(&resultHolder))
				reportF(SeverityTypeID::ST_MinorTrace, "integrity_check dc #{}: {}"
				,	++gd_NrIntegrityCheckDCs
				,	AsFLispSharedStr(funcDC->GetArgDC(1)->GetLispRef(), FormattingFlags::None));
#endif
		}
		assert(resultHolder);

		if (mustCalc)
		{
			auto curr = resultHolder.GetCurr(); // owning snapshot; weak arm (config item) can expire
			MG_CHECK(curr);
			auto arg2A = AsDataItem(args[1]);
			assert(CheckDataReady(arg2A));
			assert(CheckDataReady(curr.get()));
			DataReadLock arg2Lock(arg2A);

			IntegrityCheckFailure(curr.get(), arg2A, [&resultHolder]() -> SharedStr
				{
					auto funcDC = dynamic_cast<FuncDC*>(&resultHolder);
					MG_CHECK(funcDC);
					auto condDC = funcDC->GetArgDC(1);
					assert(condDC);

					return AsFLispSharedStr(condDC->GetLispRef(), FormattingFlags::None);
				}
			);
			if (curr->WasFailed())
				resultHolder.Fail(curr.get());
			resultHolder.SetProgress(ProgressState::Validated);
		}
		return true;
	}
};


namespace {

	CheckOperator checkOperator;

}

// ==== from SubItem.cpp ====

#include "dbg/SeverityType.h"
#include "utl/StrFormat.h"
#include "utl/Quotes.h"
#include "LispRef.h"

#include "CheckedDomain.h"
#include "LispTreeType.h"
#include "MoreDataControllers.h"
#include "OperGroups.h"
#include "ParallelTiles.h"
#include "TreeItemClass.h"
#include "DataArrayValue.h"

// *****************************************************************************
//										SubItemOperator
// *****************************************************************************

// normal phases in processing an operator
// -M find the operator group, determined by the operator name
// -M SRC->DST: determine signature, result meta-info, or result data for each of the arguments, determined by their corresponting OperArgPolicy
// -M SRC->DST: Signature and other result meta-info generation and determine suppliers: CreateResultCaller(...) -> CreateResult(..., ..., false);
// - 
// -M DST->SRC: Set target interests -> set iterest on m_ResultHolder->m_Data and SetSupplierIterest
// -M SRC->DST: Scheduling: Create OperationContexts -> Activate free OC's -> register running OC's -> Terminating OC's free waiters.
//	- W SRC->DST: Calculation: CalcResult  -> CreateResult(..., ..., true), called from running OC's

// SubItem determines the 2nd arg during interest-setting, during scheduling in order to determine
// which sub-item of a container should be calculated


// PhaseContainer Creates a full mirror-tree as result meta-info, with Phase Numbers, but no supppliers
// at Calculation, it sets interest on source items that are mirrored by interesting sub-items, 
// schedules (which creates full mirror-trees at upstream Fences), and runs them, which starts by Calculation up stream Fences, all the way up and down; depth first dependency traversal.
//
// Intended purposes
// - Calc a sequential process in phases, to force summarization of intermediate results and thus not keeping large intermediate results for later summmarization
// - partition and serialize parallel work to avoid too much simultaneous intermediate data 
// 
// issues
// + Phase suppliers are always seen and numbered during meta-info info generation as this includes a mirror-tree
// - still red items in HESTIA:/TussenResultaten/StartJaar/StateNaAllocatie_Fenced
// - setting additional targets during or after calculations, see issue #902

oper_arg_policy oap_SubItem[2] = { oper_arg_policy::calc_subitem_root, oper_arg_policy::calc_always };

SpecialOperGroup sog_SubItem(token::subitem, 2, oap_SubItem, oper_policy::existing|oper_policy::dynamic_result_class);

struct SubItemOperator: BinaryOperator
{
	using Arg2Type = DataArray<SharedStr>;

	SubItemOperator()
		: BinaryOperator(&sog_SubItem, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		const TreeItem* arg1 = args[0];
		assert(arg1);
		assert(arg1->IsCacheItem());
		if (!resultHolder) {
			dms_assert(!mustCalc);
			checked_domain<Void>(args[1], "a2");

			SharedStr subItemName = GetCurrValue<SharedStr>(args[1], 0);
			const TreeItem* subItem = arg1->GetCurrItem(subItemName).get();
			if (subItem && subItem->IsCacheItem())
				subItem = subItem->GetCurrUltimateItem().get(); // "/nr_OrgEntity" -> "/org_rel"

			if (!subItem)
				GetGroup()->throwOperErrorF("Cannot find '{}' from '{}'",
					subItemName.c_str(),
					arg1->GetFullName().c_str()
				);
			assert(subItem->IsCacheItem());
			resultHolder = subItem;
			MG_CHECK(subItem->GetInterestCount() || !resultHolder.GetInterestCount());
		}
		assert(resultHolder);
		MG_CHECK(!mustCalc || !IsDataItem(resultHolder.GetUlt()) || AsDataItem(resultHolder.GetUlt())->m_DataObject
			|| resultHolder->WasFailed(FailType::Data)
		);
		return true;
	}
};

namespace {

	SubItemOperator subItemOperator;

}

// ==== from ExprCalculator.cpp ====

#include "ExprCalculator.h"

//tic
#include "LispContextHandle.h"

// stx
#include "ParseExpr.h"

//----------------------------------------------------------------------
// ExprCalculator constructor / destructor
//----------------------------------------------------------------------

ExprCalculator::ExprCalculator(const TreeItem* context, WeakStr expr, CalcRole cr)
	:	AbstrCalculator(context, cr)
	,	m_Expression(expr)
{
	dms_assert(context);
}

LispRef ExprCalculator::GetLispExprOrg() const
{
	if (!m_HasParsed)
	{
		m_LispExprOrg = LispRef();
		LispContextHandle lch(m_Expression.c_str(), LispRef());
		m_LispExprOrg = ParseExpr(m_Expression);
		m_HasParsed = true;
	}
	return m_LispExprOrg;
}

bool ExprCalculator::CheckSyntax() const
{
	DBG_START("ExprCalculator", "CheckSyntax", false);

	return ! GetLispExprOrg().EndP();
}

#if defined(DMS_COUNT_SUPPLIERS)
void IncArrayInterestCount(const TreeItemCRefArray& supplierArray)
{
	std::for_each(
		supplierArray.begin(),
		supplierArray.end(),
		std::mem_fun(&TreeItem::IncInterestCount)
	);
}

void DecArrayInterestCount(const TreeItemCRefArray& supplierArray)
{
	std::for_each(
		supplierArray.begin(),
		supplierArray.end(),
		std::mem_fun(&TreeItem::DecInterestCount)
	);
}
#endif //defined(DMS_COUNT_SUPPLIERS)

void ExprCalculator::WriteHtmlExpr(OutStreamBase& outStream) const 
{
	annotateExpr(outStream, SearchContext().get(), m_Expression);
}

//----------------------------------------------------------------------
// Test
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

#include "StxInterface.h"
#include "ptr/AutoDeletePtr.h"

CLC_CALL bool ExprCalculatorTest()
{
	DBG_START("ExprCalculator", "TEST", true);

	AutoDeletePtr<TreeItem> testConfig = DMS_CreateTreeFromString(
		"container Test { "
		"	parameter<UInt32> Item1: Expr = \"3+5\"; "
		"	parameter<UInt32> Item2: Expr = \"Item1\"; "
		"}"
	);
	const TreeItem* item1 = testConfig->FindItem("Item1").get();
	const TreeItem* item2 = testConfig->FindItem("Item2").get();
	bool result = true;
	result &= DBG_TEST("SourceItem", item2->GetSourceItem() == item1);
	result &= DBG_TEST("RefObj",     AsDataItem(item2)->LockAndGetValue<UInt32>(0) == 8);
	return result;
}

#endif //defined(MG_DEBUG)


// ==== from Loop.cpp ====

#include "utl/StrFormat.h"

#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "MoreDataControllers.h"
#include "TreeItemClass.h"
#include "DataArrayValue.h"

// *****************************************************************************
//									Loop operator
// *****************************************************************************

class LoopOperator : public BinaryOperator
{
	using loop_count_t = UInt16;
	typedef TreeItem          Arg1Type; // container met te repeteren groep
	typedef DataArray<loop_count_t> Arg2Type; // max nr iterations; make it a unit

public:
	LoopOperator(AbstrOperGroup* gr)
		:	BinaryOperator(gr, TreeItem::GetStaticClass(), 
				Arg1Type::GetStaticClass(), 
				Arg2Type::GetStaticClass()
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const Arg1Type* loopContents   = args[0];
		dms_assert(loopContents);

		const AbstrDataItem* maxNrIterParamA= AsDataItem(args[1]);
		DataReadLock lock(maxNrIterParamA);

		loop_count_t maxNrIter = const_array_cast<loop_count_t>(maxNrIterParamA)->GetIndexedValue(0);
		if (!IsDefined(maxNrIter))
			throwErrorD("Loop", "Nr iterations is undefined");

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		TreeItem* result = resultHolder.GetNew();
		dms_assert(result);

		SharedStr lastIterName = SharedStr(loopContents->GetID());

		bool checkStopValue
			=	mustCalc 
			&&	(	loopContents->FindItem("stopValue")
				->	CheckObjCls(DataArray<Bool>::GetStaticClass())
				);

		for (loop_count_t i=0; i!= maxNrIter; ++i)
		{
			TreeItem* iter = result->CreateItem(GetTokenID_mt(mySSPrintF("iter{}", i).c_str())).get();
			dms_assert(iter);

			SharedStr expr = SharedStr( loopContents->GetID() );
			expr += "(";
			expr += mySSPrintF("UInt16({})", i);
			if (i>0)
			{
				expr += ",";
				expr += mySSPrintF("iter{}/nextValue", i-1);
			}
			expr += ")";

			iter->SetExpr(SharedStr(expr));

			if (checkStopValue && false) // TODO, NYI
			{
				const AbstrDataItem* stopParamA
					=	debug_cast<const AbstrDataItem*>(
							iter->FindItem("stopValue")->CheckObjCls(DataArray<Bool>::GetStaticClass())
						);

				if (stopParamA && GetValue<Bool>(stopParamA, 0))
					break;
			}
			lastIterName = SharedStr(iter->GetID());
		}
		TreeItem* lastIter = result->CreateItem(GetTokenID_mt("lastIter")).get();
		lastIter->SetExpr(SharedStr(lastIterName));
		result->SetIsInstantiated();

		return true;
	}
};

// *****************************************************************************
//									LoopNTV operator
// *****************************************************************************

static TokenID lastValueToken = GetTokenID_st("lastValue");


class LoopNTVOperator : public TernaryOperator
{
	typedef DataArray<SharedStr> Arg1Type; // namen van de loop-elementen
	typedef TreeItem             Arg2Type; // container met te repeteren groep
	typedef DataArray<SharedStr> Arg3Type;    // eerste waarde voor eerste parameter

public:
	LoopNTVOperator(AbstrOperGroup* gr)
		:	TernaryOperator(gr, TreeItem::GetStaticClass(), 
				Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			,	Arg3Type::GetStaticClass()
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* iterNames     = debug_cast<const AbstrDataItem*>(args[0]);
		const Arg2Type*      loopContents  = debug_cast<const Arg2Type     *>(args[1]);
		const AbstrDataItem* currValueExpr = debug_cast<const AbstrDataItem*>(args[2]);

		dms_assert(iterNames);
		dms_assert(loopContents);
		dms_assert(currValueExpr);

		SharedStr currValueExprStr = currValueExpr->LockAndGetValue<SharedStr>(0);

		DataReadLock lock(iterNames);

		row_id nrIter = iterNames->GetAbstrDomainUnit()->GetCount();

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		TreeItem* result = resultHolder.GetNew();
		dms_assert(result);

		for (row_id i=0; i!= nrIter; ++i)
		{
			SharedStr iterName = iterNames->GetValue<SharedStr>(i);
			CopyTreeContext ctc(
				resultHolder.GetNew(),
				loopContents,
				iterName.c_str(), 
				DataCopyMode::CopyExpr
			);
			auto iterItem = ctc.Apply();
			if (!iterItem)
				throwErrorF("Iterate", "Failed to instantiate {}", loopContents->GetSourceName().c_str());
			TreeItem* currValue = iterItem->_GetFirstSubItem();
			if (currValue)
				currValue->SetExpr(currValueExprStr);
			currValueExprStr = iterName + "/nextValue";
		}
		auto lastIter = result->CreateItem(lastValueToken);
		lastIter->SetExpr(currValueExprStr);
		result->SetIsInstantiated();

		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	oper_arg_policy oap_Loop[2] = { oper_arg_policy::is_templ, oper_arg_policy::calc_always };
	oper_arg_policy oap_LoopNTV[3] = { oper_arg_policy::calc_always, oper_arg_policy::is_templ, oper_arg_policy::calc_never };

	SpecialOperGroup sopLoop("loop", 2, oap_Loop, oper_policy::dont_cache_result|oper_policy::calc_requires_metainfo);
	LoopOperator lo(&sopLoop);

	SpecialOperGroup sopLoopNTV("iterate", 3, oap_LoopNTV, oper_policy::dont_cache_result);
	LoopNTVOperator loNTV(&sopLoopNTV);

}

/******************************************************************************/



// ==== from Ramp.cpp ====

#include "mci/CompositeCast.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "ParallelTiles.h"

// *****************************************************************************
//                         Ramp
// *****************************************************************************

struct AbstrRampOperator : TernaryOperator
{
	// Override Operator
	AbstrRampOperator(AbstrOperGroup* og, const Class* argClass)
		:	TernaryOperator(og, argClass, argClass, argClass, AbstrUnit::GetStaticClass()) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrUnit* e = AsUnit(args[2]);
		dms_assert(e);

		const AbstrDataItem* adi1 = AsDataItem(args[0]); dms_assert(adi1);
		const AbstrDataItem* adi2 = AsDataItem(args[1]); dms_assert(adi2);

		const AbstrUnit* v1 = adi1->GetAbstrValuesUnit();
		dms_assert(v1);
		v1->UnifyValues(adi2->GetAbstrValuesUnit());

		MG_CHECK(const_unit_dynacast<Void>(adi1->GetAbstrDomainUnit()));
		MG_CHECK(const_unit_dynacast<Void>(adi2->GetAbstrDomainUnit()));

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, v1);
		
		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock, e, adi1, adi2);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, const AbstrUnit* e, const AbstrDataItem* adi1, const AbstrDataItem* adi2) const =0;
};

template <typename RampFunc, typename ValuesType>
struct RampOperator : AbstrRampOperator
{
	typedef DataArray<ValuesType> ArgType;
	typedef ArgType               ResultType;

	// Override Operator
	RampOperator(AbstrOperGroup* og, bool isClosed)
		:	AbstrRampOperator(og, ArgType::GetStaticClass())
		,	m_IsClosed(isClosed)
	{}

	void Calculate(DataWriteLock& res, const AbstrUnit* e, const AbstrDataItem* adi1, const AbstrDataItem* adi2) const override
	{
		SizeT n = e->GetCount();
		MG_CHECK(n > 1);

		ValuesType firstV = GetTheCurrValue<ValuesType>(adi1);
		ValuesType lastV = GetTheCurrValue<ValuesType>(adi2);

		auto resData = mutable_array_cast<ValuesType>(res)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		auto resultIter = resData.begin();

		assert(SizeT(resData.end() -resultIter) == n);

		RampFunc rf(m_IsClosed ? n-1 : n, firstV, lastV);
		for (UInt32 i=0; i != n; ++i, ++resultIter)
			*resultIter = rf(i);
	}
	bool m_IsClosed;
};

struct RampLinearFunc
{
	RampLinearFunc(SizeT n, Float64 firstV, Float64 lastV)
		:	m_N(n)
		,	m_FirstV(firstV)
		,	m_LastV(lastV)
	{
		dms_assert(n>1);

		m_FirstV /= m_N;
		m_LastV  /= m_N;
	}
	Float64 operator() (SizeT i) const
	{
		dms_assert( i <= m_N );
		return m_LastV * i + m_FirstV * (m_N-i);
	}
	SizeT   m_N;
	Float64 m_FirstV, m_LastV;
};

#include "vt/color.h"
#include "DataArrayValue.h"

struct RampRgbFunc
{
	RampRgbFunc(SizeT n, DmsColor firstV, DmsColor lastV)
		:	m_N(n)
		,	m_FirstV(firstV)
		,	m_LastV(lastV)
	{

	}

	DmsColor operator() (SizeT i) const
	{
		dms_assert( i <= m_N);
		return
			CombineRGB(
				(GetRed  (m_FirstV)*(m_N-i) + GetRed  (m_LastV)*i) / m_N
			,	(GetGreen(m_FirstV)*(m_N-i) + GetGreen(m_LastV)*i) / m_N
			,	(GetBlue (m_FirstV)*(m_N-i) + GetBlue (m_LastV)*i) / m_N
			,	(GetTrans(m_FirstV)*(m_N-i) + GetTrans(m_LastV)*i) / m_N
			);
	}
	SizeT   m_N;
	DmsColor m_FirstV, m_LastV;
};

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	CommonOperGroup cog_ramp   ("ramp");
	CommonOperGroup cog_rampRgb("ramp_rgb");

	CommonOperGroup cog_rampOpen   ("ramp_open");
	CommonOperGroup cog_rampOpenRgb("ramp_open_rgb");

	template <typename Numeric>
	struct RampLinearOperator 
	{
		RampLinearOperator()
			: m_Ramp(&cog_ramp, true)
			, m_RampOpen(&cog_rampOpen, false)
		{}
		RampOperator<RampLinearFunc, Numeric> m_Ramp, m_RampOpen;
	};

	tl_oper::inst_tuple_templ< typelists::numerics, RampLinearOperator >
		operRampLinearInstances;

	RampOperator<RampRgbFunc, DmsColor> operRampRgb(&cog_rampRgb, true);
	RampOperator<RampRgbFunc, DmsColor> operRampOpenRgb(&cog_rampOpenRgb, false);
} // end anonymous namespace

