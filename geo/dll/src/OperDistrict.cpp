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

#include "GeoPCH.h"
#pragma hdrstop

#include "SpatialAnalyzer.h"

#include "dbg/Debug.h"
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

template <typename T>
struct DistrictOperator : public UnaryOperator
{
	typedef UInt32            district_type;
	typedef DataArray<T>             ArgType;
	typedef Unit<district_type>      ResultUnitType;
	typedef DataArray<district_type> ResultSubType;
			
	bool m_Rule8;

	DistrictOperator(AbstrOperGroup& og, bool rule8)
		:	UnaryOperator(&og, ResultUnitType::GetStaticClass(), ArgType::GetStaticClass()) 
		, m_Rule8(rule8)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 1);

		const AbstrDataItem* inputGridA = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(inputGridA);


		const AbstrUnit* domain = inputGridA->GetAbstrDomainUnit();

		AbstrUnit* resUnit = ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder);
		dms_assert(resUnit);
		resultHolder = resUnit;

		AbstrDataItem*
			resSub = 
				CreateDataItem(resUnit, s_Districts, 
					domain, resUnit
				);
		dms_assert(resSub);

		dms_assert(resSub->GetAbstrDomainUnit() == domain);

		if (mustCalc)
		{
			DataReadLock arg1Lock(inputGridA);
			const ArgType* inputGrid = debug_cast<const ArgType*>(inputGridA->GetCurrRefObj());
			dms_assert(inputGrid);

			auto resLock = CreateHeapTileArray<district_type>(inputGrid->GetTiledRangeData(), nullptr, false MG_DEBUG_ALLOCATOR_SRC("OperDistrict: resLock"));
			
			district_type nrDistricts = 0;

			auto inputVec  = inputGrid->GetDataRead();
			auto outputVec = resLock->GetDataWrite();

			IRect rect = domain->GetRangeAsIRect();
			if (!rect.empty())
			{
				dms_assert(_Left(rect) < _Right(rect));
				dms_assert(_Top(rect) < _Bottom(rect));
				dms_assert(inputVec.size() == Cardinality(rect));
				dms_assert(outputVec.size() == Cardinality(rect));

				UGrid<const ArgType::value_type> input(Size(rect), inputVec.begin());
				UGrid<ResultSubType::value_type> output(input.GetSize(), outputVec.begin());

				Districting(input, output, &nrDistricts, m_Rule8);
			}
			ResultUnitType* resultUnit = debug_cast<ResultUnitType*>(resUnit);
			dms_assert(resultUnit);
			resultUnit->SetRange(Range<UInt32>(0, nrDistricts));

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


template <typename ZoneType>
struct District4Operator : DistrictOperator<ZoneType>
{
	District4Operator() : DistrictOperator<ZoneType>(cogDistrict4, false)
	{}
};

template <typename  ZoneType>
struct District8Operator : DistrictOperator<ZoneType>
{
	District8Operator() : DistrictOperator<ZoneType>(cogDistrict8, true)
	{}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace
{
	DistrictOperator <UInt32>  distrUInt32(cogDistrict, false);
	DistrictOperator <UInt8>   distrUInt8 (cogDistrict, false);
	DistrictOperator <Bool>    distrBool  (cogDistrict, false);

	District4Operator <UInt32>  distr4UInt32;
	District4Operator <UInt8>   distr4UInt8;
	District4Operator <Bool>    distr4Bool;
	District8Operator <UInt32>  distr8UInt32;
	District8Operator <UInt8>   distr8UInt8;
	District8Operator <Bool>    distr8Bool;

	DiversityOperator<UInt32>  divUInt32;
	DiversityOperator<UInt8>   divUInt8;
}

/******************************************************************************/

