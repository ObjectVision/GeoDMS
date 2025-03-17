// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "geo/RangeIndex.h"
#include "utl/mySPrintF.h"

#include "DataItemClass.h"
#include "UnitClass.h"

#include "geo/CheckedCalc.h"
#include "geo/SpatialIndex.h"

#include "ParallelTiles.h"

// *****************************************************************************
//                         IndexedSearchOperator
// *****************************************************************************

#include "Unit.h"
#include "UnitProcessor.h"
#include "RtcTypeLists.h"

// *****************************************************************************
// join_near_values: (A->P, B->P, D2): AB { ->A, ->B }
// *****************************************************************************

class AbstrJoinNearValuesOperator : public TernaryOperator
{
public:
	AbstrJoinNearValuesOperator(AbstrOperGroup& gr, const UnitClass* resultUnitClass, const DataItemClass* argClass, const DataItemClass* sqrDistClass)
		: TernaryOperator(&gr, resultUnitClass, argClass, argClass, sqrDistClass)
	{
	}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		assert(args.size() == 2);

		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]);               assert(arg1A);
		const AbstrUnit* arg1_DomainUnit = arg1A->GetAbstrDomainUnit(); assert(arg1_DomainUnit);
		const AbstrDataItem* sqrDistParameter = AsDataItem(args[2]);    assert(sqrDistParameter);

		MG_CHECK(AsDynamicDataItem(GetItem(args[1])));

		arg1A->GetAbstrValuesUnit()->UnifyValues(AsDataItem(args[1])->GetAbstrValuesUnit(), "v1", "v2", UnifyMode(UM_Throw));

		if (!sqrDistParameter->HasVoidDomainGuarantee())
			sqrDistParameter->throwItemError("Should have a void domain");

		auto AB = static_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder);
		resultHolder = AB;

		AbstrDataItem* resSubA = CreateDataItem(AB, GetTokenID_mt("first_rel"), AB, AsDataItem(args[0])->GetAbstrDomainUnit());
		AbstrDataItem* resSubB = CreateDataItem(AB, GetTokenID_mt("second_rel"), AB, AsDataItem(args[1])->GetAbstrDomainUnit());

		//		AbstrDataItem* resSubX = CreateDataItem(AB, GetTokenID_mt("X_rel"), AB, AsDataItem(args[0])->GetAbstrValuesUnit());
	}
};

