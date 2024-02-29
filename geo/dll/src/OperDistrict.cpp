// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "SpatialAnalyzer.h"

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geo/Conversions.h"
#include "geo/Point.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"

#include "Param.h"
#include "DataItemClass.h"
#include "DataArray.h"
#include "Unit.h"
#include "UnitClass.h"

// *****************************************************************************
//											DistrictOperator
// *****************************************************************************

static TokenID s_Districts = GetTokenID_st("Districts");

template <typename T, typename R = UInt32>
struct DistrictOperator : public UnaryOperator
{
	using district_type = R;
	using ArgType = DataArray<T>;
	using ResultUnitType = Unit<district_type>;
	using ResultSubType = DataArray<district_type>;
			
	bool m_Use8Neighbours;

	DistrictOperator(AbstrOperGroup& og, bool use8Neighbours)
		:	UnaryOperator(&og, ResultUnitType::GetStaticClass(), ArgType::GetStaticClass()) 
		, m_Use8Neighbours(use8Neighbours)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 1);

		const AbstrDataItem* inputGridA = debug_cast<const AbstrDataItem*>(args[0]);
		assert(inputGridA);

		const AbstrUnit* domain = inputGridA->GetAbstrDomainUnit();

		auto resUnit = ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder);
		resUnit->SetTSF(TSF_Categorical);

		assert(resUnit);
		resultHolder = resUnit;

		AbstrDataItem*
			resSub = 
				CreateDataItem(resUnit, s_Districts, 
					domain, resUnit
				);
		assert(resSub);

		assert(resSub->GetAbstrDomainUnit() == domain);

		if (mustCalc)
		{
			DataReadLock arg1Lock(inputGridA);
			const ArgType* inputGrid = debug_cast<const ArgType*>(inputGridA->GetCurrRefObj());
			assert(inputGrid);

			auto resLock = CreateHeapTileArrayV<district_type>(inputGrid->GetTiledRangeData(), nullptr, false MG_DEBUG_ALLOCATOR_SRC("OperDistrict: resLock"));
			
			district_type nrDistricts = 0;

			auto inputVec  = inputGrid->GetDataRead();
			auto outputVec = resLock->GetDataWrite();

			IRect rect = domain->GetRangeAsIRect();
			if (!rect.empty())
			{
				assert(Left(rect) < Right(rect));
				assert(Top(rect) < Bottom(rect));
				assert(inputVec.size() == Cardinality(rect));
				assert(outputVec.size() == Cardinality(rect));

				UGrid<const ArgType::value_type> input(Size(rect), inputVec.begin());
				UGrid<ResultSubType::value_type> output(input.GetSize(), outputVec.begin());

				Districting(input, output, &nrDistricts, this->m_Use8Neighbours);
			}
			ResultUnitType* resultUnit = debug_cast<ResultUnitType*>(resUnit);
			dms_assert(resultUnit);
			resultUnit->SetRange(Unit<R>::range_t(0, nrDistricts));

			resLock->InitValueRangeData( resultUnit->m_RangeDataPtr );
			resSub->m_DataObject = resLock.release();
		}
		return true;
	}
};

// *****************************************************************************
//											DiversityOperator
// *****************************************************************************

CommonOperGroup cogDiversity("diversity");

//REMOVE TODO AbstrOperator

template <typename T>
class DiversityOperator : public TernaryOperator
{
	typedef DataArray<T>      Arg1Type; // inputGrid (SPoint)
	typedef DataArray<UInt16> Arg2Type; // radius    (void)
	typedef DataArray<UInt16> Arg3Type; // isCircle  (void)
	typedef Unit<T>           ResultUnitType;
	typedef DataArray<T>      ResultType;
			
public:
	DiversityOperator()
		:	TernaryOperator(&cogDiversity, 
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

		const AbstrDataItem* inputGridA= debug_cast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* radiusA   = debug_cast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* isCircleA = debug_cast<const AbstrDataItem*>(args[2]);

		const AbstrUnit* domain = inputGridA->GetAbstrDomainUnit();
		const ResultUnitType* values = debug_cast<const ResultUnitType*>(inputGridA->GetAbstrValuesUnit());
		dms_assert(domain);
		dms_assert(values);

		checked_domain<Void>(radiusA, "a1");
		checked_domain<Void>(isCircleA, "a2");

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain, values, COMPOSITION(T));

