// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/RangeIndex.h"
#include "mci/ValueClass.h"

#include "OperGroups.h"
#include "TreeItemClass.h"
#include "Unit.h"

#include "UnitCreators.h"

// *****************************************************************************
//											PerimeterOperator
// *****************************************************************************

CommonOperGroup cogPerimeter("perimeter", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);

template <typename T>
class PerimeterOperator : public UnaryOperator
{
	typedef DataArray<T> ArgType;
			
public:
	PerimeterOperator():
		UnaryOperator(&cogPerimeter, AbstrDataItem::GetStaticClass(), ArgType::GetStaticClass()) {}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 1);

		const AbstrDataItem* inputGridA = AsDataItem(args[0]);
		assert(inputGridA);

		const AbstrUnit* domain = inputGridA->GetAbstrDomainUnit();
		MG_CHECK(domain->GetValueType()->GetNrDims() == 2);

		const AbstrUnit* regionsUnit = inputGridA->GetAbstrValuesUnit();

		const AbstrUnit* resUnit = count_unit_creator(args);
		assert(resUnit);
		if (!resultHolder)
			resultHolder = CreateCacheDataItem(regionsUnit, resUnit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(inputGridA);
			const ArgType* inputGrid = const_array_cast<T>(arg1Lock.get_ptr());
			assert(inputGrid);

			AbstrDataItem* result = AsDataItem(resultHolder.GetNew());

			DataWriteLock resLock(result);
			
			auto inputVec   = inputGrid->GetDataRead();

			IRect rect = domain->GetRangeAsIRect();
			assert(Left(rect) < Right (rect));
			assert(Top (rect) < Bottom(rect));
			assert(inputVec .size() == Cardinality(rect));


			// TODO: move to UniProcessor for resUnit to accomodate large (IPoint, UPoint) domains
			typedef UInt32 count_type;
			typedef Unit     <count_type> ResultUnitType;
			typedef DataArray<count_type> ResultType;

			const ResultUnitType* resultUnit = debug_cast<const ResultUnitType*>(resUnit);
			ResultUnitType::range_t resultRange = resultUnit->GetRange();
			auto districtRange = inputGrid->GetValueRangeData()->GetRange();

			auto outputVec = mutable_array_checkedcast<count_type>(resLock)->GetDataWrite();
			SizeT pos = 0, outputVecSize = outputVec.size();
			dms_assert(outputVecSize == Cardinality(districtRange));
			Int32 e = Width(rect), pe = e-1;
			for (Int32 j = 0, f = Height(rect), pf = f-1; j != f; ++j)
				for (Int32 i = 0; i != e; ++pos, ++i)
				{
					T v = inputVec[pos];
					SizeT vi = Range_GetIndex_naked(districtRange, v);
					if (vi >= outputVecSize)
						continue;
					UInt32 c = 0;
					if (i == 0  || inputVec[pos-1] != v) ++c;
					if (i == pe || inputVec[pos+1] != v) ++c;
					if (j == 0  || inputVec[pos-e] != v) ++c;
					if (j == pf || inputVec[pos+e] != v) ++c;
					if (c)
						outputVec[vi] += c;
				}

			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//							Weighted PerimeterOperator
// *****************************************************************************

CommonOperGroup cogWeightedPerimeter("perimeter_weighted");

template <typename T, typename R>
class WeightedPerimeterOperator : public QuinaryOperator
{
	typedef DataArray<T> ArgType;
	typedef DataArray<R> WeightType;
			
public:
	WeightedPerimeterOperator():
		QuinaryOperator(&cogWeightedPerimeter
		, WeightType::GetStaticClass()
		, ArgType::GetStaticClass()
		, WeightType::GetStaticClass()
		, WeightType::GetStaticClass()
		, WeightType::GetStaticClass()
		, WeightType::GetStaticClass()
		) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 5);

		const AbstrDataItem* inputGridA = AsDataItem(args[0]);
		dms_assert(inputGridA);

		const AbstrUnit* domain = inputGridA->GetAbstrDomainUnit();
		MG_CHECK(domain->GetValueType()->GetNrDims() == 2);
		const AbstrUnit* regionsUnit = inputGridA->GetAbstrValuesUnit();

		const AbstrDataItem* northFactor= AsDataItem(args[1]);
		dms_assert(northFactor);
		domain->UnifyDomain(northFactor->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
		const AbstrUnit* resUnit = northFactor->GetAbstrValuesUnit();

		domain->UnifyDomain(AsDataItem(args[2])->GetAbstrDomainUnit(), "e1", "e3", UM_Throw);
		domain->UnifyDomain(AsDataItem(args[3])->GetAbstrDomainUnit(), "e1", "e4", UM_Throw);
		domain->UnifyDomain(AsDataItem(args[4])->GetAbstrDomainUnit(), "e1", "e5", UM_Throw);

		resUnit->UnifyValues(AsDataItem(args[2])->GetAbstrValuesUnit(), "v2", "v3", UnifyMode(UM_Throw|UM_AllowDefault));
		resUnit->UnifyValues(AsDataItem(args[3])->GetAbstrValuesUnit(), "v2", "v4", UnifyMode(UM_Throw|UM_AllowDefault));
		resUnit->UnifyValues(AsDataItem(args[4])->GetAbstrValuesUnit(), "v2", "v5", UnifyMode(UM_Throw|UM_AllowDefault));

		dms_assert(resUnit);
		if (!resultHolder) resultHolder = CreateCacheDataItem(regionsUnit, resUnit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(inputGridA), arg2Lock(AsDataItem(args[1])), arg3Lock(AsDataItem(args[2])), arg4Lock(AsDataItem(args[3])), arg5Lock(AsDataItem(args[4]));

			const ArgType* inputGrid = const_array_cast<T>(inputGridA);
			dms_assert(inputGrid);

			AbstrDataItem* result = AsDataItem(resultHolder.GetNew());

			DataWriteLock resLock(result);
			
			auto inputVec   = inputGrid->GetDataRead();
			auto
				northVec = const_array_cast<R>(args[1])->GetDataRead(), 
				eastVec  = const_array_cast<R>(args[2])->GetDataRead(), 
				southVec = const_array_cast<R>(args[3])->GetDataRead(), 
				westVec  = const_array_cast<R>(args[4])->GetDataRead();

			IRect rect = domain->GetRangeAsIRect();
			dms_assert(Left(rect) < Right (rect));
			dms_assert(Top (rect) < Bottom(rect));
			dms_assert(inputVec .size() == Cardinality(rect));

			const Unit<R>* resultUnit = debug_cast<const Unit<R>*>(resUnit);
			auto  resultRange = resultUnit->GetRange();
			auto  districtRange = inputGrid->GetValueRangeData()->GetRange();

			auto outputVec = mutable_array_checkedcast<R>(resLock)->GetDataWrite();
			SizeT pos = 0, outputVecSize = outputVec.size();
			dms_assert(outputVecSize == Cardinality(districtRange));
			Int32 e = Width(rect), pe = e-1;
			for (Int32 j = 0, f = Height(rect), pf = f-1; j != f; ++j)
				for (Int32 i = 0; i != e; ++pos, ++i)
				{
					T v = inputVec[pos];
					SizeT vi = Range_GetIndex_naked(districtRange, v);
					if (vi >= outputVecSize)
						continue;
					R c = 0;
					if (i == 0  || inputVec[pos-1] != v) c += westVec[pos];
					if (i == pe || inputVec[pos+1] != v) c += eastVec[pos];
					if (j == 0  || inputVec[pos-e] != v) c += northVec[pos];
					if (j == pf || inputVec[pos+e] != v) c += southVec[pos];
					if (c)
						outputVec[vi] += c;
				}

			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

template <typename T>
struct PerimeterOperators : PerimeterOperator<T>
{
	tl_oper::inst_tuple<typelists::floats, WeightedPerimeterOperator<T, _> > weightedPerimeterOpers;
};

namespace
{
	tl_oper::inst_tuple_templ<typelists::domain_ints, PerimeterOperators> perimeterOpers;
}

/******************************************************************************/