template <typename ResultElement, typename ArgValuesElement, typename DistType>
struct JoinNearValuesOperator : AbstrJoinNearValuesOperator
{
	JoinNearValuesOperator(AbstrOperGroup& gr)
		: AbstrJoinNearValuesOperator(gr, Unit<ResultElement>::GetStaticClass(), DataArray<ArgValuesElement>::GetStaticClass(), DataArray<DistType>::GetStaticClass())
	{
	}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext*, Explain::Context* context = nullptr) const override
	{
		const AbstrDataItem* axRef = AsDataItem(args[0]);
		const AbstrDataItem* bxRef = AsDataItem(args[1]);
		const AbstrDataItem* distRef = AsDataItem(args[2]);

		DataReadLock axRefLock(axRef);
		DataReadLock bxRefLock(bxRef); auto bxRefData = const_array_cast<ArgValuesElement>(bxRef)->GetLockedDataRead();
		DataReadLock distRefLock(distRef);

		const AbstrUnit* A = axRef->GetAbstrDomainUnit();
		const AbstrUnit* B = bxRef->GetAbstrDomainUnit();
		const Unit<ArgValuesElement>* X = debug_cast<const Unit<ArgValuesElement>*>(axRef->GetAbstrValuesUnit());
		MG_CHECK(A->IsOrdinalAndZeroBased());
		MG_CHECK(B->IsOrdinalAndZeroBased());
		auto nr_A = A->GetCount();
		auto nr_B = B->GetCount();
		//		auto nr_X = X->GetCount();
		auto dist = GetTheValue<DistType>(distRef);
		auto sqrDist = CheckedMul<DistType>(dist, dist, false);
		auto distVect = ArgValuesElement(dist, dist);
		AbstrUnit* AB = AsUnit(resultHolder.GetNew());
		const AbstrUnit* axDomain = axRef->GetAbstrDomainUnit();

		using CoordType = scalar_of_t<ArgValuesElement>;
		using SpatialIndexType = SpatialIndex<CoordType, const ArgValuesElement*>;
		SpatialIndexType spIndex(bxRefData.begin(), bxRefData.end(), 0);

		using tile_results_type = std::vector<std::pair<SizeT, SizeT > >;

		auto atn = A->GetNrTiles();
		std::vector<tile_results_type> resultArrays(atn);
		parallel_tileloop(atn, [&resultArrays, axDomain, axRef, &spIndex, &bxRefData, distVect, sqrDist](tile_id at)
		{
			auto axRefData = const_array_cast<ArgValuesElement>(axRef)->GetTile(at);
			auto tileFirstIndex = axDomain->GetTileFirstIndex(at);
			tile_results_type tileResults;
			std::vector<SizeT> second_rels;
			for (const auto& ap : axRefData)
			{
				if (!IsDefined(ap))
					continue;
				SizeT aRow = (&ap - axRefData.begin()) + tileFirstIndex;
				auto searchBox = Inflate(ap, distVect);
				second_rels.clear();
				for (auto bxIter = spIndex.begin(searchBox); bxIter; ++bxIter)
				{
					const ArgValuesElement* bxPointPtr = (*bxIter)->get_ptr();
					if (SqrDist<DistType>(*bxPointPtr, ap) <= sqrDist)
					{
						auto second_rel = bxPointPtr - bxRefData.begin();
						second_rels.emplace_back(second_rel);
					}
				}
				std::sort(second_rels.begin(), second_rels.end());
				for (auto second_rel : second_rels)
					tileResults.emplace_back(aRow, second_rel);
			}
			resultArrays[at] = std::move(tileResults);
		});
		SizeT n = 0;
		for (auto& resultArray : resultArrays)
			n += resultArray.size();
		tile_results_type results; results.reserve(n);
		for (auto& resultArray : resultArrays)
		{
			results.insert(results.end(), resultArray.begin(), resultArray.end());
			resultArray = tile_results_type();
		}

		AB->SetCount(results.size());

		AbstrDataItem* resSubA = CreateDataItem(AB, GetTokenID_mt("first_rel"), AB, AsDataItem(args[0])->GetAbstrDomainUnit());
		AbstrDataItem* resSubB = CreateDataItem(AB, GetTokenID_mt("second_rel"), AB, AsDataItem(args[1])->GetAbstrDomainUnit());

		DataWriteLock resSubALock(resSubA);

		visit<typelists::domain_elements>(A, 
			[&] <typename a_type> (const Unit<a_type>* unitA) 
			{
				auto aRange = unitA->GetRange();
				auto subAData = mutable_array_cast<a_type>(resSubALock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
				for (SizeT i = 0, n = results.size(); i != n; ++i)
					subAData[i] = Range_GetValue_naked(aRange, results[i].first);
			}
		);
		resSubALock.Commit();

		DataWriteLock resSubBLock(resSubB);

		visit<typelists::domain_elements>(B, 
			[&] <typename b_type> (const Unit<b_type>* unitB) 
			{
				auto bRange = unitB->GetRange();
				auto subBData = mutable_array_cast<b_type>(resSubBLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
				for (SizeT i = 0, n = results.size(); i != n; ++i)
					subBData[i] = Range_GetValue_naked(bRange, results[i].second);
			}
		);
		resSubBLock.Commit();

		return true;
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	static const oper_policy op = oper_policy::better_not_in_meta_scripting; // op_allow_extra_args;
	CommonOperGroup cog_jev("join_near_values", op);
	CommonOperGroup cog_jev_u8 ("join_near_values_uint8", op);
	CommonOperGroup cog_jev_u16("join_near_values_uint16", op);
	CommonOperGroup cog_jev_u32("join_near_values_uint32", op);
	CommonOperGroup cog_jev_u64("join_near_values_uint64", op);

	using domains = typelists::points; // domain_elements;

	template <typename SqrDistType>
	struct JNVOpers {
		tl_oper::inst_tuple<domains, JoinNearValuesOperator<UInt32, _, SqrDistType>, AbstrOperGroup&> jevOpers;
		tl_oper::inst_tuple<domains, JoinNearValuesOperator<UInt8 , _, SqrDistType>, AbstrOperGroup&> jevOpers_u8;
		tl_oper::inst_tuple<domains, JoinNearValuesOperator<UInt16, _, SqrDistType>, AbstrOperGroup&> jevOpers_u16;
		tl_oper::inst_tuple<domains, JoinNearValuesOperator<UInt32, _, SqrDistType>, AbstrOperGroup&> jevOpers_u32;
		tl_oper::inst_tuple<domains, JoinNearValuesOperator<UInt64, _, SqrDistType>, AbstrOperGroup&> jevOpers_u64;

		JNVOpers() 
		:	jevOpers(cog_jev)
		,	jevOpers_u8(cog_jev_u8)
		,	jevOpers_u16(cog_jev_u16)
		,	jevOpers_u32(cog_jev_u32)
		,	jevOpers_u64(cog_jev_u64)
		{}
	};

	JNVOpers<Float64> jevF64;
//	JNVOpers<Float32> jevF32;
//	JNVOpers<UInt64> jevu64;
//	JNVOpers<UInt32> jevu32;
//	JNVOpers<UInt16> jevu16;
} // end anonymous namespace