		if (mustCalc)
		{
			const Arg1Type* inputGrid = debug_cast<const Arg1Type*>(inputGridA->GetCurrRefObj());
			dms_assert(inputGrid);

			DataReadLock arg1Lock(inputGridA);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResultType* result= mutable_array_cast<T>(resLock);

			auto inputVec  = inputGrid->GetLockedDataRead();
			auto outputVec = result   ->GetLockedDataWrite();
		
			Range<T> range = values->GetRange();
			dms_assert(range.first < range.second);
			T inputUpperBound = Cardinality(range);

			IRect rect = domain->GetRangeAsIRect();
			if (!rect.empty())
			{
				dms_assert(rect.first.first < rect.second.first);
				dms_assert(rect.first.second < rect.second.second);
				dms_assert(inputVec.size() == Cardinality(rect));
				dms_assert(outputVec.size() == Cardinality(rect));

				UGrid<const Arg1Type::value_type> input(Size(rect), inputVec.begin());
				UGrid<      Arg1Type::value_type> output(input.GetSize(), outputVec.begin());

				Diversity(
					input,
					inputUpperBound,
					GetCurrValue<Arg2Type::value_type>(radiusA, 0),
					GetCurrValue<Arg3Type::value_type>(isCircleA, 0),
					output
				);
			}
			resLock.Commit();
		}
		return true;
	}
};

CommonOperGroup cogDistrict("district");
CommonOperGroup cogDistrict4("district_4");
CommonOperGroup cogDistrict8("district_8");

CommonOperGroup cogDistrict_uint8("district_uint8");
CommonOperGroup cogDistrict4_uint8("district_4_uint8");
CommonOperGroup cogDistrict8_uint8("district_8_uint8");

CommonOperGroup cogDistrict_uint16("district_uint16");
CommonOperGroup cogDistrict4_uint16("district_4_uint16");
CommonOperGroup cogDistrict8_uint16("district_8_uint16");

CommonOperGroup cogDistrict_uint32("district_uint32");
CommonOperGroup cogDistrict4_uint32("district_4_uint32");
CommonOperGroup cogDistrict8_uint32("district_8_uint32");

CommonOperGroup cogDistrict_uint64("district_uint64");
CommonOperGroup cogDistrict4_uint64("district_4_uint64");
CommonOperGroup cogDistrict8_uint64("district_8_uint64");


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace
{
	template <typename R>
	struct DistrictOperators
	{
		DistrictOperators(AbstrOperGroup& og, bool use8Neighbours)
		: distrUInt32(og, use8Neighbours)
		, distrUInt16(og, use8Neighbours)
		, distrUInt8 (og, use8Neighbours)
		, distrBool  (og, use8Neighbours)
		{
			og.SetBetterNotInMetaScripting();
		}

		DistrictOperator <UInt32, R>  distrUInt32;
		DistrictOperator <UInt16, R>  distrUInt16;
		DistrictOperator <UInt8 , R>  distrUInt8;
		DistrictOperator <Bool  , R>  distrBool;
	};


	DistrictOperators <UInt32>  distr(cogDistrict, false), distr4(cogDistrict4, false), distr8(cogDistrict8, true);
	DistrictOperators <UInt8 >  distr_UInt8 (cogDistrict_uint8 , false), distr4_UInt8 (cogDistrict4_uint8 , false), distr8_uint8 (cogDistrict8_uint8 , true);
	DistrictOperators <UInt16>  distr_UInt16(cogDistrict_uint16, false), distr4_UInt16(cogDistrict4_uint16, false), distr8_uint16(cogDistrict8_uint16, true);
	DistrictOperators <UInt32>  distr_UInt32(cogDistrict_uint32, false), distr4_UInt32(cogDistrict4_uint32, false), distr8_uint32(cogDistrict8_uint32, true);
	DistrictOperators <UInt64>  distr_UInt64(cogDistrict_uint64, false), distr4_UInt64(cogDistrict4_uint64, false), distr8_uint64(cogDistrict8_uint64, true);

	DiversityOperator<UInt32>  divUInt32;
	DiversityOperator<UInt8>   divUInt8;
}

/******************************************************************************/

