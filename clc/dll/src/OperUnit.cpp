// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperUnit.h"
#include "OperConv.h"

//#include "RtcGeneratedVersion.h"
#include "vt/GeoSequence.h"
#include "mci/CompositeCast.h"
#include "mci/ValueWrap.h"
#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "ser/StringStream.h"
#include "utl/StrFormat.h"

#include "Unit.h"
#include "UnitClass.h"
#include "AbstrDataItem.h"
#include "DataArray.h"
#include "Metric.h"
#include "Projection.h"
#include "DataItemClass.h"
#include "Param.h"
#include "LispTreeType.h"
#include "TileFunctorImpl.h"
#include "TreeItemClass.h"
#include "UnitProcessor.h"

#include "OperGroups.h"
#include "OperSignature.h"
#include "ParallelTiles.h"
#include "TiledUnit.h"
#include "DataArrayValue.h"


// *****************************************************************************

const UnitClass* GetUnitClass(TokenID valueTypeID)
{
	const ValueClass* vc = ValueClass::FindByScriptName(valueTypeID);
	if (!vc)
		throwErrorF("UnitClass", 
			"'{}' is not recognized as a valid type indicator",
			valueTypeID
		);
	const UnitClass* uc = UnitClass::Find(vc);
	if (!uc)
		throwErrorF("UnitClass", 
			"'{}' is not a valid type for defining a unit",
			valueTypeID
		);
	return uc;
}

// *****************************************************************************
// DefaultUnitOperator
//
// The ONE callable form is DefaultUnit(<typename as string>), e.g.
//     unit<string> u := DefaultUnit('string');
// arg1 is a Void-domain string DataItem (Arg1Type below), NOT a unit: DefaultUnit(uint32)
// passes a unit and therefore does not bind ("Cannot find operator for these arguments"),
// which is what made the #1177 operator sweep report the group as having no callable form.
// The string form is exercised by the Operator regression config (UnitFunctions/unit_default).
// For a value type known at config-time there is no need for this operator at all: the
// per-type nullary group gives the same default unit as e.g. uint32().
// *****************************************************************************

DefaultUnitOperator::DefaultUnitOperator(AbstrOperGroup* gr)
	:	UnaryOperator(gr,
			ResultType::GetStaticClass(), 
			Arg1Type::GetStaticClass()
		)
{}

// Override Operator
bool DefaultUnitOperator::CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const
{
	dms_assert(args.size() == 1);

	if (!resultHolder)
	{
		checked_domain<Void>(args[0], "a1");
		SharedStr typeName = GetValue<Arg1Type::value_type>(args[0], 0);
		resultHolder = GetUnitClass(GetTokenID_mt(typeName.c_str()))->CreateDefault();
	}
	return true;
}

// The names of the sub-items that combine gives its result, one relation per factor, and also of the
// operators that give the relation to one factor per element (#1293). "twelfth_rel" was spelled
// "twelveth_rel" up to GeoDMS 20.23.1.
static CharPtr s_FactorRelNames[16] = {
	"first_rel", "second_rel", "third_rel", "fourth_rel"
,	"fifth_rel", "sixth_rel", "seventh_rel", "eighth_rel"
,	"ninth_rel", "tenth_rel", "eleventh_rel", "twelfth_rel"
,	"thirteenth_rel", "fourteenth_rel", "fifteenth_rel", "sixteenth_rel"
};
constexpr arg_index MAX_NR_COMBINE_FACTORS = 16;

bool UnitCombine_impl(AbstrUnit* res, const ArgSeqType& args, bool mustCalc, bool mustCreateBackRefs)
{
	arg_index n = args.size();
	dms_assert(res);
	for (arg_index i = 0; i!=n; ++i)
		if (!IsUnit(args[i]))
		{
			auto clsName = SharedStr(args[i]->GetDynamicClass()->GetName());
			throwErrorF("combine_unit", "ranged units expected, but argument {} is a {}", i + 1, clsName.c_str());
		}

	if (n > MAX_NR_COMBINE_FACTORS)
		throwErrorD("combine_unit", "the number of arguments exceeds the maximum of 16."
			"\nConsider splitting up the arguments into separate sub-combines and check that the product of the number of values doesn't exceed the possible range of the resulting unit."
		);

	// #1293: record the factors on the result, for combine_data, combine_rel and first_rel .. sixteenth_rel.
	// The meta phase creates the result and the calculation phase finds them there.
	if (!mustCalc)
	{
		auto factors = std::make_shared<AbstrUnit::CombineFactors>();
		factors->reserve(n);
		for (arg_index i = 0; i != n; ++i)
			factors->emplace_back(make_shared_tree(AsCertainUnit(args[i]), existing_obj{}));
		res->SetCombineFactors(std::move(factors));
	}

	SizeT productSize = 1;
	if (mustCalc)
	{
		// determine cardinality for result unit
		arg_index i = n;
		while ((i > 0) && (productSize != 0))
		{
			std::shared_ptr<const AbstrUnit> ithUnit = make_shared_tree(AsCertainUnit(args[--i]), existing_obj{});  // i gets decremented here
			SizeT unitCount = ithUnit->GetCount();
			SizeT newProductSize = productSize * unitCount;
			// SafeMul
			MG_CHECK2(!unitCount || newProductSize / unitCount == productSize,
				"Error in Combine operator: the product of the cardinalities of the arguments exceeds Max(SizeT)");

			productSize = newProductSize;
		}
		res->SetCount(productSize);
	}

	if (!mustCreateBackRefs)
		return true;

	// assign all combinations of domain elements in lexicographical order
	arg_index i = n;
	SizeT groupSize = 1;
	for (; i; --i)
	{
		std::shared_ptr<const AbstrUnit> ithUnit = make_shared_tree(AsCertainUnit(args[i - 1]), existing_obj{});
		AbstrDataItem* resSub = CreateDataItem(res, GetTokenID_mt(s_FactorRelNames[i-1]), res, ithUnit.get()).get(); // owned by res
		resSub->SetTSF(TSF_Categorical);

		if (!mustCalc)
			continue; // go to next sub)

		SizeT unitCount = ithUnit->GetCount();

		SizeT cycleSize = groupSize * unitCount;
		auto trd = res->GetTiledRangeData();
		visit<typelists::domain_elements>(ithUnit.get(),
			[resSub, trd, groupSize, cycleSize, unitCount MG_DEBUG_ALLOCATOR_SRC(res)] <typename V> (const Unit<V>*valuesUnit)
			{
				auto conv = CountableValueConverter<V>(valuesUnit->m_RangeDataPtr);
				auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(make_shared_tree(resSub, existing_obj{}), trd.get(), valuesUnit->m_RangeDataPtr
					, [trd, groupSize, cycleSize, unitCount, conv](AbstrDataObject* self, tile_id t) {
						tile_offset  tileSize = trd->GetTileSize(t);
						if (!tileSize)
							return; // an empty argument unit makes cycleSize 0; a small-range result (combine08/16) still has one, empty, tile
						SizeT tileStart = trd->GetFirstRowIndex(t);
						SizeT cyclePos = tileStart % cycleSize;
						SizeT iGroup = cyclePos % groupSize;
						SizeT iOrg = cyclePos / groupSize; assert(iOrg < unitCount);
						auto resultObj = mutable_array_cast<V>(self);
						auto resData = resultObj->GetWritableTile(t, dms_rw_mode::write_only_all);
						for (tile_offset r = 0; r != tileSize; ++r)
						{
							Assign(resData[r], conv.GetValue(iOrg));

							if (++iGroup == groupSize)
							{
								iGroup = 0;
								if (++iOrg == unitCount)
									iOrg = 0;
							}
						}
					}
					MG_DEBUG_ALLOCATOR_SRC(res->md_FullName +  ": = combine()")
				);
				resSub->m_DataObject = lazyTileFunctor.release();
			}
		);
		groupSize = cycleSize;
	}

	return true;
}

// Cartesian product
template <typename ResultValueType>
class UnitCombineOperator : public NullaryOperator
{
	typedef Unit<ResultValueType>  ResultType;
	typedef AbstrDataItem ResultSubType;
	typedef AbstrUnit     ArgType;

	bool m_MustCreateBackRefs;
public:
	UnitCombineOperator(AbstrOperGroup& cog, bool mustCreateBackRefs)
		:	NullaryOperator(&cog, ResultType::GetStaticClass())
		,	m_MustCreateBackRefs(mustCreateBackRefs)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		if (!resultHolder)
		{
			assert(!mustCalc);
			resultHolder = ResultType::GetStaticClass()->CreateResultUnit(resultHolder.GetNew());
		}
		auto resUnit = AsUnit(resultHolder.GetNew());
		assert(resUnit);
		resUnit->SetTSF(TSF_Categorical);

		return UnitCombine_impl(resUnit, args, mustCalc, m_MustCreateBackRefs);
	}
};

