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

#include "geo/SpatialIndex.h"

//#include "pcount.h"

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

template <typename ResultElement, typename ArgValuesElement, typename SqrDistType>
struct JoinNearValuesOperator : AbstrJoinNearValuesOperator
{
	JoinNearValuesOperator(AbstrOperGroup& gr)
		: AbstrJoinNearValuesOperator(gr, Unit<ResultElement>::GetStaticClass(), DataArray<ArgValuesElement>::GetStaticClass(), DataArray<SqrDistType>::GetStaticClass())
	{
	}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext*, Explain::Context* context = nullptr) const override
	{
		const AbstrDataItem* axRef = AsDataItem(args[0]);
		const AbstrDataItem* bxRef = AsDataItem(args[1]);

		DataReadLock axRefLock(axRef);
		DataReadLock bxRefLock(bxRef); auto bxRefData = const_array_cast<ArgValuesElement>(bxRef)->GetLockedDataRead();

		const AbstrUnit* A = axRef->GetAbstrDomainUnit();
		const AbstrUnit* B = bxRef->GetAbstrDomainUnit();
		const Unit<ArgValuesElement>* X = debug_cast<const Unit<ArgValuesElement>*>(axRef->GetAbstrValuesUnit());
		MG_CHECK(A->IsOrdinalAndZeroBased());
		MG_CHECK(B->IsOrdinalAndZeroBased());
		MG_CHECK(X->IsOrdinalAndZeroBased());
		auto nr_A = A->GetCount();
		auto nr_B = B->GetCount();
		//		auto nr_X = X->GetCount();

		AbstrUnit* AB = AsUnit(resultHolder.GetNew());

		SpatialIndex<ArgValuesElement, const ArgValuesElement*> spIndex(bxRefData);

		neighbour_iter<SpatialIndexType> iter(&spIndex);

		std::vector<std::pair<SizeT, SizeT > > results;

		for (tile_id at = 0, atn = A->GetNrTiles(); at != atn; ++at)
		{
			auto axRefData = const_array_cast<ArgValuesElement>(axRef)->GetTileData(at);
			auto tileOffset = axxDomain->GetTiledData()->GetTileOffset(at);
			for (const auto& ap : axRefData)
			{
				if (!IsDefined(ap))
					continue;
				iter.Reset(ap);
				SizeT aRow = (&ap - axRefData.begiin() + tileOffset;
				for (auto i = spIndex.begin(ap.Expand())); i; ++i)
				{
					if (dist2(*i, ap) <= sqrDist)
					{
						results.emplace_back(aRow, * i - bxRefData);
					}
				}
			}
		}

//		auto xRange = X->GetRange();
		ResultElement nr_AB = 0;
		for (auto aCountPtr = aCounts.begin(), bCountPtr = bCounts.begin(), aCountEnd = aCounts.end(); aCountPtr != aCountEnd; ++aCountPtr, ++bCountPtr)
		{
			ResultElement aCount = ThrowingConvertNonNull<ResultElement>(*aCountPtr);
			ResultElement bCount = ThrowingConvertNonNull<ResultElement>(*bCountPtr);
			ResultElement abCount = aCount * bCount;
			// SafeMul
			MG_USERCHECK2(!bCount || abCount / bCount == aCount,
				"join_equal_values operator: the product of the cardinalities of a common value exceeds the maximum value of the resulting unit");

			abCounts.emplace_back(abCount);
			abOffsets.emplace_back(nr_AB);
			ResultElement old_nr_AB = nr_AB;
			nr_AB += abCount;
			MG_USERCHECK2(nr_AB >= old_nr_AB,
				"join_equal_values operator: the cumulation of the cardinalities of common values exceeds the maximum value of the resulting unit");
		}
		AB->SetCount(nr_AB);

		AbstrDataItem* resSubA = CreateDataItem(AB, GetTokenID_mt("first_rel"), AB, AsDataItem(args[0])->GetAbstrDomainUnit());
		AbstrDataItem* resSubB = CreateDataItem(AB, GetTokenID_mt("second_rel"), AB, AsDataItem(args[1])->GetAbstrDomainUnit());
		AbstrDataItem* resSubX = CreateDataItem(AB, GetTokenID_mt("X_rel"), AB, AsDataItem(args[0])->GetAbstrValuesUnit());

		DataWriteLock resSubALock(resSubA);

		visit<typelists::domain_elements>(A, 
			[&] <typename a_type> (const Unit<a_type>* unitA) 
			{
				auto aRange = unitA->GetRange();
				auto subAData = mutable_array_cast<a_type>(resSubALock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
				for (SizeT ab_index = 0, aIndex = 0, aSize = axRefData.size(); aIndex != aSize; ++aIndex)
				{
					ArgValuesElement x = axRefData[aIndex];
					if (IsIncluding(xRange, x))
					{
						SizeT x_index = Range_GetIndex_naked(xRange, x);
						assert(x_index < nr_X);
						SizeT b_count = bCounts[x_index];

						SizeT resIndex = abOffsets[x_index] + aUsed[x_index]++ * b_count;

						while (b_count)
						{
							assert(aIndex < nr_A);
							subAData[resIndex + --b_count] = Range_GetValue_naked(aRange, aIndex);
						}
					}
				}
			}
		);
		resSubALock.Commit();

		DataWriteLock resSubBLock(resSubB);

		visit<typelists::domain_elements>(B, 
			[&] <typename b_type> (const Unit<b_type>* unitB) 
			{
				auto bRange = unitB->GetRange();
				auto subBData = mutable_array_cast<b_type>(resSubBLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
				for (SizeT ab_index = 0, bIndex = 0, bSize = bxRefData.size(); bIndex != bSize; ++bIndex)
				{
					ArgValuesElement x = bxRefData[bIndex];
					if (IsIncluding(xRange, x))
					{
						SizeT x_index = Range_GetIndex_naked(xRange, x);
						dms_assert(x_index < nr_X);
						SizeT a_count = aCounts[x_index];
						SizeT b_count = bCounts[x_index];

						SizeT resIndex = abOffsets[x_index]; resIndex += bUsed[x_index]++;

						while (a_count--)
						{
							dms_assert(bIndex < nr_B);
							subBData[resIndex] = Range_GetValue_naked(bRange, bIndex);
							resIndex += b_count;
						}
					}
				}
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
