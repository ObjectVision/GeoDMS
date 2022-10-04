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

#include "ClcPCH.h"
#pragma hdrstop

#include "OperUnit.h"

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
		checked_domain<Void>(args[0]);
		SharedStr typeName = GetValue<Arg1Type::value_type>(args[0], 0);
		resultHolder = GetUnitClass(GetTokenID_mt(typeName.c_str()))->CreateDefault();
	}
	return true;
}

bool UnitCombine_impl(AbstrUnit* res, const ArgSeqType& args, bool mustCalc, bool mustCreateBackRefs)
{
	arg_index n = args.size();
	dms_assert(res);

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
	while (i)
	{
		TokenID combineRefToken = GetTokenID_mt(myArrayPrintF<10>("nr_%d", i));
		AbstrDataItem* resSub = CreateDataItem(
			res
			//			,	GetTokenID((SharedStr("nr_") + argUnits[i]->GetName()).c_str())
			, combineRefToken
			, res
			, AsCertainUnit(args[--i]) // i gets decremented here
		);

		if (!mustCalc)
			continue; // go to next sub

		SharedPtr<const AbstrUnit> ithUnit = AsCertainUnit(args[i]);
		SizeT unitBase = ithUnit->GetBase();
		SizeT unitCount = ithUnit->GetCount();
		SizeT unitUB = unitCount + unitBase;

		DataWriteLock resSubLock(resSub);

		visit<typelists::domain_elements>(resSub->GetAbstrValuesUnit(), 
			[resSubObj = resSubLock.get(), unitBase, unitUB, productSize, groupSize] <typename value_type> (const Unit<value_type>* valuesUnit)
			{
				auto resData = mutable_array_cast<value_type>(resSubObj)->GetDataWrite();
				auto conv = CountableValueConverter<value_type>(valuesUnit->m_RangeDataPtr);

				SizeT iOrg = unitBase;
				SizeT iGroup = 0;

				for (SizeT r = 0; r != productSize; ++r)
				{
					Assign(resData[r], conv.GetValue(iOrg));

					if (++iGroup == groupSize)
					{
						iGroup = 0;
						if (++iOrg == unitUB)
							iOrg = unitBase;
					}
				}
				dms_assert(iOrg == unitBase);
				dms_assert(!iGroup);
			}
		);

		groupSize = groupSize * unitCount;

		resSubLock.Commit();
	}
	dms_assert(groupSize == productSize);

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
			dms_assert(!mustCalc);
			resultHolder = ResultType::GetStaticClass()->CreateResultUnit(resultHolder);
		}
		auto resUnit = AsUnit(resultHolder.GetNew());
		dms_assert(resUnit);
		return UnitCombine_impl(resUnit, args, mustCalc, m_MustCreateBackRefs);
	}
};

// *****************************************************************************
// CastUnitOperator
// *****************************************************************************

CastUnitOperator::CastUnitOperator(AbstrOperGroup* gr, const UnitClass* resultUnitClass)
	:	UnaryOperator(gr, resultUnitClass,  Arg1Type::GetStaticClass())
	,	m_ResultUnitClass(resultUnitClass)
{
	dms_assert(m_ResultUnitClass == GetUnitClass(gr->GetNameID()));
}