// *****************************************************************************
// combine_data, combine_rel and first_rel .. sixteenth_rel (#1293)
//
// An element of X := combine(A, B, C) is the mixed-radix number ((a * #B) + b) * #C + c of the
// indices a, b and c within the ranges of its factors: the last factor varies fastest, as in
// UnitCombine_impl. combine_data(X, a_rel, b_rel, c_rel) encodes one element per factor into an
// element of X, and combine_rel(x_rel, k) decodes factor k back out of one; first_rel(x_rel) ..
// sixteenth_rel(x_rel) are combine_rel(x_rel, 0) .. combine_rel(x_rel, 15). They work per element
// of their arguments and hold no data over X, so they work for an X of 10^12 elements and more,
// where the sub-items first_rel .. sixteenth_rel of a combine would each be an attribute over X.
// They take the factors from the record UnitCombine_impl leaves on X (AbstrUnit::GetCombineFactors).
// Until #1293 combine_data was a function in the prelude, computed from the values units of its
// arguments alone; for an X without such a record it still computes that way.
// *****************************************************************************

namespace {

	// the range of one factor, in the terms the encoding needs
	struct CombineFactor
	{
		SharedTreeItem   m_RangeItem;
		const AbstrUnit* m_Unit = nullptr;   // the unit that carries the range
		SizeT            m_Count = 0;
		Int64            m_LowerBound = 0;   // of a one-dimensional factor with a variable range
	};

	CombineFactor GetCombineFactor(const AbstrUnit* factor)
	{
		CombineFactor result;
		result.m_RangeItem = factor->GetCurrRangeItem();
		result.m_Unit = AsUnit(result.m_RangeItem.get());
		result.m_Count = result.m_Unit->GetCount();
		visit<typelists::domain_elements>(result.m_Unit, [&result] <typename F> (const Unit<F>* fu)
			{
				if constexpr (dimension_of_v<F> == 1 && has_var_range_v<F>)
					result.m_LowerBound = Int64(fu->GetRange().first);
			}
		);
		return result;
	}

	// The factors that the relations of combine_data(X, a1, .., an) index, one per relation. When the
	// combine that made X recorded its factors, those, unfolded through nested combines when that makes
	// their number equal to n, so that combine_data(V, a, b, c) on V := combine(combine(A, B), C) keeps
	// the meaning the prelude fold gave it. Without a record, the values units of the relations.
	struct CombineDataFactors
	{
		std::shared_ptr<const AbstrUnit::CombineFactors> m_Record; // keeps the factors below alive
		std::vector<const AbstrUnit*> m_Factors;
		bool m_FromRecord = false;
	};

	void CombineData_Unfold(const AbstrUnit::CombineFactors& factors, std::vector<const AbstrUnit*>& result, bool metaPhase)
	{
		for (const auto& factor : factors)
		{
			auto inner = metaPhase ? factor->GetCombineFactors() : factor->GetCurrCombineFactors();
			if (inner)
				CombineData_Unfold(*inner, result, metaPhase);
			else
				result.emplace_back(factor.get());
		}
	}

	auto CombineData_GetFactors(const AbstrOperGroup* og, const AbstrUnit* xUnit, const ArgSeqType& args, bool metaPhase) -> CombineDataFactors
	{
		arg_index nrRels = args.size() - 1;
		CombineDataFactors result;
		result.m_Record = metaPhase ? xUnit->GetCombineFactors() : xUnit->GetCurrCombineFactors();
		if (!result.m_Record)
		{
			for (arg_index i = 1; i <= nrRels; ++i)
				result.m_Factors.emplace_back(AsDataItem(args[i])->GetAbstrValuesUnit());
			return result;
		}
		result.m_FromRecord = true;
		if (result.m_Record->size() == nrRels)
		{
			for (const auto& factor : *result.m_Record)
				result.m_Factors.emplace_back(factor.get());
			return result;
		}
		CombineData_Unfold(*result.m_Record, result.m_Factors, metaPhase);
		if (result.m_Factors.size() != nrRels)
		{
			auto unfolded = (result.m_Factors.size() != result.m_Record->size())
				? mySSPrintF(", or {} after unfolding the combines among them", result.m_Factors.size())
				: SharedStr();
			og->throwOperErrorF("{} is a combine of {} factors{}, but {} relations were given"
			,	xUnit->GetSourceName().c_str(), result.m_Record->size(), unfolded.c_str(), nrRels
			);
		}
		return result;
	}

	// the index of v within the range of factor f; undefined for a null or a value outside that range
	template <typename V>
	SizeT CombineData_IndexInFactor(V v, const CombineFactor& f)
	{
		static_assert(dimension_of_v<V> == 1);
		if constexpr (has_undefines_v<V>)
			if (!IsDefined(v))
				return UNDEFINED_VALUE(SizeT);

		UInt64 index;
		if constexpr (is_bitvalue_v<V>)
			index = UInt64(v.base_value()) - UInt64(f.m_LowerBound);
		else if constexpr (std::is_signed_v<V>)
		{
			if (Int64(v) < f.m_LowerBound)
				return UNDEFINED_VALUE(SizeT);
			index = UInt64(Int64(v)) - UInt64(f.m_LowerBound);
		}
		else
		{
			if (f.m_LowerBound > 0 && UInt64(v) < UInt64(f.m_LowerBound))
				return UNDEFINED_VALUE(SizeT);
			index = UInt64(v) - UInt64(f.m_LowerBound);
		}
		if (index >= f.m_Count)
			return UNDEFINED_VALUE(SizeT);
		return index;
	}

	// acc[i] += (the index of element i of argA within factor f) * multiplier, or undefined where that element is null
	// or outside its factor. checkSum: also where the sum would leave X, which only an X without a record of
	// its factors allows, since only there the factors need not multiply to #X.
	template <typename V, typename IndexOf>
	void CombineData_AddIndices(SizeT* acc, SizeT n, const AbstrDataItem* argA, tile_id t, IndexOf&& indexOf, SizeT multiplier, SizeT countX, bool checkSum)
	{
		bool isParameter = argA->HasVoidDomainGuarantee();
		auto argTile = const_array_cast<V>(argA)->GetTile(isParameter ? 0 : t);

		SizeT parameterIndex = isParameter ? indexOf(V(argTile[0])) : 0;
		for (SizeT i = 0; i != n; ++i)
		{
			if (!IsDefined(acc[i]))
				continue;
			SizeT index = isParameter ? parameterIndex : indexOf(V(argTile[i]));
			if (!IsDefined(index) || (checkSum && index && index > (countX - 1 - acc[i]) / multiplier))
				acc[i] = UNDEFINED_VALUE(SizeT);
			else
				acc[i] += index * multiplier;
		}
	}

	template <typename V>
	void CombineData_AddArg(SizeT* acc, SizeT n, const AbstrDataItem* argA, tile_id t, const CombineFactor& f, SizeT multiplier, SizeT countX, bool checkSum)
	{
		if constexpr (dimension_of_v<V> == 1)
			CombineData_AddIndices<V>(acc, n, argA, t, [&f](V v) { return CombineData_IndexInFactor<V>(v, f); }, multiplier, countX, checkSum);
		else
		{
			// a two-dimensional factor has the value type of its relations: UnifyValues allows no other
			auto range = debug_cast<const Unit<V>*>(f.m_Unit)->GetRange();
			CombineData_AddIndices<V>(acc, n, argA, t, [&range](V v) { return Range_GetIndex_checked(range, v); }, multiplier, countX, checkSum);
		}
	}

	template <typename X>
	struct CombineDataOperator : UnaryOperator
	{
		CombineDataOperator(AbstrOperGroup* gr)
			: UnaryOperator(gr, DataArray<X>::GetStaticClass(), Unit<X>::GetStaticClass())
		{}

		// mirrors CreateResult below: combine_data(X; a1 (D), .., an (D)) -> X (D), void broadcasting.
		// The values unit of each relation is checked against a factor of X, which the vocabulary cannot
		// state, so those stay unclaimed.
		bool DescribeSignature(AbstrSignatureBuilder& sb) const override
		{
			sig_var V = sb.UnitVar("V"), D = sb.UnitVar("D");
			sb.MemberValueClass(V, ValueWrap<X>::GetStaticClass());
			sb.ArgName(0, "combined_unit");
			sb.ArgUnit(0, V);
			sb.RepeatArgs(1, no_sig_var, D, ValueComposition::Single);
			sb.ResultAttr(V, D, ValueComposition::Single);
			sb.DeferredRelation("the values unit of relation i matches factor i of the combine that made V");
			return true;
		}

		bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
		{
			arg_index nrRels = args.size() - 1;
			const AbstrUnit* xUnit = AsUnit(args[0]);
			assert(xUnit);

			if (!resultHolder)
			{
				assert(!mustCalc);
				if (!nrRels)
					GetGroup()->throwOperErrorF("expects a combined unit followed by one relation per factor, but got no relation");

				const AbstrUnit* domain = nullptr;
				for (arg_index i = 1; i <= nrRels; ++i)
				{
					if (!IsDataItem(args[i]))
						GetGroup()->throwOperErrorF("argument {} is not an attribute or a parameter", i + 1);
					auto argA = AsDataItem(args[i]);
					auto argValuesUnit = argA->GetAbstrValuesUnit();
					if (!argValuesUnit->CanBeDomain())
						GetGroup()->throwOperErrorF("argument {} has values of type {}, which cannot index a factor of a combined unit"
						,	i + 1, argValuesUnit->GetValueType()->GetNameID()
						);
					auto argDomain = argA->GetAbstrDomainUnit();
					if (!domain || domain->GetValueType() == ValueWrap<Void>::GetStaticClass())
						domain = argDomain;
					else
					{
						auto rightRole = mySSPrintF("Domain of argument {}", i + 1);
						domain->UnifyDomain(argDomain, "Domain of the first relation", rightRole.c_str(), UnifyMode(UM_Throw | UM_AllowVoidRight));
					}
				}

				auto factors = CombineData_GetFactors(GetGroup(), xUnit, args, true);
				if (factors.m_FromRecord)
					for (arg_index k = 0; k != nrRels; ++k)
					{
						const AbstrUnit* factor = factors.m_Factors[k];
						const AbstrUnit* argValuesUnit = AsDataItem(args[k + 1])->GetAbstrValuesUnit();
						// a literal comes with a default unit, of whatever integer type the parser gave it
						auto um = UnifyMode(UM_Throw | UM_AllowDefault);
						if (argValuesUnit->IsDefaultUnit() && argValuesUnit->GetNrDimensions() == 1 && factor->GetNrDimensions() == 1)
							um = um | UM_AllowTypeDiff;
						// the units of a calculation have no full name of their own, so the roles name them
						auto leftRole  = mySSPrintF("factor {} {} of {}", k + 1, factor->GetSourceName().c_str(), xUnit->GetSourceName().c_str());
						auto rightRole = mySSPrintF("Values {} of argument {}", argValuesUnit->GetSourceName().c_str(), k + 2);
						factor->UnifyValues(argValuesUnit, leftRole.c_str(), rightRole.c_str(), um);
					}

				resultHolder = CreateCacheDataItem(domain, xUnit);
				resultHolder->SetTSF(TSF_Categorical);
			}

			if (mustCalc)
			{
				auto res = AsDataItem(resultHolder.GetNew());
				auto factorUnits = CombineData_GetFactors(GetGroup(), xUnit, args, false);

				std::vector<CombineFactor> factors; factors.reserve(nrRels);
				for (auto factor : factorUnits.m_Factors)
					factors.emplace_back(GetCombineFactor(factor));

				// the weight of each index: the product of the counts of the later factors, saturated, which only
				// an X without a record can reach, and then CombineData_AddArg checks every sum against #X
				std::vector<SizeT> multipliers(nrRels);
				SizeT multiplier = 1;
				for (arg_index k = nrRels; k--; )
				{
					multipliers[k] = multiplier;
					SizeT count = factors[k].m_Count;
					multiplier = (count && multiplier > MAX_VALUE(SizeT) / count) ? MAX_VALUE(SizeT) : multiplier * count;
				}
				SizeT countX = AsUnit(xUnit->GetCurrRangeItem().get())->GetCount();
				bool checkSum = !factorUnits.m_FromRecord;

				auto domain = res->GetAbstrDomainUnit();
				DataWriteLock resLock(res);
				parallel_tileloop(domain->GetNrTiles(), [&](tile_id t)
					{
						SizeT n = domain->GetTileSize(t);
						std::vector<SizeT> acc(n, countX ? SizeT(0) : UNDEFINED_VALUE(SizeT));
						for (arg_index k = 0; k != nrRels; ++k)
						{
							auto argA = AsDataItem(args[k + 1]);
							visit<typelists::domain_elements>(argA->GetAbstrValuesUnit(), [&] <typename V> (const Unit<V>*)
								{
									CombineData_AddArg<V>(acc.data(), n, argA, t, factors[k], multipliers[k], countX, checkSum);
								}
							);
						}
						auto resTile = mutable_array_cast<X>(resLock)->GetWritableTile(t, dms_rw_mode::write_only_all);
						assert(resTile.size() == n);
						for (SizeT i = 0; i != n; ++i)
							resTile[i] = IsDefined(acc[i]) ? X(acc[i]) : UNDEFINED_VALUE(X);
					}
				);
				resLock.Commit();
			}
			return true;
		}
	};

	// the relation to factor pos of the combine that made the values unit of xA, per element of xA
	template <typename X>
	bool CombineRel_CreateResult(const AbstrOperGroup* og, TreeItemDualRef& resultHolder, const AbstrDataItem* xA, SizeT pos, bool mustCalc)
	{
		const AbstrUnit* xUnit = xA->GetAbstrValuesUnit();
		if (!resultHolder)
		{
			assert(!mustCalc);
			auto factors = xUnit->GetCombineFactors();
			if (!factors)
				og->throwOperErrorF("the values unit {} of its argument is not made by combine or combine_unit, so it has no factors to relate to"
				,	xUnit->GetSourceName().c_str()
				);
			if (pos >= factors->size())
				og->throwOperErrorF("{} is a combine of {} factors, so it has no factor at position {} (counted from 0)"
				,	xUnit->GetSourceName().c_str(), factors->size(), pos
				);
			resultHolder = CreateCacheDataItem(xA->GetAbstrDomainUnit(), (*factors)[pos].get());
			resultHolder->SetTSF(TSF_Categorical);
		}

		if (mustCalc)
		{
			auto factors = xUnit->GetCurrCombineFactors();
			MG_CHECK(factors && pos < factors->size());

			SizeT divisor = 1; // the product of the counts of the later factors, which cannot overflow since all of them multiply to #X
			for (SizeT j = pos + 1; j != factors->size(); ++j)
				divisor *= GetCombineFactor((*factors)[j].get()).m_Count;
			CombineFactor factor = GetCombineFactor((*factors)[pos].get());
			SizeT count = factor.m_Count;
			SizeT countX = AsUnit(xUnit->GetCurrRangeItem().get())->GetCount();

			auto res = AsDataItem(resultHolder.GetNew());
			auto domain = res->GetAbstrDomainUnit();
			DataReadLock argLock(xA);
			DataWriteLock resLock(res);
			visit<typelists::domain_elements>(factor.m_Unit, [&] <typename F> (const Unit<F>* factorUnit)
				{
					auto conv = CountableValueConverter<F>(factorUnit->m_RangeDataPtr);
					auto xData = const_array_cast<X>(xA);
					parallel_tileloop(domain->GetNrTiles(), [&](tile_id t)
						{
							auto xTile = xData->GetTile(t);
							auto resTile = mutable_array_cast<F>(resLock)->GetWritableTile(t, dms_rw_mode::write_only_all);
							assert(xTile.size() == resTile.size());
							for (SizeT i = 0, n = resTile.size(); i != n; ++i)
							{
								X x = xTile[i];
								if (!IsDefined(x) || SizeT(x) >= countX) // a value outside X gives null, as lookup does
									Assign(resTile[i], Undefined());
								else
									Assign(resTile[i], conv.GetValue((SizeT(x) / divisor) % count));
							}
						}
					);
				}
			);
			resLock.Commit();
		}
		return true;
	}

	template <typename X>
	bool CombineRel_DescribeSignature(AbstrSignatureBuilder& sb, bool hasPositionArg)
	{
		// mirrors CombineRel_CreateResult: x_rel: X (D) -> F (D), where F is a factor of the combine that made
		// X, which only the meta phase knows
		sig_var VX = sb.UnitVar("X"), D = sb.UnitVar("D"), F = sb.UnitVar("F");
		sb.MemberValueClass(VX, ValueWrap<X>::GetStaticClass());
		sb.ArgName(0, "x_rel");
		sb.ArgAttr(0, VX, D, ValueComposition::Single);
		if (hasPositionArg)
		{
			sb.ArgName(1, "position");
			sb.ArgMetaValue(1, ValueWrap<UInt32>::GetStaticClass(), "the position of the factor, counted from 0");
		}
		sb.ResultAttr(F, D, ValueComposition::Single);
		sb.DeferredRelation("F is a factor of the combine that made X");
		return true;
	}

	// first_rel .. sixteenth_rel
	template <typename X>
	struct FactorRelOperator : UnaryOperator
	{
		FactorRelOperator(AbstrOperGroup* gr, arg_index pos)
			: UnaryOperator(gr, AbstrDataItem::GetStaticClass(), DataArray<X>::GetStaticClass())
			, m_Pos(pos)
		{}

		bool DescribeSignature(AbstrSignatureBuilder& sb) const override
		{
			return CombineRel_DescribeSignature<X>(sb, false);
		}

		bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
		{
			assert(args.size() == 1);
			return CombineRel_CreateResult<X>(GetGroup(), resultHolder, AsDataItem(args[0]), m_Pos, mustCalc);
		}

	private:
		arg_index m_Pos;
	};

	// combine_rel(x_rel, pos), for templates; pos is a parameter that the meta phase reads
	template <typename X>
	struct CombineRelOperator : BinaryOperator
	{
		CombineRelOperator(AbstrOperGroup* gr)
			: BinaryOperator(gr, AbstrDataItem::GetStaticClass(), DataArray<X>::GetStaticClass(), DataArray<UInt32>::GetStaticClass())
		{}

		bool DescribeSignature(AbstrSignatureBuilder& sb) const override
		{
			return CombineRel_DescribeSignature<X>(sb, true);
		}

		bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
		{
			assert(args.size() == 2);
			checked_domain<Void>(args[1], "a2");
			UInt32 pos = mustCalc ? GetCurrValue<UInt32>(args[1], 0) : GetValue<UInt32>(args[1], 0);
			if (!IsDefined(pos))
				GetGroup()->throwOperErrorF("the position of the factor is null");
			return CombineRel_CreateResult<X>(GetGroup(), resultHolder, AsDataItem(args[0]), pos, mustCalc);
		}
	};
} // namespace

// *****************************************************************************
// CastUnitOperator
// *****************************************************************************

class CastUnitOperatorBase : public UnaryOperator
{
	typedef AbstrUnit Arg1Type;
	const UnitClass* m_ResultUnitClass;
public:
	CastUnitOperatorBase(AbstrOperGroup* gr, const UnitClass* resultUnitClass);

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override;
};

// *****************************************************************************
// CastUnitOperator
// *****************************************************************************

CastUnitOperatorBase::CastUnitOperatorBase(AbstrOperGroup* gr, const UnitClass* resultUnitClass)
	:	UnaryOperator(gr, resultUnitClass,  Arg1Type::GetStaticClass())
	,	m_ResultUnitClass(resultUnitClass)
{
	assert(m_ResultUnitClass == GetUnitClass(gr->GetNameID()));
}

// Override Operator
bool CastUnitOperatorBase::CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc)  const
{
	assert(args.size() == 1);

	const Arg1Type* arg1 = debug_cast<const Arg1Type*>(args[0]);
	assert(arg1);

	// TODO G8: not Tmp, since that calls SetMaxRange; call CreateResultUnit and copy the range and intersect as part of DuplFrom with mustCalc, as Range(srcUnit, lb, ub);
	// TODO G8: Of schedule geen calc phase voor dit soort operator die in meta-info tijd een compleet resultaat geven (dus zonder range output) 
	if (!resultHolder)
	{
		assert(!mustCalc);
		resultHolder = m_ResultUnitClass->CreateTmpUnit(resultHolder.GetNew());
		auto resUnit = AsUnit(resultHolder.GetNew());
		dms_assert(resUnit);
		resUnit->DuplFrom(arg1);
	}

	return true;
}

// *****************************************************************************
// DomainUnitOperator
// *****************************************************************************

class DomainUnitOperator : public UnaryOperator
{
	typedef AbstrUnit     ResultType;
	typedef AbstrDataItem Arg1Type;
public:
	DomainUnitOperator(AbstrOperGroup* gr)
		:	UnaryOperator(gr,
				ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass()
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const Arg1Type* arg1 = debug_cast<const Arg1Type*>(args[0]);

		dms_assert(arg1);

		const AbstrUnit* arg1Unit;
		do {
			arg1Unit = arg1->GetAbstrDomainUnit();
			if (! arg1Unit->IsDefaultUnit())
				break;
			arg1 = AsDataItem(arg1->GetReferredItem()).get();
		} while (arg1);

		resultHolder = arg1Unit;
		dms_assert(!mustCalc || AsUnit(arg1Unit->GetCurrRangeItem())->HasTiledRangeData());
		return true;
	}
};

class ValuesUnitOperator : public UnaryOperator
{
	typedef AbstrUnit     ResultType;
	typedef AbstrDataItem Arg1Type;
public:
	ValuesUnitOperator(AbstrOperGroup* gr)
		:	UnaryOperator(gr,
				ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass()
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const Arg1Type* arg1 = debug_cast<const Arg1Type*>(args[0]);

		dms_assert(arg1);

		const AbstrUnit* arg1Unit;
		do {
			arg1Unit = arg1->GetAbstrValuesUnit();
			if (! arg1Unit->IsDefaultUnit())
				break;
			arg1 = AsDataItem(arg1->GetCurrRefItem()).get();
		} while (arg1);

		resultHolder = arg1Unit;
		dms_assert(!mustCalc || AsUnit(arg1Unit->GetCurrRangeItem())->HasTiledRangeData());
		return true;
	}
};

// *****************************************************************************
// BaseUnitOperator
// *****************************************************************************

oper_arg_policy oap_BaseUnit[2] = { 
	oper_arg_policy::calc_always,  // arg0 is metric str
	oper_arg_policy::calc_never    // arg1 is a default unit for a valuesType
};
SpecialOperGroup sog_BaseUnit("BaseUnit", 2, oap_BaseUnit);

class AbstrBaseUnitOperator : public BinaryOperator
{
	typedef DataArray<SharedStr> Arg1Type;

public:
	AbstrBaseUnitOperator(const UnitClass* resCls)
		:	BinaryOperator(&sog_BaseUnit
			,	resCls
			,	Arg1Type::GetStaticClass()
			,	resCls
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 2);
		if (mustCalc)
		{
			dms_assert(resultHolder);
			return true;
		}
		const AbstrUnit* arg2 = AsUnit(args[1]);
		dms_assert(arg2);

		checked_domain<Void>(args[0], "a1");

		SharedStr baseUnitName = GetCurrValue<Arg1Type::value_type>(args[0], 0);
		auto result_owner = arg2->GetUnitClass()->CreateTmpUnit(resultHolder.GetNew()); AbstrUnit* result = result_owner.get();
		resultHolder = result;

		if (baseUnitName.empty())
			result->SetMetric({});
		else
		{
			auto m = std::make_unique<UnitMetric>();
			m->m_BaseUnits[baseUnitName] = 1;
			result->SetMetric(m.release());
		}
//		result->SetDataInMem();

		return true;
	}
};

template <typename T>
struct BaseUnitOperator : AbstrBaseUnitOperator
{
	typedef Unit<T> ResultType;
	BaseUnitOperator()
		:	AbstrBaseUnitOperator(ResultType::GetStaticClass())
	{}
};

// *****************************************************************************
// CrsUnitOperator
//
// Stamps a coordinate reference system onto a unit, TRANSPARENTLY: the result
// expresses whatever the wrapped unit expressed (metric or projection) and adds the
// CRS in its own slot. It exists so a CRS is a first-class component of a unit's
// DataController key expression instead of being packed into a base-unit symbol as
// "<SpatialReference>\xFF<DialogData>".
//
// Unit<V>::GetKeyExprImpl currently emits it as a WRAPPER around the old term:
//     (CrsUnit "EPSG:28992" (BaseUnit "EPSG:28992\xFFwmts_layer" (fpoint)))
// so the packed metric -- and therefore every existing 0xFF fallback -- keeps working
// unchanged while the new channel is validated alongside it. See
// doc/development/crs-metric-decoupling.md (Stage 2; the packing goes in Stage 7).
// *****************************************************************************

oper_arg_policy oap_CrsUnit[2] = {
	oper_arg_policy::calc_always,  // arg0 is the spatial reference str
	oper_arg_policy::calc_never    // arg1 is the unit being wrapped; only its metainfo is read
};
SpecialOperGroup sog_CrsUnit("CrsUnit", 2, oap_CrsUnit);

class AbstrCrsUnitOperator : public BinaryOperator
{
	typedef DataArray<SharedStr> Arg1Type;

public:
	AbstrCrsUnitOperator(const UnitClass* resCls)
		:	BinaryOperator(&sog_CrsUnit
			,	resCls
			,	Arg1Type::GetStaticClass()
			,	resCls
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 2);
		if (mustCalc)
		{
			dms_assert(resultHolder);
			return true;
		}
		const AbstrUnit* arg2 = AsUnit(args[1]);
		dms_assert(arg2);

		checked_domain<Void>(args[0], "a1");

		SharedStr crsName = GetCurrValue<Arg1Type::value_type>(args[0], 0);
		auto result_owner = arg2->GetUnitClass()->CreateTmpUnit(resultHolder.GetNew()); AbstrUnit* result = result_owner.get();
		resultHolder = result;

		// Transparent: carry over exactly what the wrapped unit expresses. Units never have
		// both a metric and a projection (see the assert in UnitCreators.cpp), so this is a
		// genuine either/or rather than a copy of both.
		if (auto p = arg2->GetProjection())
			result->SetProjection(p);
		else
			result->SetMetric(arg2->GetMetric());

