// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// union_data / union_unit operators: concatenating domains and their
// attributes, with value-info (Explain) support for the merged items.

#include "Explain.h" // Explain::Context, AddQueueEntry
#include "vt/GeoSequence.h"
#include "vt/StringBounds.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "dbg/SeverityType.h"
#include "set/VectorFunc.h"
#include "utl/StrFormat.h"
#include "xct/DmsException.h"

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataController.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "Metric.h"
#include "ParallelTiles.h"
#include "TileChannel.h"
#include "TreeItemContextHandle.h"
#include "Unit.h"
#include "UnitClass.h"

#include "OperSignature.h"

#include "OperRelUni.h"
#include "Lookup.h"
#include "UnitProcessor.h"

CommonOperGroup cog_union      ("union",      oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit  ("union_unit", oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit08("union_unit_uint8", oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit16("union_unit_uint16", oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit32("union_unit_uint32", oper_policy::allow_extra_args);
CommonOperGroup cog_unionUnit64("union_unit_uint64", oper_policy::allow_extra_args);
Annotated<CommonOperGroup> cog_unionData (
	"Note that the first argument indicates the domain of the result and subsequent arguments (at least one) determine the unit and type of the resulting values."
,	token::union_data, oper_policy::allow_extra_args|oper_policy::can_explain_value
);

Annotated<CommonOperGroup> cog_orderedUnionData(
	"Note that the first argument indicates the domain of the result and subsequent arguments (at least one) determine the unit and type of the resulting values."
	" Furthermore, ordered_union_data guarantees that the concatenation of the values of the subsequent arguments is non-decreasing (monotone increasing, but not strictly);"
	" that guarantee is verified when the result is calculated and its violation is reported as a data error. Use union_data when the values are not known to be ordered."
	, token::ordered_union_data, oper_policy::allow_extra_args | oper_policy::can_explain_value
);

// *****************************************************************************
//                         UnionOperator
// *****************************************************************************
static StaticLateTokenID s_UnionData("UnionData");

class AbstrUnionOperator : public UnaryOperator // extra args are allowed
{
	ValueComposition m_VC;
public:
	AbstrUnionOperator(const Class* resCls, const DataItemClass* arg1Cls)
		: UnaryOperator(&cog_union, resCls, arg1Cls)
		,	m_VC(arg1Cls->GetValuesType()->GetValueComposition())
	{}

	// K6: union(a: attribute<V>(D0); b: …; …) -> a FRESH unit U [new] whose
	// UnionData sub-item borrows V. Only the existential result and its value
	// class (Unit<UInt32> == uint32, produced UNCONDITIONALLY at metainfo) are
	// stated. The arguments must share ONE value class END-TO-END (const_array_cast<V>
	// in UnionCopy at CALC time), but that is NOT claimed here: adversarial review
	// (2026-07-19) showed CreateResult's *metainfo* cross-arg check is skipped when
	// arg0 carries the DEFAULT values unit -- `if (arg1_ValuesUnit->IsDefaultUnit())
	// arg1_ValuesUnit = currArg_ValuesUnit;` adopts the next arg's unit of ANY class
	// without UnifyValues -- so a hard shared-values-var claim would reject a
	// definition whose metainfo succeeds (the batch-U default-unit S1 mirror).
	// The arguments therefore stay class-independent (deferred); only arg0's own
	// class is recorded, for the printer and member selection
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		auto argCls = dynamic_cast<const DataItemClass*>(GetArgClass(0));
		if (!argCls)
			return false;
		sig_var V = sb.UnitVar("V"), U = sb.GeneratedUnit("U");
		sb.MemberValueClass(V, argCls->GetValuesType()); // V is arg0-only: no cross-arg link
		if (auto resCls = dynamic_cast<const UnitClass*>(GetResultClass()))
			if (auto rvt = resCls->GetValueType())
				sb.MemberValueClass(U, rvt);
		sb.ArgName(0, "first");
		sb.ArgAttr(0, V, no_sig_var, m_VC);       // domain unconstrained; subsequent args deferred
		sb.ResultUnit(U);
		// §12.7 slSubItemCall tranche: the one UnionData sub-item. Its VALUES
		// unit escapes description (the default-unit adoption above) and its
		// composition is unified across the arguments, so the member claims
		// domain identity only
		sb.ResultContainerMember("UnionData", no_sig_var, U, ValueComposition::Unknown);
		sb.ResultMembersComplete();
		return true;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		SizeT n = args.size();                              assert(n>=1);

		auto arg1A = AsDataItem(args[0]);                   assert(arg1A);
		auto arg1_ValuesUnit = arg1A->GetAbstrValuesUnit(); assert(arg1_ValuesUnit);

		auto vc = m_VC;
		Unify(vc, arg1A->GetValueComposition());
		for (arg_index i=1; i!=n; ++i)
		{
			const AbstrDataItem* argA = AsCertainDataItem(args[i]);
			assert(argA);

			const AbstrUnit* currArg_ValuesUnit = argA->GetAbstrValuesUnit();
			Unify(vc, argA->GetValueComposition());

			assert(currArg_ValuesUnit);
			if (arg1_ValuesUnit->IsDefaultUnit())
				arg1_ValuesUnit = currArg_ValuesUnit;
			else
				currArg_ValuesUnit->UnifyValues(arg1_ValuesUnit, "Values of a next attribute wiht known values unit", "Values of the first attribute with known values unit", UnifyMode(UM_Throw | UM_AllowDefault));
		}

		auto resultDomain_owner = debug_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* resultDomain = resultDomain_owner.get();
		assert(resultDomain);
		resultHolder = resultDomain;

		AbstrDataItem* resSub = CreateDataItem(resultDomain, s_UnionData, resultDomain, arg1_ValuesUnit, vc ).get(); // owned by resultDomain
		MG_PRECONDITION(resSub);

		if (mustCalc)
		{
			SizeT count = 0;
			for (SizeT i=0; i!=n; ++i)
				count += debug_cast<const AbstrDataItem*>(args[i])->GetAbstrDomainUnit()->GetCount();

			resultDomain->SetCount(count);

			DataWriteLock resSubLock(resSub); 

			SizeT nrWritten = 0;

			for (arg_index i=0; i!=n; ++i)
			{
				const AbstrDataItem* argA = AsDataItem(args[i]);
				dms_assert(argA);
				DataReadLock argLock(argA);

				nrWritten = UnionCopy(resSubLock, argA, nrWritten);
			}
			dms_assert(nrWritten == count);

			resSubLock.Commit();
		}
		return true;
	}
	virtual SizeT UnionCopy(DataWriteLock& resSub, const AbstrDataItem* argA, SizeT nrCopied) const =0;
};

template <class V>
class UnionOperator : public AbstrUnionOperator // extra args are allowed
{
   typedef DataArray<V>     ArgType;
   typedef Unit<UInt32>     ResultType;
   typedef DataArray<V>     ResultSubType;

public:
	UnionOperator() : AbstrUnionOperator(ResultType::GetStaticClass(), ArgType::GetStaticClass()) 
	{}

	// Override Operator
	SizeT UnionCopy(DataWriteLock& resSub, const AbstrDataItem* argA, SizeT nrCopied) const  override
	{
		// don't use tileWriteChannel as it doesn't support parallel copying.
		ResultSubType* resultSub = mutable_array_cast<V>(resSub);
		auto resultSubData = resultSub->GetDataWrite(no_tile, dms_rw_mode::read_write); // TODO G8: copy tile by tile if possible; this will break non covering tilings and non-sequential tilings
		auto resultSubDataEnd = resultSubData.end();
		auto xx = resultSubData.begin() + nrCopied;
		dms_assert(!(resultSubDataEnd < xx));

		const ArgType* arg = const_array_cast<V>(argA);
		auto adu = argA->GetAbstrDomainUnit();
		tile_id tn = adu->GetNrTiles();

		std::atomic<SizeT> atomicNrCopied = nrCopied;
		parallel_tileloop_if_separable<V>(tn, [=, &atomicNrCopied](tile_id t)
			{
				auto x = xx + adu->GetTileFirstIndex(t);
				auto argData = arg->GetLockedDataRead(t);
				dms_assert(!(argData.size() && resultSubDataEnd < x + argData.size()));

				for (auto di = argData.begin(), de = argData.end(); di != de; ++x, ++di)
					Assign(*x, *di);

				atomicNrCopied += argData.size();
			}
		);
		return atomicNrCopied;
	}
};

// *****************************************************************************
//                         UnionUnitOperator
// *****************************************************************************
bool UnionUnit_impl(TreeItemDualRef& resultHolder, AbstrUnit* result, const ArgSeqType& args, bool mustCalc)
{
	dms_assert(args.size() >= 1);

	const AbstrUnit* arg1 = debug_cast<const AbstrUnit*>(args[0]);
	dms_assert(arg1);
	dms_assert(result);

	resultHolder = result;

	if (mustCalc)
	{
		row_id count = 0;
		for (arg_index i = 0, n = args.size(); i < n; ++i)
		{
			const AbstrUnit* adu = dynamic_cast<const AbstrUnit*>(args[i]);
			if (!adu)
				throwErrorF(cog_unionUnit.GetName().c_str(), "argument {} is not a Unit", i+1);
			for (tile_id t = 0, tn = adu->GetNrTiles(); t != tn; ++t)
			{
				auto tileCount = adu->GetTileCount(t);
				row_id newCount = count + tileCount;
				MG_CHECK2(newCount >= count,
					"Error in union_unit operator: the cumulation of the cardinalities of the arguments exceeds Max(SizeT)");
				count = newCount;
			}
		}
		result->SetCount(count);
	}
	return true;
}

template <typename ResultValueType>
class UnionUnitOperator : public UnaryOperator // extra args are allowed
{
   typedef AbstrUnit             ArgType;
   typedef Unit<ResultValueType> ResultType;

public:
   UnionUnitOperator(AbstrOperGroup& cog)
	   : UnaryOperator(&cog, ResultType::GetStaticClass(), ArgType::GetStaticClass())
	{}

	// K6: union_unit(a: unit; b: unit; ...) -> a FRESH unit U [new] of the
	// group's fixed class (uint32 / uint8 / ...). The arguments are units to
	// concatenate with no cross-constraint; only the existential result and its
	// value class are stated (printer + a typed unit result at definition)
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		sig_var S = sb.UnitVar("S"), U = sb.GeneratedUnit("U");
		if (auto resCls = dynamic_cast<const UnitClass*>(GetResultClass()))
			if (auto rvt = resCls->GetValueType())
				sb.MemberValueClass(U, rvt);
		sb.ArgName(0, "first");
		sb.ArgUnit(0, S);
		sb.ResultUnit(U);
		return true;
	}

   // Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		auto resultUnit_owner = ResultType::GetStaticClass()->CreateResultUnit(resultHolder.GetNew()); auto resultUnit = resultUnit_owner.get();
		resultUnit->SetTSF(TSF_Categorical);
		return UnionUnit_impl(resultHolder, resultUnit, args, mustCalc);
	}
};

// *****************************************************************************
//                         union_data part domain check (#990)
// *****************************************************************************

// union_data concatenates the values of its data arguments and presents them over the domain given as
// first argument. The only thing it verifies is that the cardinalities add up (ValidateCount in
// CalcResult below), which is deliberate: union_data is a concatenating relabel, and in most of its
// applications the result domain has no structural relation to the arguments at all. But it lets a
// mistake pass as soon as the sizes happen to line up:
// union_data(union_unit(NoordHolland, ZuidHolland), NH/CityNames, NH/CityNames) is accepted because
// 5+5 == 5+5 (#990).
//
// In general the intended decomposition of the result domain is unknown here, but not when that domain
// is itself produced by union_unit or combine(_unit): there the configurator wrote it down, and the
// parts are available as sub-expressions of the first argument's key expression. Two decompositions
// are recognised:
//
//	union_unit(u1 .. uk) with k data arguments: part i is ui.
//	combine(a, b) with two or more data arguments: every part is b, because combine enumerates its
//		factors with the FIRST varying slowest (the cycleSize loop in OperUnit.cpp), so the result
//		splits into #a consecutive blocks of b.
//
// The combine case does not need the cardinality of a, which is not guaranteed to be available in the
// meta phase: once every argument has domain b, the count check that runs at calculation time already
// implies that there are #a of them.
//
// A mismatch is reported as a warning and not as an error: the relabel contract still holds, so a
// configuration that means a per-part relabel keeps working. Everything this cannot establish -- an
// unknown domain, an unrecognised decomposition, a differing arity -- is accepted silently, and the
// caller wraps the check so that a diagnostic can never fail a result.

static StaticLateTokenID
	s_combine       ("combine"       ), s_combine_unit       ("combine_unit"       )
,	s_combine_uint8 ("combine_uint8" ), s_combine_unit_uint8 ("combine_unit_uint8" )
,	s_combine_uint16("combine_uint16"), s_combine_unit_uint16("combine_unit_uint16")
,	s_combine_uint32("combine_uint32"), s_combine_unit_uint32("combine_unit_uint32")
,	s_combine_uint64("combine_uint64"), s_combine_unit_uint64("combine_unit_uint64");

static bool IsUnionUnitGroupID(TokenID id)
{
	return id == cog_unionUnit  .GetNameID() || id == cog_unionUnit08.GetNameID()
		|| id == cog_unionUnit16.GetNameID() || id == cog_unionUnit32.GetNameID()
		|| id == cog_unionUnit64.GetNameID();
}

static bool IsCombineGroupID(TokenID id)
{
	return id == s_combine        || id == s_combine_uint8        || id == s_combine_uint16
		|| id == s_combine_uint32 || id == s_combine_uint64
		|| id == s_combine_unit   || id == s_combine_unit_uint8   || id == s_combine_unit_uint16
		|| id == s_combine_unit_uint32 || id == s_combine_unit_uint64;
}

// The parts are given as key (sub)expressions; interning one yields the unit item that the domain
// argument was built from. GetOrCreateDataController is meta-thread only, which is why the check runs
// only when a call expression was passed: that is the FuncDC path, whose FuncDC_CreateResult asserts
// IsMetaThread, and the ApplyMetaFunc path.
static auto UnionData_PartUnit(LispPtr partExpr) -> SharedTreeItem
{
	if (partExpr.EndP())
		return {};
	auto dc = GetOrCreateDataController(partExpr);
	if (!dc || dc->WasFailed(FailType::MetaInfo))
		return {};
	auto item = dc->MakeResult();
	if (!item || item->WasFailed(FailType::MetaInfo) || !AsDynamicUnit(item.get()))
		return {};
	return item;
}

static void UnionData_CheckPartDomains(const AbstrOperGroup* og, const TreeItemDualRef& resultHolder
	, const ArgRefs& args, LispPtr metaCallArgs)
{
	assert(og);
	if (!metaCallArgs.IsRealList())
		return; // no call expression available to inspect

	arg_index n = args.size() - 1;
	if (n < 2)
		return; // a single data argument relabels the whole domain; that is not a decomposition

	LispPtr domainExpr = metaCallArgs.Left();
	if (!domainExpr.IsRealList() || !domainExpr.Left().IsSymb())
		return; // the result domain is not the result of an operator application

	auto domainGroupID = domainExpr.Left().GetSymbID();
	bool isUnionUnit = IsUnionUnitGroupID(domainGroupID);
	if (!isUnionUnit && !IsCombineGroupID(domainGroupID))
		return; // no decomposition of the result domain is known here

	std::vector<LispPtr> factors;
	for (auto tail = domainExpr.Right(); tail.IsRealList(); tail = tail.Right())
		factors.push_back(tail.Left());

	// union_unit states one part per argument, so a differing arity means the arguments do not
	// correspond to the parts one by one and nothing can be concluded. Of the combine forms only the
	// two-factor one has an unambiguous decomposition into equal consecutive blocks.
	if (isUnionUnit ? (factors.size() != SizeT(n)) : (factors.size() != 2))
		return;

	SizeT nrMismatches = 0;
	arg_index firstMismatch = 0;
	SharedTreeItem firstPartItem;
	const AbstrDataItem* firstArg = nullptr;

	for (arg_index i = 0; i != n; ++i)
	{
		const AbstrDataItem* argA = AsDataItem(args[i + 1]);
		if (!argA)
			continue;
		auto argDomain = argA->GetAbstrDomainUnit();
		if (!argDomain)
			continue;

		auto partItem = UnionData_PartUnit(isUnionUnit ? factors[i] : factors[1]);
		if (!partItem)
			continue; // the part could not be established; say nothing about this argument
		auto part = AsUnit(partItem.get());

		// UM_AllowDefault: an argument whose domain is not (yet) known must not be judged.
		// UM_AllowVoidRight: a parameter fits any part; that is how a total row is appended to a table
		//	(union_unit(RowSet, void)) and how the AsItemList idiom fills a domain with parameters.
		// UM_AllowRightExpansion: makes the comparison total and symmetric instead of dependent on DC
		//	interning order; legal because this runs on the meta thread (see UnionData_PartUnit).
		if (part->UnifyDomain(argDomain, "", "", UnifyMode(UM_AllowDefault | UM_AllowVoidRight | UM_AllowRightExpansion)))
			continue;

		if (!nrMismatches)
		{
			firstMismatch = i;
			firstPartItem = partItem;
			firstArg      = argA;
		}
		++nrMismatches;
	}
	if (!nrMismatches)
		return;

	// Name every item by its SOURCE name: the units taking part here are cache results, whose
	// GetFullName is the empty name of a parentless cache root; GetSourceName walks the back
	// reference to the config item that refers to it and renders it as "[[/full/name]]".
	auto part          = AsUnit(firstPartItem.get());
	auto argDomainName = firstArg->GetAbstrDomainUnit()->GetSourceName();
	auto partName      = part->GetSourceName();
	auto extra = (nrMismatches > 1) ? mySSPrintF(" (+{} more)", nrMismatches - 1) : SharedStr();

	// Name the item this union_data is being calculated for, in FRONT of the message: the units that
	// disagree do not tell the configurator which calculation rule to go and fix, and the result
	// itself is an anonymous cache item. Putting the name first is also where reportD expects a
	// self-supplied item name and where it then suppresses its own appending of it (#795).
	auto itemName = resultHolder.GetItemNameStr();
	if (itemName.empty())
		itemName = GetReportingItemName();
	auto prefix = itemName.empty() ? SharedStr() : itemName + " ";

	// MsgDispatch truncates every message at 256 characters, and two source names of a deeply nested
	// configuration already take about half of that, on top of the item prefix. So state the finding
	// and nothing more: which argument, the domain it has, and the part it was matched against. The
	// argument itself is identified by its number rather than by a third source name, and the wiki
	// page carries the explanation and the advice.
	if (isUnionUnit)
		reportF(SeverityTypeID::ST_Warning
		,	"{0}{1}: argument {2} has domain {3}, but part {4} of the result domain is {5}.{6}"
		,	prefix.c_str(), og->GetNameStr()
		,	firstMismatch + 2, argDomainName.c_str()
		,	firstMismatch + 1, partName.c_str()
		,	extra.c_str()
		);
	else
		reportF(SeverityTypeID::ST_Warning
		,	"{0}{1}: argument {2} has domain {3}, but the result domain splits into blocks of {4};"
			" swap the factors of its {5}.{6}"
		,	prefix.c_str(), og->GetNameStr()
		,	firstMismatch + 2, argDomainName.c_str()
		,	partName.c_str()
		,	GetTokenStrLock(domainGroupID).c_str()
		,	extra.c_str()
		);
}

// *****************************************************************************
//                         UnionDataOperator
// *****************************************************************************

// REMOVE TODO AbstrOperator
template <typename V>
class UnionDataOperator : public BinaryOperator // extra args are allowed
{
	typedef AbstrUnit            Arg1Type;
	typedef DataArray<V>         ArgType;
	typedef DataArray<V>         ResultType;

public:
	bool m_IsOrdered;

	UnionDataOperator(bool isOrdered)
		:	BinaryOperator(isOrdered ? &cog_orderedUnionData : &cog_unionData, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), ArgType::GetStaticClass())
		,   m_IsOrdered(isOrdered)
	{}

	// A union_data CONCATENATES its data arguments, so its result volume is exactly the sum of
	// theirs -- which the base already accumulates as inputSize.
	//
	// Summing is only better than the generic estimate if the ARGUMENT sizes are themselves better.
	// They were not, at first: EstimateDataBytes fell back to ASSUMED_SEQ_LENGTH for every
	// sequence-valued argument, so the sum re-applied exactly the guess it was meant to replace --
	// the run-4 calibration logged `B=7.15G (1.00x) res=7.15G` for allLinks/geometry, i.e. Sum(args)
	// and the generic estimate agreeing to the digit, against a measured 1.67G. The fix is the
	// per-element width that points2sequence publishes and that consumers inherit; with it, the sum
	// carries a real width up the chain instead of re-guessing at every level (§8.1.17).
	//
	// Argument 0 is the result DOMAIN (a unit, not data), so it contributes nothing to inputSize.
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = BinaryOperator::EstimatePerformance(resultHolder, args);

		if (result.inputSize)
		{
			result.resultingMemory = result.inputSize;
			// Keep the regime-dependent charge consistent with the corrected volume.
			if (result.regime == materialization::eager || result.regime == materialization::deferred)
				result.residentMemory = result.resultingMemory;
			if (result.nrChores)
				result.choreMemory = result.resultingMemory / result.nrChores;

			// Pass the concatenated width on, so a union of unions does not fall back to the guess.
			// This is the average over the arguments; SetEstimatedBytesPerElement keeps the larger of
			// this and whatever the base already inherited, which is the conservative side.
			if (result.resultingNrElements)
				if (auto resultItem = resultHolder.GetUlt(); resultItem && IsDataItem(resultItem))
					AsDataItem(resultItem)->SetEstimatedBytesPerElement(
						result.resultingMemory / result.resultingNrElements);
		}
		return result;
	}

   // Override Operator
	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr metaCallArgs) const override
	{
		arg_index n = args.size() - 1;
		assert(n >= 1);

		const AbstrUnit* resultDomain = AsUnit(GetItem(args[0]));

		// #990: report a result domain that was built by union_unit or combine but whose parts do not
		// match the arguments. Diagnostic only, so never let it fail a result that the contract accepts.
		try { UnionData_CheckPartDomains(GetGroup(), resultHolder, args, metaCallArgs); }
		catch (...) {}

		ConstUnitRef constUnitRef;
		bool hadToTryWithoutCategoricalCheck = false;
		try {
			constUnitRef = compatible_values_unit_creator_func(1, &cog_unionData, GetItems(args), true);
		}
		catch (const DmsException& x)
		{
			constUnitRef = compatible_values_unit_creator_func(1, &cog_unionData, GetItems(args), false);
			reportF(SeverityTypeID::ST_Warning, "Depreciated usage of Union_data: {}"
				, x.AsErrMsg()->Why().c_str()
			);
			hadToTryWithoutCategoricalCheck = true;
		}

		auto vc = COMPOSITION(V);
		bool isCategorical = false;
		if (!hadToTryWithoutCategoricalCheck)
			for (arg_index i = 1; i <= n; ++i)
			{
				auto adi = AsDataItem(args[i]); assert(adi);
				if (adi->GetTSF(TSF_Categorical))
					isCategorical = true;
				Unify(vc, AsDataItem(args[i])->GetValueComposition());
			}

		if (resultHolder)
			return;

		resultHolder = CreateCacheDataItem(resultDomain, constUnitRef.get(), vc );
		if (isCategorical)
			resultHolder->SetTSF(TSF_Categorical);
		if (m_IsOrdered)
			resultHolder->m_StatusFlags.SetHasSortedValues();
	}

	// ordered_union_data promises that the concatenation of its data arguments is non-decreasing, and
	// CreateResultCaller stamps that promise onto the result as DSF_HasSortedValues. Consumers read that
	// flag and then take a sorted-merge path instead of building an index (rlookup, unique, GetCounts),
	// so a false promise doesn't surface as an error downstream but silently yields wrong results.
	// Verify it here, where the values pass by anyway on their way into the result.
	//
	// Non-decreasing means monotone increasing but NOT strict: equal successors are fine, only a descent
	// is an error. The check runs on freshly produced values only; values that are already there were
	// checked when they were produced.
	//
	// Only instantiated for the arithmetic value types: ordered_union_data is registered for
	// typelists::num_objects (see OrderedUnionOpers below), which is exactly the arithmetic subset of the
	// typelists::value_elements that UnionDataOperator as a whole is instantiated for. For the other
	// value types m_IsOrdered is always false and the check is not compiled at all.
	static constexpr bool can_check_ascending_v = std::is_arithmetic_v<V>;

	// running state of the ascending check, carried across the tiles of an argument and across the arguments
	struct ascending_check_state
	{
		SizeT m_NrChecked = 0;     // number of result elements checked so far; also the index of the next one
		SizeT m_ArgFirstIndex = 0; // result index of the first element of the argument currently being checked
		V     m_PrevValue = V();   // value of the last checked element; only meaningful when m_NrChecked != 0
	};

	[[noreturn]] void ThrowNotAscending(const ascending_check_state& state, const V& currValue
		, const AbstrDataItem* res, const AbstrDataItem* argA, arg_index argNr) const
	{
		res->ThrowFail(mySSPrintF(
			"ordered_union_data: the concatenation of the argument values is not non-decreasing.\n"
			"Element {0} of the result, which is element {1} of argument {2}: {3}, has value {4},"
			" which is less than the value {5} of its predecessor.\n"
			"Use union_data if the values of the arguments are not guaranteed to be ordered."
		,	state.m_NrChecked
		,	state.m_NrChecked - state.m_ArgFirstIndex
		,	argNr
		,	argA->GetSourceName()
		,	AsString(currValue)
		,	AsString(state.m_PrevValue)
		), FailType::Data);
	}

	template <typename ConstIter>
	void CheckAscending(ConstIter first, ConstIter last, ascending_check_state& state
		, const AbstrDataItem* res, const AbstrDataItem* argA, arg_index argNr) const
	{
		for (; first != last; ++first)
		{
			V currValue = *first;
			if (state.m_NrChecked && (currValue < state.m_PrevValue))
				ThrowNotAscending(state, currValue, res, argA, argNr);
			state.m_PrevValue = currValue;
			++state.m_NrChecked;
		}
	}

	// Check all tiles of one argument, in the order in which they contribute to the result.
	void CheckAscendingArg(ascending_check_state& state, const AbstrDataItem* res, const AbstrDataItem* argA, arg_index argNr) const
	{
		const ArgType* arg = debug_cast<const ArgType*>(argA->GetCurrRefObj().get());
		const AbstrUnit* argDU = argA->GetAbstrDomainUnit();
		state.m_ArgFirstIndex = state.m_NrChecked;
		for (tile_id t = 0, tn = argDU->GetNrTiles(); t != tn; ++t)
		{
			auto argData = arg->GetLockedDataRead(t);
			CheckAscending(argData.begin(), argData.end(), state, res, argA, argNr);
		}
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		dms_assert(resultHolder);

		const AbstrUnit* resultDomain = AsUnit(GetItem(args[0]));
		// aggregate resulting cardinality
		SizeT count = 0;
		arg_index n = args.size() - 1;
		for (arg_index i=1; i<=n; ++i)
		{
			const AbstrUnit* adu = AsDataItem(args[i])->GetAbstrDomainUnit();
			for (tile_id t = 0, tn= adu->GetNrTiles(); t!=tn; ++t)
				count += adu->GetTileCount(t);
		}

		resultDomain->ValidateCount(count);

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

		dms_assert(!context || (context->m_Domain && resultDomain->UnifyDomain(context->m_Domain, "r1", "e2")));
		dms_assert(!context || context->m_Coordinate);
		SizeT coordOffset = 0;
		if (context) if (auto coordPtr = context->m_Coordinate)
			coordOffset = coordPtr->first;

		dms_assert(!res->m_DataObject || context);
		bool dontRecalc = IsDataReady(res);
		dms_assert(!dontRecalc || context);

		// both are only read from within the if constexpr (can_check_ascending_v) blocks below
		[[maybe_unused]] bool mustCheckAscending = m_IsOrdered && !dontRecalc;
		[[maybe_unused]] ascending_check_state ascendingState;

		if (n == 1)
		{
			const AbstrDataItem* argA = AsDataItem(args[1]);

			DataReadLock argLock(argA);
			if (argLock->GetTiledRangeData() == resultDomain->GetTiledRangeData())
			{
				// the single argument becomes the result as-is, so the loop below that checks while writing
				// is not entered; verify the promise on the adopted values here.
				if constexpr (can_check_ascending_v)
				{
					if (mustCheckAscending)
						CheckAscendingArg(ascendingState, res, argA, 2);
				}
				res->m_DataObject = argLock;
				return true;
			}
		}

		locked_tile_write_channel<V> resultDataChannel(dontRecalc ? nullptr : res);
		for (arg_index i = 1; i <= n; ++i)
		{
			const AbstrDataItem* argA = AsDataItem(args[i]);
			dms_assert(argA);
			const AbstrUnit* argDU = argA->GetAbstrDomainUnit();

			DataReadLock argLock(argA);
			const ArgType* arg = debug_cast<const ArgType*>(argA->GetCurrRefObj().get());
			if constexpr (can_check_ascending_v)
				ascendingState.m_ArgFirstIndex = ascendingState.m_NrChecked;
			for (tile_id t = 0, tn = argDU->GetNrTiles(); t != tn; ++t)
			{
				auto argData = arg->GetLockedDataRead(t);

				if (!dontRecalc)
				{
					if constexpr (can_check_ascending_v)
					{
						if (mustCheckAscending)
							CheckAscending(argData.begin(), argData.end(), ascendingState, res, argA, i + 1);
					}
					resultDataChannel.Write(argData.begin(), argData.end());
				}
				if (context)
				{
					SizeT sz = argData.size();
					if (coordOffset < sz)
					{
						Explain::AddQueueEntry(context->m_CalcExpl, argDU, argDU->GetTileIndex(t, coordOffset));
						context = nullptr;
					}
					else
						coordOffset -= sz;
				}
			}
		}
		if (!dontRecalc)
		{
			dms_assert(resultDataChannel.IsEndOfChannel());
			resultDataChannel.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{

	template <typename X>
	struct UnionOpers
	{
		UnionOperator<X>     unionOrg;
		UnionDataOperator<X> unionData = UnionDataOperator<X>(false);
	};

	template <typename X>
	struct OrderedUnionOpers
	{
		UnionDataOperator<X> orderedUnionData = UnionDataOperator<X>(true);
	};

	tl_oper::inst_tuple_templ<typelists::value_elements, UnionOpers > instUnionOpers;
	tl_oper::inst_tuple_templ<typelists::num_objects, OrderedUnionOpers > instOrderedUnionOpers;
	UnionUnitOperator<UInt32> unionUnit(cog_unionUnit);
	UnionUnitOperator<UInt8 > unionUnit08(cog_unionUnit08);
	UnionUnitOperator<UInt16> unionUnit16(cog_unionUnit16);
	UnionUnitOperator<UInt32> unionUnit32(cog_unionUnit32);
	UnionUnitOperator<UInt64> unionUnit64(cog_unionUnit64);

} // end anonymous namespace



