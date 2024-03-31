// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "OperUnit.h"

#include "RtcGeneratedVersion.h"
#include "geo/GeoSequence.h"
#include "mci/CompositeCast.h"
#include "mci/ValueWrap.h"
#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "ser/StringStream.h"
#include "utl/mySPrintF.h"

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
#include "TiledUnit.h"


// *****************************************************************************

const UnitClass* GetUnitClass(TokenID valueTypeID)
{
	const ValueClass* vc = ValueClass::FindByScriptName(valueTypeID);
	if (!vc)
		throwErrorF("UnitClass", 
			"'%s' is not recognized as a valid type indicator",
			GetTokenStr(valueTypeID)
		);
	const UnitClass* uc = UnitClass::Find(vc);
	if (!uc)
		throwErrorF("UnitClass", 
			"'%s' is not a valid type for defining a unit",
			GetTokenStr(valueTypeID)
		);
	return uc;
}

// *****************************************************************************
// DefaultUnitOperator
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

bool UnitCombine_impl(AbstrUnit* res, const ArgSeqType& args, bool mustCalc, bool mustCreateBackRefs)
{
	arg_index n = args.size();
	dms_assert(res);
	for (arg_index i = 0; i!=n; ++i)
		if (!IsUnit(args[i]))
		{
			auto clsName = SharedStr(args[i]->GetDynamicClass()->GetName());
			throwErrorF("combine_unit", "ranged units expected, but argument %d is a %s", i + 1, clsName.c_str());
		}

	if (n > 16)
		throwErrorD("combine_unit", "the number of arguments exceeds the maximum of 16."
			"\nConsider splitting up the arguments into separate sub-combines and check that the product of the number of values doesn't exceed the possible range of the resulting unit."
		);

	SizeT productSize = 1;
	if (mustCalc)
	{
		// determine cardinality for result unit
		arg_index i = n;
		while ((i > 0) && (productSize != 0))
		{
			SharedPtr<const AbstrUnit> ithUnit = AsCertainUnit(args[--i]);  // i gets decremented here
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

	// assign all combinations of nr_OrgEntity in lexicographical order
	arg_index i = n;
	SizeT groupSize = 1;
	static TokenID subItemNameID[] = {
		GetTokenID_mt("first_rel"), GetTokenID_mt("second_rel"), GetTokenID_mt("third_rel"), GetTokenID_mt("fourth_rel")
	,	GetTokenID_mt("fifth_rel"), GetTokenID_mt("sixth_rel"), GetTokenID_mt("seventh_rel"), GetTokenID_mt("eighth_rel")
	,	GetTokenID_mt("ninth_rel"), GetTokenID_mt("tenth_rel"), GetTokenID_mt("eleventh_rel"), GetTokenID_mt("twelveth_rel")
	,	GetTokenID_mt("thirteenth_rel"), GetTokenID_mt("fourteenth_rel"), GetTokenID_mt("fifteenth_rel"), GetTokenID_mt("sixteenth_rel")
	};
	for (; i; --i)
	{
		SharedPtr<const AbstrUnit> ithUnit = AsCertainUnit(args[i - 1]);
		AbstrDataItem* resSub = CreateDataItem(res, subItemNameID[i-1], res, ithUnit);
		resSub->SetTSF(TSF_Categorical);

		if constexpr (DMS_VERSION_MAJOR < 15)
		{
			if (!mustCalc)
			{
				auto depreciatedRes = CreateDataItem(res, GetTokenID_mt(myArrayPrintF<10>("nr_%d", i)), res, AsCertainUnit(args[i - 1]));
				depreciatedRes->SetTSF(TSF_Categorical);
				depreciatedRes->SetTSF(TSF_Depreciated);
				depreciatedRes->SetReferredItem(resSub);
				continue; // go to next sub
			}
		}

		SizeT unitCount = ithUnit->GetCount();

		SizeT cycleSize = groupSize * unitCount;
		auto trd = res->GetTiledRangeData();
		visit<typelists::domain_elements>(ithUnit.get(),
			[resSub, trd, groupSize, cycleSize, unitCount] <typename V> (const Unit<V>* valuesUnit)
			{
				auto conv = CountableValueConverter<V>(valuesUnit->m_RangeDataPtr);
				auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(resSub, trd, valuesUnit->m_RangeDataPtr
					, [trd, groupSize, cycleSize, unitCount, conv](AbstrDataObject* self, tile_id t) {
						tile_offset  tileSize = trd->GetTileSize(t);
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
					MG_DEBUG_ALLOCATOR_SRC("res->md_FullName +  := combine()")
				);
				resSub->m_DataObject = lazyTileFunctor.release();
			}
		);
		groupSize = cycleSize;
	}
//	dms_assert(groupSize == productSize);

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
			resultHolder = ResultType::GetStaticClass()->CreateResultUnit(resultHolder);
		}
		auto resUnit = AsUnit(resultHolder.GetNew());
		assert(resUnit);
		resUnit->SetTSF(TSF_Categorical);

		return UnitCombine_impl(resUnit, args, mustCalc, m_MustCreateBackRefs);
	}
};

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

	// TODO G8: Niet Tmp, want dat roept SetMaxRange aan, roep CreateResultUnit aan en copy range en intersect als onderdeel van DuplFrom met mustCalc conform Range(srcUnit, lb, ub);
	// TODO G8: Of schedule geen calc phase voor dit soort operator die in meta-info tijd een compleet resultaat geven (dus zonder range output) 
	if (!resultHolder)
	{
		assert(!mustCalc);
		resultHolder = m_ResultUnitClass->CreateTmpUnit(resultHolder);
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
			arg1 = AsDataItem(arg1->GetReferredItem());
		} while (arg1);

		resultHolder = arg1Unit;
		dms_assert(!mustCalc || AsUnit(arg1Unit->GetCurrRangeItem())->HasTiledRangeData());
		return true; // REMOVE (!mustCalc) || arg1Unit->PrepareData();
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
			arg1 = AsDataItem(arg1->GetCurrRefItem());
		} while (arg1);

		resultHolder = arg1Unit;
		dms_assert(!mustCalc || AsUnit(arg1Unit->GetCurrRangeItem())->HasTiledRangeData());
		return true; // REMOVE (!mustCalc) || arg1Unit->PrepareData();
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
		AbstrUnit* result = arg2->GetUnitClass()->CreateTmpUnit(resultHolder);
		resultHolder = result;

		if (baseUnitName.empty())
			result->SetMetric(nullptr);
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
// BaseUnitOperator
// *****************************************************************************