		if (!crsName.empty())
		{
			result->SetCrs(new UnitCrs(GetTokenID_mt(crsName.begin(), crsName.send())));

			// Stage 6: give a PROJECTED coordinate reference system a real linear metric,
			// so that area(geom, m2) on an EPSG:28992 chain simply works instead of
			// comparing m^2 against a squared CRS tag (issue #1119). Only meaningful once
			// the wrapped unit no longer supplies the packed tag as its "metric", which is
			// why Stage 6 and Stage 7 land together.
			//
			// PROJECTED ONLY, and metre-based only:
			//  - a GEOGRAPHIC crs reports "degree" here, and giving EPSG:4326 a {degree:1}
			//    metric would turn area(geom, m2) from a warning into a HARD ERROR for
			//    2BURP, 2UP and tst/Unit/CRS. Those stay metric-free; geodesic area is a
			//    separate question.
			//  - a projected crs in some other linear unit (US survey foot) would need the
			//    scale factor to be folded in as a projection factor. RULING 2026-07-29:
			//    leave GetUnitlabeledScalePair().second unused for now, so such a crs also
			//    stays metric-free and keeps today's label-only behaviour.
			if (IsEmpty(result->GetMetric()) && !result->GetProjection())
			{
				auto labelScale = result->GetUnitlabeledScalePair(); // reads the slot set above
				auto unitName = SharedStr(labelScale.first);
				bool isMetre = (unitName == "metre" || unitName == "meter" || unitName == "m")
					&& labelScale.second == 1.0;
				if (isMetre)
				{
					auto m = std::make_unique<UnitMetric>();
					m->m_BaseUnits[SharedStr("m")] = 1; // 'm', matching baseunit('m', ...) in configs
					result->SetMetric(m.release());
				}
			}
		}

		return true;
	}
};

template <typename T>
struct CrsUnitOperator : AbstrCrsUnitOperator
{
	typedef Unit<T> ResultType;
	CrsUnitOperator()
		:	AbstrCrsUnitOperator(ResultType::GetStaticClass())
	{}
};

// *****************************************************************************
// BaseUnitOperator
// *****************************************************************************

// BaseUnitObsoleteOperator stays to give configurations the migration message below; once it goes,
// the fourth argument can become oper_arg_policy::calc_always

class BaseUnitObsoleteOperator : public BinaryOperator
{
	typedef AbstrUnit         ResultType;
	typedef DataArray<SharedStr> Arg1Type;
	typedef DataArray<SharedStr> Arg2Type;

public:
	BaseUnitObsoleteOperator()
		:	BinaryOperator(&sog_BaseUnit 
			,	ResultType::GetStaticClass()
			,	Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		GetGroup()->throwOperError("BaseUnit(String, String) OBSOLETE; use BseUnit(String, ValueType");
	}
};

// *****************************************************************************
// UnitGridsetOperator
// *****************************************************************************

oper_arg_policy oap_gridset [4]= { oper_arg_policy::calc_as_result, oper_arg_policy::calc_always, oper_arg_policy::calc_always, oper_arg_policy::calc_never };
SpecialOperGroup sog_gridset("gridset", 4, oap_gridset); // , oper_policy::calc_requires_metainfo);

class AbstrUnitGridsetOperator : public QuaternaryOperator
{
	typedef AbstrUnit     Arg1Type;
	typedef AbstrDataItem Arg2Type; // scale factor
	typedef AbstrDataItem Arg3Type; // offset coordinate

public:
	AbstrUnitGridsetOperator(const UnitClass* resCls)
		:	QuaternaryOperator(&sog_gridset
			,	resCls
			,	Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			,	Arg3Type::GetStaticClass()
			,	resCls
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 4);

		const Arg1Type *arg1 = AsUnit(args[0]);
		dms_assert(arg1);
		checked_domain<Void>(args[1], "a2");
		checked_domain<Void>(args[2], "a3");

		dms_assert(AsUnit(args[3])->GetUnitClass() == GetResultClass());
		if (!resultHolder)
		{
			dms_assert(!mustCalc);
			auto result_owner = AsUnit(args[3])->GetUnitClass()->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* result = result_owner.get();
			dms_assert(result);
			resultHolder = result;
		}
		AbstrUnit* result = AsUnit(resultHolder.GetNew());
		dms_assert(result);

		const AbstrDataItem* factorItem = AsDataItem(args[1]);
		const AbstrDataItem* offsetItem = AsDataItem(args[2]);

		DataReadLock factorLock(factorItem), offsetLock(offsetItem);

		DPoint relFactor = factorItem->GetCurrRefObj()->GetValueAsDPoint(0);
		DPoint relOffset = offsetItem->GetCurrRefObj()->GetValueAsDPoint(0);

		MG_CHECK2(IsDefined(relFactor), "Error in Projection calculation: relFactor (arg2) is UNDEFINED");
		MG_CHECK2(IsDefined(relOffset), "Error in Projection calculation: relOffset (arg3) is UNDEFINED");

		CrdTransformation trRel(relOffset, relFactor);

		const UnitProjection* orgP = arg1->GetCurrProjection();
		if (orgP)
			trRel *= *orgP;
		auto newP = std::make_unique<UnitProjection>((orgP ? orgP->GetBaseUnit() : AsUnit(arg1->GetCurrUltimateItem()).get()), trRel.Offset(), trRel.Factor());
		result->SetProjection(newP.release());

		// A gridset is a rescaling of arg1, not a change of coordinate reference system:
		// rdc_mm is still EPSG:28992, just in millimetres. Carry the CRS across so a
		// chained gridset does not lose it. See doc/development/crs-metric-decoupling.md.
		result->SetCrs(arg1->GetCurrCrs());

		if (mustCalc)
		{
			DRect range = arg1->GetRangeAsDRect();
			if (IsDefined(range))
				CrdTransformation(relOffset, relFactor).InplReverse(range);

			result->SetRangeAsDRect( range );
		}

		return true;
	}
};

template <class T> // = spoint, ipoint, fpoint, dpoint
struct UnitGridsetOperator : AbstrUnitGridsetOperator
{
	typedef Unit<T> ResultType;
	UnitGridsetOperator()
		:	AbstrUnitGridsetOperator(ResultType::GetStaticClass())
	{}
};

// *****************************************************************************
// UnitRangeOperator
// *****************************************************************************

template <typename T> auto
CutTileSpec(const TiledRangeData<T>* arg1, const Range<T>& bounds)
{
	tile_id nrTiles = arg1->GetNrTiles();

	tile_id nrResultTiles = 0;

	for (tile_id t = 0; t != nrTiles; ++t)
	{
		auto srcTileBounds = arg1->GetTileRange(t);
		if (!IsDefined(srcTileBounds))
			continue;
		auto tileBounds = bounds & srcTileBounds;
		if (!tileBounds.empty())
			++nrResultTiles;
	}

	std::vector<Range<T>> tileRanges;
	tileRanges.reserve(nrResultTiles);

	for (tile_id t = 0; t != nrTiles; ++t)
	{
		auto srcTileBounds = arg1->GetTileRange(t);
		if (!IsDefined(srcTileBounds))
			continue;
		auto tileBounds = bounds & srcTileBounds;
		if (!tileBounds.empty())
			tileRanges.push_back(tileBounds);
	}
	dms_assert(tileRanges.size() == nrResultTiles);
	return tileRanges;
}

template <typename T>
bool CreateRangeUnit(TreeItemDualRef& resultHolder, const AbstrOperGroup* whoCalled, const AbstrUnit* arg1, const TreeItem* lbItem, const TreeItem* ubItem, bool isCategorical, bool mustCalc)
{
	if (!resultHolder)
	{
		checked_domain<Void>(lbItem, "LowerBound");
		checked_domain<Void>(ubItem, "UpperBound");

		assert(!mustCalc);
		auto result_owner = Unit<T>::GetStaticClass()->CreateResultUnit(resultHolder.GetNew());
		auto result = mutable_unit_cast<T>(result_owner.get());
		assert(result);
		resultHolder = result;
		if (arg1)
			result->DuplFrom(arg1);
		if (isCategorical)
			result->SetTSF(TSF_Categorical);
	}

	if (mustCalc)
	{
		auto result = debug_cast<Unit<T>*>(resultHolder.GetNew());
		assert(result);

		T lb = GetTheCurrValue<T>(lbItem);
		T ub = GetTheCurrValue<T>(ubItem);

		MG_USERCHECK2(IsDefined(lb), "lowerBound (arg2) in call to range(orgUnit, lowerBound, upperBound) is UNDEFINED");
		MG_USERCHECK2(IsDefined(ub), "upperBound (arg3) in call to range(orgUnit, lowerBound, upperBound) is UNDEFINED");

		Range<T> bounds(lb, ub);
		MG_USERCHECK(IsLowerBound(bounds.first, bounds.second));
		if (arg1)
		{
			auto arg1Unit = dynamic_cast<const Unit<T>*>(arg1);

			if (!bounds.empty() && arg1Unit)
			{
				auto arg1Range = arg1Unit->GetCurrSegmInfo()->GetRange();
				if (!arg1Range.empty())
				{
					auto orgBounds = bounds;
					bounds &= arg1Range;
					if (bounds != orgBounds)
					{
						reportF(SeverityTypeID::ST_Warning,
							"{}: The specified range {} is limited by range {} of the given base unit resulting in range {}"
						,	whoCalled->GetNameStr()
						,	AsString(orgBounds), AsString(arg1Range), AsString(bounds)
						);
					}
				}
				if constexpr (!has_simple_range_v<T> && !has_small_range_v<T>)
				{
					if (!arg1->GetTiledRangeData()->IsCovered() && !bounds.empty())
					{
						auto tileBreaks = CutTileSpec<T>(arg1Unit->GetCurrSegmInfo(), bounds);
						if (tileBreaks.size() > 1)
						{
							result->SetIrregularTileRange(std::move(tileBreaks));
							return true;
						}
						if (tileBreaks.size() == 1)
							bounds = tileBreaks[0];
						else
							bounds = Range<T>();
					}
				}
			}
		}
		result->SetRange(bounds);
	}
	return true;
}