// Override Operator
bool CastUnitOperator::CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc)  const
{
	dms_assert(args.size() == 1);

	const Arg1Type* arg1 = debug_cast<const Arg1Type*>(args[0]);
	dms_assert(arg1);

	// TODO G8: Niet Tmp, want dat roept SetMaxRange aan, roep CreateResultUnit aan en copy range en intersect als onderdeel van DuplFrom met mustCalc conform Range(srcUnit, lb, ub);
	// TODO G8: Of schedule geen calc phase voor dit soort operator die in meta-info tijd een compleet resultaat geven (dus zonder range output) 
	if (!resultHolder)
	{
		dms_assert(!mustCalc);
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

		checked_domain<Void>(args[0]);

		SharedStr baseUnitName = GetCurrValue<Arg1Type::value_type>(args[0], 0);
		AbstrUnit* result = arg2->GetUnitClass()->CreateTmpUnit(resultHolder);
		resultHolder = result;

		SharedPtr<UnitMetric> m;
		if (!baseUnitName.empty())
		{
			m = new UnitMetric;
			m->m_BaseUnits[baseUnitName] = 1;
		}
		result->SetMetric(m);
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
		checked_domain<Void>(args[1]);
		checked_domain<Void>(args[2]);

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
		SharedPtr<UnitProjection> newP = new UnitProjection((orgP ? orgP->GetBaseUnit() : AsUnit(arg1->GetCurrUltimateItem())), trRel.Offset(), trRel.Factor());

		result->SetProjection(newP);

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


template <class T> // = spoint, ipoint, fpoint, dpoint, float32/64, (u)int32/16/8
class UnitRangeOperator : public TernaryOperator
{
	typedef Unit<T>              ResultType;
	typedef AbstrUnit            Arg1Type;
	typedef DataArray<T>         Arg2Type; // LowerBound 
	typedef DataArray<T>         Arg3Type; // UpperBound

	static_assert(has_var_range_field_v<T>);

public:
	UnitRangeOperator(AbstrOperGroup* gr)
		:	TernaryOperator(gr,
				ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), 
				Arg2Type::GetStaticClass(),
				Arg3Type::GetStaticClass()
			)
	{}


	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrUnit* arg1 = debug_cast<const AbstrUnit*>(args[0]);

		dms_assert(arg1);
		checked_domain<Void>(args[1]);
		checked_domain<Void>(args[2]);

		if (!resultHolder)
		{
			dms_assert(!mustCalc);
			ResultType* result = mutable_unit_cast<T>(Unit<T>::GetStaticClass()->CreateResultUnit(resultHolder));
			dms_assert(result);
			resultHolder = result;
			result->DuplFrom(arg1);
		}

		if (mustCalc)
		{
			ResultType* result = debug_cast<Unit<T>*>(resultHolder.GetNew());
			dms_assert(result);

			T lb = GetTheCurrValue<T>(args[1]);
			T ub = GetTheCurrValue<T>(args[2]);

			MG_USERCHECK2(IsDefined(lb), "Error in range-operator: lowerBound (arg2) is UNDEFINED");
			MG_USERCHECK2(IsDefined(ub), "Error in range-operator: upperBound (arg3) is UNDEFINED");

			Range<T> bounds(lb, ub);
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
			result->SetRange(bounds);
		}
		return true;
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
		dms_assert(e1);

		ResultType *result = composite_cast<ResultType*>(res);
		dms_assert(result);

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
		a1->GetAbstrDomainUnit()->UnifyDomain(a2->GetAbstrDomainUnit(), UM_Throw);
		a1->GetAbstrValuesUnit()->UnifyDomain(a2->GetAbstrValuesUnit(), UM_Throw);

		if (!resultHolder)
		{
			dms_assert(!mustCalc);
			AbstrUnit* result = debug_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder);
			dms_assert(result);
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
		dms_assert(args.size() == 1);

		const AbstrDataItem* a1 = AsDataItem(args[0]);
		checked_domain<Void>(a1);

		if (!resultHolder)
		{
			dms_assert(!mustCalc);
			AbstrUnit* result = debug_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder);
			dms_assert(result);
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
		cog_LowerBound("LowerBound"), 
		cog_UpperBound("UpperBound"), 
		cog_BoundRange("BoundRange"), 
		cog_BoundCenter("BoundCenter");

	template <typename RU>
	struct UnitRangeOperators
	{
		UnitRangeOperators()
			: ur(&cog_Range)
			, lb(&cog_LowerBound)
			, ub(&cog_UpperBound)
			, rb(&cog_BoundRange)
			, cb(&cog_BoundCenter)
		{}

		UnitRangeOperator  <RU> ur;
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
	tl_oper::inst_tuple<typelists::domain_elements, NrOfRowsOperator<_>, AbstrOperGroup* > nrOfRowsOpers(&cog_NrOfRows);

	tl_oper::inst_tuple<typelists::tiled_domain_elements, TiledDomainOperators<_> > tiledDomainOpers;

	tl_oper::inst_tuple<typelists::ranged_unit_objects, UnitRangeOperators<_> > unitRangeOpers;
	tl_oper::inst_tuple<typelists::bints, UnitFixedRangeOperators<_> > unitFixedRangeOpers;

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
}