// REMOVE OBSOLETE VERSION AND THEN oper_arg_policy::calc_always for the fourth argument

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
			AbstrUnit* result = AsUnit(args[3])->GetUnitClass()->CreateResultUnit(resultHolder);
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
		auto newP = std::make_unique<UnitProjection>((orgP ? orgP->GetBaseUnit() : AsUnit(arg1->GetCurrUltimateItem())), trRel.Offset(), trRel.Factor());
		result->SetProjection(newP.release());

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
bool CreateRangeUnit(TreeItemDualRef& resultHolder, const AbstrUnit* arg1, const TreeItem* lbItem, const TreeItem* ubItem, bool isCategorical, bool mustCalc)
{
	if (!resultHolder)
	{
		checked_domain<Void>(lbItem, "LowerBound");
		checked_domain<Void>(ubItem, "UpperBound");

		assert(!mustCalc);
		auto result = mutable_unit_cast<T>(Unit<T>::GetStaticClass()->CreateResultUnit(resultHolder));
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
					bounds &= arg1Range;
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
		return CreateRangeUnit<T>(resultHolder, nullptr, args[0], args[1], m_IsCatRangeFunc, mustCalc);
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
		return CreateRangeUnit<T>(resultHolder, arg1, args[1], args[2], m_IsCatRangeFunc, mustCalc);
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

		result->GetDataWrite()[0] = e1->GetCurrMetric()->m_Factor;
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
		result->GetDataWrite()[0] = UnitProjection::GetCompositeTransform(p).Factor();
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
		result->GetDataWrite()[0] = UnitProjection::GetCompositeTransform(p).Offset();
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
	typedef Unit<E1>             Arg1Type;
	typedef DataArray<UInt32>    ResultType;

public:
	// Override Operator
	NrOfRowsOperator(AbstrOperGroup* gr)
		:	AbstrUnitPropOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrUnit* e1) const override
	{
		ResultType *result = composite_cast<ResultType*>(res);
		dms_assert(result);

		result->GetDataWrite()[0] = e1->GetCount();
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
		dms_assert(args.size() == 1);

		const AbstrUnit *e1 = AsUnit(args[0]);
		dms_assert(e1);

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

		ResultType *result = composite_cast<ResultType*>(res);
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
		result->GetDataWrite()[0] = e1->GetRange().first;
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

		ResultType *result = composite_cast<ResultType*>(res);
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
		result->GetDataWrite()[0] = e1->GetRange().second;
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

		ResultType *result = composite_cast<ResultType*>(res);
		dms_assert(result);

		result->GetDataWrite()[0] = Size( e1->GetRange() );
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

		ResultType *result = composite_cast<ResultType*>(res);
		dms_assert(result);

		result->GetDataWrite()[0] = Center( e1->GetRange() );
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
			AbstrUnit* result = debug_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder);
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
			segmInfo.emplace_back(*lbi++, *ubi++);
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
			AbstrUnit* result = debug_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder);
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

	CommonOperGroup cog_NrOfRows("NrOfRows");
	tl_oper::inst_tuple_templ<typelists::domain_elements, NrOfRowsOperator, AbstrOperGroup* > nrOfRowsOpers(&cog_NrOfRows);

	tl_oper::inst_tuple_templ<typelists::tiled_domain_elements, TiledDomainOperators > tiledDomainOpers;

	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, UnitRangeOperators > unitRangeOpers;
	tl_oper::inst_tuple_templ<typelists::bints, UnitFixedRangeOperators > unitFixedRangeOpers;

	tl_oper::inst_tuple_templ<typelists::domain_objects, UnitRange2Operator, AbstrOperGroup*, bool> unitCatRange2Opers(&cog_CatRange, true);
	tl_oper::inst_tuple_templ<typelists::domain_objects, UnitRange3Operator, AbstrOperGroup*, bool> unitCatRange3Opers(&cog_CatRange, true);

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


	tl_oper::inst_tuple<typelists::points, PointOperators<_> > pointOpers;

	tl_oper::inst_tuple<typelists::ranged_unit_objects, BaseUnitOperator<_> > baseUnitOpers; // here the 2nd arg is a unit of which only the UnitClass is requested
	BaseUnitObsoleteOperator  buOldOper; // actually this DOES calc_always the 2nd arg (typename string)

	SpecialOperGroup sog_DefUnit("DefaultUnit", 1, &oap_BaseUnit[0], oper_policy::dynamic_result_class); // arg0 is typename str: calc_always

	DefaultUnitOperator defOper(&sog_DefUnit);

	// give default unit
	tl_oper::inst_tuple<typelists::all_unit_types, Default0UnitOperator<_> > defaultUnitOpers; // here the 2nd arg is a unit of which only the UnitClass is requested

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
	tl_oper::inst_tuple<typelists::numerics, castUnitOperator<_> > castUnitOpers;

}