template <class T> // = spoint, ipoint, fpoint, dpoint, float32/64, (u)int32/16/8
class UnitRange2Operator : public BinaryOperator
{
	using ResultType = Unit<T>;
	using BoundType = DataArray<T>; // LowerBound, UpperBound

	static_assert(has_var_range_field_v<T>);
	bool m_IsCatRangeFunc;

public:
	UnitRange2Operator(AbstrOperGroup* gr, bool isCatRangeFunc)
		:	BinaryOperator(gr, ResultType::GetStaticClass(), BoundType::GetStaticClass(), BoundType::GetStaticClass())
		,	m_IsCatRangeFunc(isCatRangeFunc)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		return CreateRangeUnit<T>(resultHolder, GetGroup(), nullptr, args[0], args[1], m_IsCatRangeFunc, mustCalc);
	}
};

template <class T> // = spoint, ipoint, fpoint, dpoint, float32/64, (u)int32/16/8
class UnitRange3Operator : public TernaryOperator
{
	using ResultType = Unit<T>;
	using Arg1Type = AbstrUnit;
	using BoundType = DataArray<T>; // LowerBound, UpperBound

	static_assert(has_var_range_field_v<T>);
	bool m_IsCatRangeFunc;

public:
	UnitRange3Operator(AbstrOperGroup* gr, bool isCatRangeFunc)
		: TernaryOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), BoundType::GetStaticClass(), BoundType::GetStaticClass())
		, m_IsCatRangeFunc(isCatRangeFunc)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);
		auto arg1 = AsUnit(args[0]); assert(arg1);
		return CreateRangeUnit<T>(resultHolder, GetGroup(), arg1, args[1], args[2], m_IsCatRangeFunc, mustCalc);
	}
};

// *****************************************************************************
// AbstrUnitPropOperator
// *****************************************************************************

class AbstrUnitPropOperator : public UnaryOperator
{
public:
	AbstrUnitPropOperator(AbstrOperGroup* gr, const DataItemClass* dic, const UnitClass* uc)
		:	UnaryOperator(gr, dic, uc)
	{
		dms_assert(dic->GetValuesType()->GetValueComposition() == ValueComposition::Single);
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		WeakPtr<const DataItemClass> dic = debug_cast<const DataItemClass*>(m_ResultClass);

		if (!resultHolder)
		{
			dms_assert(!mustCalc);
			resultHolder = CreateCacheDataItem(
				Unit<Void>::GetStaticClass()->CreateDefault(),
				UnitClass::Find(dic->GetValuesType())->CreateDefault(),
				ValueComposition::Single
			);
		}
		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			dms_assert(res);

			DataWriteLock resLock(res); 

			Calculate(resLock.get(), AsUnit(args[0]));

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* res, const AbstrUnit* e1) const= 0;
};

// *****************************************************************************
// GetProjection
// *****************************************************************************

oper_arg_policy oap_UnitProps = oper_arg_policy::calc_never;
oper_arg_policy oap_UnitBaseProps = oper_arg_policy::calc_as_result;
SpecialOperGroup sog_MetricFactor    ("GetMetricFactor",     1, &oap_UnitProps);
SpecialOperGroup sog_ProjectionFactor("GetProjectionFactor", 1, &oap_UnitProps); // , oper_policy::calc_requires_metainfo);
SpecialOperGroup sog_ProjectionOffset("GetProjectionOffset", 1, &oap_UnitProps); // , oper_policy::calc_requires_metainfo);
SpecialOperGroup sog_ProjectionBase  ("GetProjectionBase",   1, &oap_UnitBaseProps, oper_policy(oper_policy::existing|oper_policy::dynamic_result_class));


template <typename P>
class GetMetricFactorOperator : public AbstrUnitPropOperator
{
	typedef Unit<P>            Arg1Type;
	typedef DataArray<Float64> ResultType;

public:
	// Override Operator
	GetMetricFactorOperator()
		:	AbstrUnitPropOperator(&sog_MetricFactor, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e1) const override
	{
		WeakPtr<ResultType> result = mutable_array_cast<Float64>(res);

		result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0] = e1->GetCurrMetric()->m_Factor;
	}
};

template <typename P>
class GetProjectionFactorOperator : public AbstrUnitPropOperator
{
	typedef Unit<P>           Arg1Type;
	typedef DataArray<DPoint> ResultType;

public:
	// Override Operator
	GetProjectionFactorOperator()
		:	AbstrUnitPropOperator(&sog_ProjectionFactor, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e1) const override
	{
		WeakPtr<ResultType> result = mutable_array_cast<DPoint>(res);

		const UnitProjection* p = e1->GetCurrProjection();
		result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0] = UnitProjection::GetCompositeTransform(p).Factor();
	}
};

template <typename P>
class GetProjectionOffsetOperator : public AbstrUnitPropOperator
{
	typedef Unit<P>           Arg1Type;
	typedef DataArray<DPoint> ResultType;

public:
	// Override Operator
	GetProjectionOffsetOperator()
		:	AbstrUnitPropOperator(&sog_ProjectionOffset, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e1) const override
	{
		WeakPtr<ResultType> result = mutable_array_cast<DPoint>(res);

		const UnitProjection* p = e1->GetCurrProjection();
		result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0] = UnitProjection::GetCompositeTransform(p).Offset();
	}
};

template <typename P>
class GetProjectionBaseOperator : public UnaryOperator
{
public:
	GetProjectionBaseOperator()
		:	UnaryOperator(&sog_ProjectionBase, AbstrUnit::GetStaticClass(), Unit<P>::GetStaticClass())
	{
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrUnit* arg1U = AsUnit(args[0]);
		const UnitProjection* p = arg1U->GetCurrProjection();
		resultHolder = p ? p->GetCompositeBase() : arg1U;

		return true;
	}
};

// *****************************************************************************
// NrOfRowsOperator
// *****************************************************************************

template <class E1>
class NrOfRowsOperator : public AbstrUnitPropOperator
{
	using Arg1Type = Unit<E1>;
	using ResultValueType = std::conditional_t<sizeof(E1) <= 4, UInt32, UInt64>;
	using ResultType = DataArray<ResultValueType>;

public:
	// Override Operator
	NrOfRowsOperator(AbstrOperGroup* gr)
		:	AbstrUnitPropOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e1) const override
	{
		ResultType *result = mutable_array_cast<ResultValueType>(res);
		assert(result);

		result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0] = e1->GetCount();
	}
};

// *****************************************************************************
// BoundsOperator
// *****************************************************************************

class AbstrBoundOperator : public UnaryOperator
{

public:
	// Override Operator
	AbstrBoundOperator(AbstrOperGroup* gr, const DataItemClass* dic, const UnitClass* uc)
		:	UnaryOperator(gr, dic, uc)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrUnit *e1 = AsUnit(args[0]);
		assert(e1);

		if (!resultHolder)
		{
			dms_assert(!mustCalc);
			resultHolder = CreateCacheDataItem(Unit<Void>::GetStaticClass()->CreateDefault(), e1);
		}

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			dms_assert(res);
			DataWriteLock resLock(res); 

			Calculate(resLock.get(), e1);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* res, const AbstrUnit* e1) const = 0;
};

