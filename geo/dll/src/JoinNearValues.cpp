// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// join_near_values operator: relational join of two attributes on nearly
// equal values.

#include "vt/RangeIndex.h"
#include "utl/StrFormat.h"

#include "DataItemClass.h"
#include "UnitClass.h"

#include "vt/CheckedCalc.h"
#include "geom/SpatialIndex.h"
#include "geom/SpatialSearchBox.h"
#include "mem/MyContainers.h"

#include "IndexAssigner.h"
#include "ParallelTiles.h"
#include "TileChannel.h" // ordered_row_sink (#1327)

// *****************************************************************************
//                         IndexedSearchOperator
// *****************************************************************************

#include "Unit.h"
#include "UnitProcessor.h"
#include "RtcTypeLists.h"
#include "DataArrayValue.h"

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

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		assert(args.size() == 3);

		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]);               assert(arg1A);
		const AbstrUnit* arg1_DomainUnit = arg1A->GetAbstrDomainUnit(); assert(arg1_DomainUnit);
		const AbstrDataItem* sqrDistParameter = AsDataItem(args[2]);    assert(sqrDistParameter);

		MG_CHECK(AsDynamicDataItem(GetItem(args[1])));

		arg1A->GetAbstrValuesUnit()->UnifyValues(AsDataItem(args[1])->GetAbstrValuesUnit(), "v1", "v2", UnifyMode(UM_Throw));

		if (!sqrDistParameter->HasVoidDomainGuarantee())
			sqrDistParameter->throwItemError("Should have a void domain");

		auto AB = static_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder.GetNew());

		AbstrDataItem* resSubA = CreateDataItem(AB.get(), GetTokenID_mt("first_rel"), AB.get(), AsDataItem(args[0])->GetAbstrDomainUnit()).get(); // owned by AB
		AbstrDataItem* resSubB = CreateDataItem(AB.get(), GetTokenID_mt("second_rel"), AB.get(), AsDataItem(args[1])->GetAbstrDomainUnit()).get(); // owned by AB

		resultHolder = AB;
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

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context = nullptr) const override
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
		auto dist = GetTheCurrValue<DistType>(distRef);
		auto sqrDist = CheckedMul<DistType>(dist, dist, false);
		AbstrUnit* AB = AsUnit(resultHolder.GetNew());

		SpatialIndexType spIndex(bxRefData.begin(), bxRefData.end(), 0);

		// the row numbers in UInt32 when both domains allow it, so that relations to domains of UInt32 take their tiles as they are
		if (nr_A < MAX_VALUE(UInt32) && nr_B < MAX_VALUE(UInt32))
			Join<UInt32>(AB, args, axRef, spIndex, bxRefData.begin(), dist, sqrDist);
		else
			Join<SizeT>(AB, args, axRef, spIndex, bxRefData.begin(), dist, sqrDist);
		return true;
	}

private:
	using CoordType = scalar_of_t<ArgValuesElement>;
	using SpatialIndexType = SpatialIndex<CoordType, const ArgValuesElement*>;

	// #1327: the pairs go in one pass, in the order of A, into chains of tiles that first_rel and second_rel adopt once AB
	// has its count. Each tile of A is cut into items of ordered_item_size points, so that a worker holds the pairs of one
	// item at a time; before, every tile kept its pairs until all tiles were done, and they were then copied into the result.
	template <typename I>
	void Join(AbstrUnit* AB, const ArgRefs& args, const AbstrDataItem* axRef, const SpatialIndexType& spIndex, const ArgValuesElement* bxBegin, DistType dist, DistType sqrDist) const
	{
		const AbstrUnit* A = axRef->GetAbstrDomainUnit();
		const AbstrUnit* B = AsDataItem(args[1])->GetAbstrDomainUnit();
		tile_id atn = A->GetNrTiles();
		auto itemBases = ordered_item_bases(atn, [A](tile_id t) { return A->GetTileSize(t); });

		ordered_row_sink<I, I> sink(itemBases[atn], MAX_VALUE(ResultElement));
		parallel_tileloop(atn, [&](tile_id at)
		{
			auto producer = sink.Producer();
			auto axRefData = const_array_cast<ArgValuesElement>(axRef)->GetTile(at);
			SizeT tileFirstIndex = A->GetTileFirstIndex(at);
			SizeT tileSize = axRefData.size();

			my_vec_t<I> firstRels, secondRels, candidates;
			SizeT k = itemBases[at];
			for (SizeT itemBegin = 0; itemBegin < tileSize; itemBegin += ordered_item_size, ++k)
			{
				SizeT itemEnd = std::min<SizeT>(itemBegin + ordered_item_size, tileSize);
				firstRels.clear();
				secondRels.clear();
				for (SizeT i = itemBegin; i != itemEnd; ++i)
				{
					const ArgValuesElement& ap = axRefData[i];
					if (!IsDefined(ap))
						continue;
					// Inflated outward by at least one unit: the index tests a point leaf half-open, so a point at exactly ap + dist on an
					// axis was never visited, and ArgValuesElement(dist, dist) truncated 1.5 to 1 for integer points (GEO-A12, as #1228 for connect).
					// The exact test below keeps only the points within dist.
					auto searchBox = InflatedSearchBox<CoordType>(Range<ArgValuesElement>(ap, ap), dist);
					candidates.clear();
					for (auto bxIter = spIndex.begin(searchBox); bxIter; ++bxIter)
					{
						const ArgValuesElement* bxPointPtr = (*bxIter)->get_ptr();
						if (SqrDist<DistType>(*bxPointPtr, ap) <= sqrDist)
							candidates.emplace_back(I(bxPointPtr - bxBegin));
					}
					std::sort(candidates.begin(), candidates.end());
					for (auto second_rel : candidates)
					{
						firstRels.emplace_back(I(tileFirstIndex + i));
						secondRels.emplace_back(second_rel);
					}
				}
				sink.Deliver(k, producer, firstRels.size(), firstRels.data(), secondRels.data());
			}
			assert(k == itemBases[at + 1]);
		});

		AB->SetCount(sink.Finish());

		AbstrDataItem* resSubA = CreateDataItem(AB, GetTokenID_mt("first_rel"), AB, A).get(); // owned by AB
		AbstrDataItem* resSubB = CreateDataItem(AB, GetTokenID_mt("second_rel"), AB, B).get(); // owned by AB

		DataWriteLock resSubALock(resSubA);
		auto firstTiles = sink.template Column<0>().TakeTiles();
		A->InviteUnitProcessor(IndexTileAssigner<I>(resSubALock.get(), firstTiles));
		resSubALock.Commit();

		DataWriteLock resSubBLock(resSubB);
		auto secondTiles = sink.template Column<1>().TakeTiles();
		B->InviteUnitProcessor(IndexTileAssigner<I>(resSubBLock.get(), secondTiles));
		resSubBLock.Commit();
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

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
		tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinNearValuesOperator, UInt32, ph::_1, SqrDistType>> jevOpers;
		tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinNearValuesOperator, UInt8 , ph::_1, SqrDistType>> jevOpers_u8;
		tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinNearValuesOperator, UInt16, ph::_1, SqrDistType>> jevOpers_u16;
		tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinNearValuesOperator, UInt32, ph::_1, SqrDistType>> jevOpers_u32;
		tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinNearValuesOperator, UInt64, ph::_1, SqrDistType>> jevOpers_u64;

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