template <class E1>
class LowerBoundOperator : public AbstrBoundOperator
{
	typedef Unit<E1>               Arg1Type;
	typedef DataArray<E1>          ResultType;

public:
	// Override Operator
	LowerBoundOperator(AbstrOperGroup* gr)
		:	AbstrBoundOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e) const override
	{
		const Arg1Type *e1 = debug_cast<const Arg1Type*>(e);
		assert(e1);

		ResultType *result = mutable_array_cast<E1>(res);
		assert(result);

		if constexpr (!is_bitvalue_v<E1>)
		{
			if (e1->GetRange().first == UNDEFINED_VALUE(E1))
				reportD(SeverityTypeID::ST_Warning, "LowerBoundOperator: start of range of argument is UNDEFINED");
			if (e1->GetRange().first == MAX_VALUE(E1))
				reportD(SeverityTypeID::ST_Warning, "LowerBoundOperator: start of range of argument is MAX_VALUE");

			if constexpr (is_signed_v<E1>)
			{
				if (e1->GetRange().first == MIN_VALUE(E1))
					reportD(SeverityTypeID::ST_Warning, "LowerBoundOperator: start of range of argument is MIN_VALUE");
			}
		}
		result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0] = e1->GetRange().first;
	}
};

template <class E1>
class UpperBoundOperator : public AbstrBoundOperator
{
	typedef Unit<E1>               Arg1Type;
	typedef DataArray<E1>          ResultType;

public:
	// Override Operator
	UpperBoundOperator(AbstrOperGroup* gr)
		:	AbstrBoundOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e) const override
	{
		const Arg1Type *e1 = debug_cast<const Arg1Type*>(e);
		dms_assert(e1);

		ResultType *result = mutable_array_cast<E1>(res);
		dms_assert(result);

		if constexpr (!is_bitvalue_v<E1>)
		{
			if (e1->GetRange().second == UNDEFINED_VALUE(E1))
				reportD(SeverityTypeID::ST_Warning, "UpperBoundOperator: end of range of argument is UNDEFINED");
			if (e1->GetRange().second == MAX_VALUE(E1))
				reportD(SeverityTypeID::ST_Warning, "UpperBoundOperator: end of range of argument is MAX_VALUE");

			if constexpr (is_signed_v<E1>)
			{
				if (e1->GetRange().second == MIN_VALUE(E1))
					reportD(SeverityTypeID::ST_Warning, "UpperBoundOperator: end of range of argument is MIN_VALUE");
			}
		}
		result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0] = e1->GetRange().second;
	}
};

template <class E1>
class BoundRangeOperator : public AbstrBoundOperator
{
	typedef Unit<E1>      Arg1Type;
	typedef DataArray<E1> ResultType;

public:
	// Override Operator
	BoundRangeOperator(AbstrOperGroup* gr)
		:	AbstrBoundOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e) const override
	{
		const Arg1Type *e1 = debug_cast<const Arg1Type*>(e);
		dms_assert(e1);

		ResultType *result = mutable_array_cast<E1>(res);
		dms_assert(result);

		result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0] = Size( e1->GetRange() );
	}
};

template <class E1>
class BoundCenterOperator : public AbstrBoundOperator
{
	typedef Unit<E1>               Arg1Type;
	typedef DataArray<E1>          ResultType;

public:
	// Override Operator
	BoundCenterOperator(AbstrOperGroup* gr)
		:	AbstrBoundOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e) const override
	{
		const Arg1Type *e1 = debug_cast<const Arg1Type*>(e);
		dms_assert(e1);

		ResultType *result = mutable_array_cast<E1>(res);
		dms_assert(result);

		result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0] = Center( e1->GetRange() );
	}
};

// *****************************************************************************
//	 TiledUnit
// *****************************************************************************

#include "TiledRangeData.h"

CommonOperGroup cog_TiledUnit(token::TiledUnit);

// (LB, UB) -> TiledUnit
struct AbstrTiledUnitOper: BinaryOperator
{
	AbstrTiledUnitOper(const UnitClass* uc, const DataItemClass* dic)
		: BinaryOperator(&cog_TiledUnit, uc, dic, dic)
	{
		dms_assert(uc->GetValueType() == dic->GetValuesType());
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* a1 = AsDataItem(args[0]);
		const AbstrDataItem* a2 = AsDataItem(args[1]);
		a1->GetAbstrDomainUnit()->UnifyDomain(a2->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
		a1->GetAbstrValuesUnit()->UnifyDomain(a2->GetAbstrValuesUnit(), "v1", "v2", UM_Throw);

		if (!resultHolder)
		{
			assert(!mustCalc);
			auto result_owner = debug_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* result = result_owner.get();
			assert(result);
			result->SetTSF(TSF_Categorical);

			resultHolder = result;
			result->DuplFrom(a1->GetAbstrValuesUnit());
		}

		if (mustCalc)
		{
			AbstrUnit* res = AsUnit(resultHolder.GetNew());
			dms_assert(res);

			DataReadLock l1(a1), l2(a2);
			Calculate(res, a1, a2);
		}
		return true;
	}
	virtual void Calculate(AbstrUnit* res, const AbstrDataItem* lb, const AbstrDataItem* ub) const = 0;
};

template <typename D>
struct TiledUnitOper: AbstrTiledUnitOper
{
	typedef Unit<D>      ResultType;
	typedef DataArray<D> ArgType;
	TiledUnitOper()
		:	AbstrTiledUnitOper(ResultType::GetStaticClass(), ArgType::GetStaticClass())
	{}

	void Calculate(AbstrUnit* res, const AbstrDataItem* lb, const AbstrDataItem* ub) const override
	{
		auto
			lbData = const_array_cast<D>(lb)->GetDataRead(),
			ubData = const_array_cast<D>(ub)->GetDataRead();
		ResultType* resUnit = mutable_unit_cast<D>(res);

		SizeT nrTiles = lbData.size();
		dms_assert(nrTiles == ubData.size());

		CheckNrTiles(nrTiles);
		std::vector<Range<D> > segmInfo;
		segmInfo.reserve(nrTiles);

		auto
			lbi = lbData.begin(),
			lbe = lbData.end(),
			ubi = ubData.begin();
		dms_assert(ubi + (lbe - lbi) == ubData.end()); // dms_assert(lbData.size() == ubData.size());

		while (lbi != lbe)
		{
			Range<D> tileRange(*lbi++, *ubi++);
			// The tiles come from user data. An empty tile used to reach an assert-only guard in
			// AbstrUnit::GetTileSizeAsI64Rect, and the 1-D lookups assume sorted, disjoint tiles.
			if (!IsDefined(tileRange.first) || !IsDefined(tileRange.second) || tileRange.empty())
				GetGroup()->throwOperErrorF("TiledUnit: tile {} has an empty or undefined range", segmInfo.size());
			if constexpr (is_numeric_v<D>)
				if (!segmInfo.empty() && tileRange.first < segmInfo.back().second)
					GetGroup()->throwOperErrorF("TiledUnit: tile {} starts before the end of tile {}; tiles must be sorted and disjoint", segmInfo.size(), segmInfo.size() - 1);
			segmInfo.push_back(tileRange);
		}

		resUnit->SetIrregularTileRange(move(segmInfo));
	}
};

// (Unit, Size) -> TiledUnit
struct AbstrTiledUnitFromSizeOper: UnaryOperator
{
	AbstrTiledUnitFromSizeOper(const UnitClass* uc, const DataItemClass* dic)
		: UnaryOperator(&cog_TiledUnit, uc, dic)
	{
		dms_assert(uc->GetValueType() == dic->GetValuesType());
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrDataItem* a1 = AsDataItem(args[0]);
		checked_domain<Void>(a1, "a1");

		if (!resultHolder)
		{
			assert(!mustCalc);
			auto result_owner = debug_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* result = result_owner.get();
			assert(result);
			result->SetTSF(TSF_Categorical);

			resultHolder = result;
			result->DuplFrom(a1->GetAbstrValuesUnit());
		}

		if (mustCalc)
		{
			AbstrUnit* res = AsUnit(resultHolder.GetNew());
			dms_assert(res);

			DataReadLock l2(a1);
			Calculate(res, a1);
		}
		return true;
	}
	virtual void Calculate(AbstrUnit* res, const AbstrDataItem* sz) const = 0;
};

template <typename D>
struct TiledUnitFromSizeOper: AbstrTiledUnitFromSizeOper
{
	typedef Unit<D>      ResultType;
	typedef DataArray<D> Arg1Type;

	TiledUnitFromSizeOper()
		:	AbstrTiledUnitFromSizeOper(ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrUnit* res, const AbstrDataItem* sz) const override
	{
		auto vu = dynamic_cast<const Unit<D>*>(sz->GetAbstrValuesUnit());
		MG_CHECK(vu);		
		Range<D> srcRange   = vu->GetCurrSegmInfo()->GetRange();
		D        srcMaxSize = const_array_cast<D>(sz)->GetDataRead()[0];

		mutable_unit_cast<D>(res)->SetRegularTileRange(srcRange, srcMaxSize);
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	template <typename TD>
	struct TiledDomainOperators
	{
		TiledUnitOper<TD>         m_TileOper;
		TiledUnitFromSizeOper<TD> m_FSTileOper;
	};

	CommonOperGroup
		cog_Range(token::range), 
		cog_CatRange(token::cat_range),
		cog_LowerBound("LowerBound"),
		cog_UpperBound("UpperBound"), 
		cog_BoundRange("BoundRange"), 
		cog_BoundCenter("BoundCenter");

	template <typename RU>
	struct UnitRangeOperators
	{
		UnitRangeOperators()
			: ur2(&cog_Range, false)
			, ur3(&cog_Range, false)
			, lb(&cog_LowerBound)
			, ub(&cog_UpperBound)
			, rb(&cog_BoundRange)
			, cb(&cog_BoundCenter)
		{}

		UnitRange2Operator  <RU> ur2;
		UnitRange3Operator  <RU> ur3;
		LowerBoundOperator <RU> lb;
		UpperBoundOperator <RU> ub;
		BoundRangeOperator <RU> rb;
		BoundCenterOperator<RU> cb;
		BaseUnitOperator   <RU> bu;
		GetMetricFactorOperator<RU> mf;
	};

	template <typename RU>
	struct UnitFixedRangeOperators
	{
		UnitFixedRangeOperators()
			: lb(&cog_LowerBound)
			, ub(&cog_UpperBound)
		{}

		LowerBoundOperator <RU> lb;
		UpperBoundOperator <RU> ub;
	};

	template <typename P>
	struct PointOperators
	{
		UnitGridsetOperator<P>         gridSet;
		GetProjectionOffsetOperator<P> getProjectionOffset;
		GetProjectionFactorOperator<P> getProjectionFactor;
		GetProjectionBaseOperator  <P> getProjectionBase;
	};

	// `nrofrows`, not `NrOfRows`: token::NrOfRows in tic/LispTreeType.cpp interns first, and the two
	// must agree or GetName() and GetNameStr() would spell this operator differently (#1161).
	CommonOperGroup cog_NrOfRows("nrofrows");
	tl_oper::inst_tuple_templ<typelists::domain_elements, NrOfRowsOperator> nrOfRowsOpers(&cog_NrOfRows);

	tl_oper::inst_tuple_templ<typelists::tiled_domain_elements, TiledDomainOperators > tiledDomainOpers;

	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, UnitRangeOperators > unitRangeOpers;
	tl_oper::inst_tuple_templ<typelists::bints, UnitFixedRangeOperators > unitFixedRangeOpers;

	tl_oper::inst_tuple_templ<typelists::domain_objects, UnitRange2Operator> unitCatRange2Opers(&cog_CatRange, true);
	tl_oper::inst_tuple_templ<typelists::domain_objects, UnitRange3Operator> unitCatRange3Opers(&cog_CatRange, true);

	CommonOperGroup cog_combine("combine", oper_policy::allow_extra_args);
	CommonOperGroup cog_combine08("combine_uint8", oper_policy::allow_extra_args);
	CommonOperGroup cog_combine16("combine_uint16", oper_policy::allow_extra_args);
	CommonOperGroup cog_combine32("combine_uint32", oper_policy::allow_extra_args);
	CommonOperGroup cog_combine64("combine_uint64", oper_policy::allow_extra_args);

	CommonOperGroup cog_combineu("combine_unit", oper_policy::allow_extra_args);
	CommonOperGroup cog_combineu08("combine_unit_uint8", oper_policy::allow_extra_args);
	CommonOperGroup cog_combineu16("combine_unit_uint16", oper_policy::allow_extra_args);
	CommonOperGroup cog_combineu32("combine_unit_uint32", oper_policy::allow_extra_args);
	CommonOperGroup cog_combineu64("combine_unit_uint64", oper_policy::allow_extra_args);

	UnitCombineOperator<UInt32> g_UnitCombine  (cog_combine  , true);
	UnitCombineOperator<UInt8 > g_UnitCombine08(cog_combine08, true);
	UnitCombineOperator<UInt16> g_UnitCombine16(cog_combine16, true);
	UnitCombineOperator<UInt32> g_UnitCombine32(cog_combine32, true);
	UnitCombineOperator<UInt64> g_UnitCombine64(cog_combine64, true);

	UnitCombineOperator<UInt32> g_UnitCombineu  (cog_combineu  , false);
	UnitCombineOperator<UInt8 > g_UnitCombineu08(cog_combineu08, false);
	UnitCombineOperator<UInt16> g_UnitCombineu16(cog_combineu16, false);
	UnitCombineOperator<UInt32> g_UnitCombineu32(cog_combineu32, false);
	UnitCombineOperator<UInt64> g_UnitCombineu64(cog_combineu64, false);

	// #1293: encoding into and decoding out of a combined unit, per element
	CommonOperGroup cog_CombineData("combine_data", oper_policy::allow_extra_args);
	tl_oper::inst_tuple_templ<typelists::domain_int_objects, CombineDataOperator> combineDataOpers(&cog_CombineData);

	oper_arg_policy oap_CombineRel[2] = { oper_arg_policy::calc_as_result, oper_arg_policy::calc_always }; // the position decides the values unit
	SpecialOperGroup sog_CombineRel("combine_rel", 2, oap_CombineRel, oper_policy::dynamic_result_class);

	CommonOperGroup cog_FactorRel[MAX_NR_COMBINE_FACTORS] = {
		{ s_FactorRelNames[ 0], oper_policy::dynamic_result_class }, { s_FactorRelNames[ 1], oper_policy::dynamic_result_class }
	,	{ s_FactorRelNames[ 2], oper_policy::dynamic_result_class }, { s_FactorRelNames[ 3], oper_policy::dynamic_result_class }
	,	{ s_FactorRelNames[ 4], oper_policy::dynamic_result_class }, { s_FactorRelNames[ 5], oper_policy::dynamic_result_class }
	,	{ s_FactorRelNames[ 6], oper_policy::dynamic_result_class }, { s_FactorRelNames[ 7], oper_policy::dynamic_result_class }
	,	{ s_FactorRelNames[ 8], oper_policy::dynamic_result_class }, { s_FactorRelNames[ 9], oper_policy::dynamic_result_class }
	,	{ s_FactorRelNames[10], oper_policy::dynamic_result_class }, { s_FactorRelNames[11], oper_policy::dynamic_result_class }
	,	{ s_FactorRelNames[12], oper_policy::dynamic_result_class }, { s_FactorRelNames[13], oper_policy::dynamic_result_class }
	,	{ s_FactorRelNames[14], oper_policy::dynamic_result_class }, { s_FactorRelNames[15], oper_policy::dynamic_result_class }
	};

	template <typename X>
	struct CombineRelOperators
	{
		CombineRelOperators()
			: m_CombineRel(&sog_CombineRel)
		{
			for (arg_index k = 0; k != MAX_NR_COMBINE_FACTORS; ++k)
				m_FactorRels[k] = std::make_unique<FactorRelOperator<X>>(&cog_FactorRel[k], k);
		}

		CombineRelOperator<X> m_CombineRel;
		std::unique_ptr<FactorRelOperator<X>> m_FactorRels[MAX_NR_COMBINE_FACTORS];
	};
	tl_oper::inst_tuple_templ<typelists::uints, CombineRelOperators> combineRelOpers;


	tl_oper::inst_tuple_templ<typelists::points, PointOperators > pointOpers;

	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, BaseUnitOperator > baseUnitOpers; // here the 2nd arg is a unit of which only the UnitClass is requested
	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, CrsUnitOperator  > crsUnitOpers;  // wraps a unit and stamps its CRS; see AbstrCrsUnitOperator
	BaseUnitObsoleteOperator  buOldOper; // actually this DOES calc_always the 2nd arg (typename string)

	SpecialOperGroup sog_DefUnit("DefaultUnit", 1, &oap_BaseUnit[0], oper_policy::dynamic_result_class); // arg0 is typename str: calc_always

	DefaultUnitOperator defOper(&sog_DefUnit);

	// give default unit
	tl_oper::inst_tuple_templ<typelists::all_unit_types, Default0UnitOperator > defaultUnitOpers; // here the 2nd arg is a unit of which only the UnitClass is requested

	oper_arg_policy oap_DataUnit = oper_arg_policy::calc_as_result;
	SpecialOperGroup sog_DomainUnit("DomainUnit", 1, &oap_DataUnit, oper_policy(oper_policy::existing | oper_policy::dynamic_result_class));
	SpecialOperGroup sog_ValuesUnit("ValuesUnit", 1, &oap_DataUnit, oper_policy(oper_policy::existing | oper_policy::dynamic_result_class));

	DomainUnitOperator  domOper(&sog_DomainUnit);
	ValuesUnitOperator  valOper(&sog_ValuesUnit);

	template<typename V>
	struct castUnitOperator: CastUnitOperatorBase
	{
		castUnitOperator() : CastUnitOperatorBase(GetUnitGroup<V>(), Unit<V>::GetStaticClass())
		{}
	};
	tl_oper::inst_tuple_templ<typelists::numerics, castUnitOperator > castUnitOpers;
}
