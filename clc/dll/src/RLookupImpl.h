// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// Shared implementation for the rlookup / rlookup_with_null / classify operators.
// The heavy per-value-type instantiations are split across RLookup*.cpp (one value-
// type group per translation unit) to keep each .obj well under the /MP straggler
// threshold; all of them register into the single inline operator groups below.

#pragma once
#if !defined(__CLC_RLOOKUPIMPL_H)
#define __CLC_RLOOKUPIMPL_H

#include "ser/AsString.h"
#include "set/VectorFunc.h"
#include "geo/StringBounds.h"
#include "geo/GeoSequence.h"

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "Metric.h"
#include "ParallelTiles.h"
#include "TileFunctorImpl.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "stg/AbstrStorageManager.h"

#include "OperSignature.h"

#include "lookup.h"

#include "rlookup.h"

class AbstrIndexedSearchOperator : public BinaryOperator
{
	typedef AbstrDataItem      ResultType;

public:
	AbstrIndexedSearchOperator(AbstrOperGroup* gr, const Class* argClass)
		:	BinaryOperator(gr, ResultType::GetStaticClass(), argClass, argClass)
	{}

	// mirrors CreateResult below: rlookup(a: V1[D]; b: V2[E]) -> E[D] — the
	// result's VALUES unit IS b's domain (K4; one variable E in b's domain role
	// and the result's values role — reduction-honest: the result is flagged
	// categorical, so a declared-values conflict also fails CheckResultItem's
	// UnifyDomain discharge). E carries no member class (dynamic result class);
	// its class flows through the unit node's companion when E binds. The
	// values-compatibility of a and b stays class-level via the member tuples
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		auto argCls = dynamic_cast<const DataItemClass*>(GetArgClass(0));
		if (!argCls)
			return false;
		ValueComposition vc = argCls->GetValuesType()->GetValueComposition();
		sig_var D = sb.UnitVar("D"), E = sb.UnitVar("E"), V1 = sb.UnitVar("V1"), V2 = sb.UnitVar("V2");
		sb.MemberValueClass(V1, argCls->GetValuesType());
		sb.MemberValueClass(V2, argCls->GetValuesType());
		sb.ArgAttr(0, V1, D, vc);
		sb.ArgAttr(1, V2, E, vc);
		sb.ResultAttr(E, D, ValueComposition::Single);
		return true;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A= AsDataItem(args[0]);
		const AbstrDataItem* arg2A= AsDataItem(args[1]);
		assert(arg1A);
		assert(arg2A);

		const AbstrUnit* arg1_DomainUnit = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* arg2_DomainUnit = arg2A->GetAbstrDomainUnit();
		assert(arg1_DomainUnit);
		assert(arg2_DomainUnit);


		if (!resultHolder)
		{
			compatible_values_unit_creator_func(0, GetGroup(), args, true);
			resultHolder = CreateCacheDataItem(arg1_DomainUnit, arg2_DomainUnit);
			resultHolder->SetTSF(TSF_Categorical);
		}

		resultHolder->m_StatusFlags.SetHasSortedValues(arg1A->m_StatusFlags.HasSortedValues() && arg2A->m_StatusFlags.HasSortedValues());

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

			tile_id nrTiles = arg1_DomainUnit->GetNrTiles();

			bool hasIndex = !arg2A->m_StatusFlags.HasSortedValues() || arg2A->HasUndefinedValues();
			auto index = MakeIndex(hasIndex, arg2A, arg2_DomainUnit);
			const AbstrUnit* arg2Domain = arg2A->GetAbstrDomainUnit();
			auto arg2DomainRange = arg2Lock->GetTiledRangeData();


			if (IsMultiThreaded3() && (nrTiles > 1) && !IsInMMD(res) && (nrTiles > arg2DomainRange->GetNrTiles()))
				AsDataItem(resultHolder.GetOld())->m_DataObject = CreateFutureTileIndexer(make_shared_tree(res, existing_obj{}), res->GetLazyCalculatedState(), arg2_DomainUnit, arg1A, arg2Domain, arg2DomainRange.get(), hasIndex, std::move(index) MG_DEBUG_ALLOCATOR_SRC(res->md_FullName + " := RLookup()"));
			else
			{
				DataWriteLock resLock(res);
				parallel_tileloop(nrTiles, [this, &resLock, arg1A, arg2Domain, hasIndex, &index](tile_id t)->void
					{
						this->Calculate(resLock.get(), arg1A, arg2Domain, hasIndex, index, t);
					}
				);
				resLock.Commit();
			}
		}
		return true;
	}
	virtual std::any MakeIndex(bool mustMakeIndex, const AbstrDataItem* arg2A, const AbstrUnit* arg2_DomainUnit) const = 0;
	virtual auto CreateFutureTileIndexer(std::shared_ptr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* arg2Domain, const AbstrTileRangeData* arg2DomainRange, bool hasIndex, std::any index MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)) const->SharedPtr<const AbstrDataObject> = 0;
	virtual void Calculate(AbstrDataObject* resObj, const AbstrDataItem* arg1A, const AbstrUnit* arg2Domain, bool hasIndex, const std::any&, tile_id t) const =0;
};


// Helper to determine if domain uses 32-bit or 64-bit indexing
// Size > 4 means Int64/UInt64 scalars or IPoint/UPoint (which have 8-byte cardinality)
inline bool Uses64BitIndex(const AbstrUnit* domainUnit)
{
	return domainUnit->GetValueType()->GetSize() > 4;
}

template< typename V>
void MakeIndexForAbstrDomainSkipNull(const AbstrDataItem* arg2A, const AbstrUnit* arg2DomainA, std::any& index)
{
	auto values = const_array_cast<V>(arg2A)->GetDataRead();
	if (Uses64BitIndex(arg2DomainA))
		index = make_index_array_skip_null<UInt64, V>(std::move(values));
	else
		index = make_index_array_skip_null<UInt32, V>(std::move(values));
}

template< typename V>
void MakeIndexForAbstrDomainAllValues(const AbstrDataItem* arg2A, const AbstrUnit* arg2DomainA, std::any& index)
{
	auto values = const_array_cast<V>(arg2A)->GetDataRead();
	if (Uses64BitIndex(arg2DomainA))
		index = make_index_array_all_values<UInt64, V>(std::move(values));
	else
		index = make_index_array_all_values<UInt32, V>(std::move(values));
}


template <class V, class IndexApplicator, bool SkipNull>
class SearchIndexOperatorImpl : AbstrIndexedSearchOperator
{
public:
	SearchIndexOperatorImpl(AbstrOperGroup* og)
		: AbstrIndexedSearchOperator(og, DataArray<V>::GetStaticClass())
	{}

	std::any MakeIndex(bool mustMakeIndex, const AbstrDataItem* arg2A, const AbstrUnit* arg2DomainA) const override
	{
		std::any result;
		if (!mustMakeIndex)
		{
			assert(arg2A->m_StatusFlags.HasSortedValues() && !arg2A->HasUndefinedValues());
			result = const_array_cast<V>(arg2A)->GetDataRead();
		}
		else
		{
			if constexpr (SkipNull)
				MakeIndexForAbstrDomainSkipNull<V>(arg2A, arg2DomainA, result);
			else
				MakeIndexForAbstrDomainAllValues<V>(arg2A, arg2DomainA, result);
		}
		return result;
	}

	template <typename E>
	static auto CreateTileData(typename sequence_traits<E>::seq_t resData, typename sequence_traits<V>::cseq_t arg1Data, bool hasIndex, const std::any* indexBoxPtr, typename Unit<E>::range_t arg2DomainRange)
	{
		using index_type = index_type_t<E>;
		using index_tile = indexed_tile_t<index_type, V>;
		using res_seq_t = sequence_traits<E>::seq_t;


		static_assert(!std::is_same_v<E, WPoint> || std::is_same_v<index_type, UInt32>);
		static_assert(std::is_same_v<index_type, typename index_tile::first_type::value_type>);

		if (hasIndex)
		{
			auto indexPtr = std::any_cast<index_tile>(indexBoxPtr);
			assert(indexPtr);
			CalcTileWithIndex<E>(resData, arg1Data, indexPtr, arg2DomainRange);
		}
		else
		{
			auto keyValuesPtr = std::any_cast<typename DataArray<V>::locked_cseq_t>(indexBoxPtr);
			assert(keyValuesPtr);
			CalcTileWithKeyValues< E>(resData, arg1Data, keyValuesPtr, arg2DomainRange);
		}
	}

	auto CreateFutureTileIndexer(std::shared_ptr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* arg2DomainA, const AbstrTileRangeData* arg2DomainRange, bool hasIndex, std::any indexBox MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)) const -> SharedPtr<const AbstrDataObject> override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
//		auto valuesUnit = debug_cast<const Unit<field_of_t<ResultValueType>>*>(valuesUnitA);

		auto arg1 = MakeSharedFromBorrowedObjectPtr(const_array_cast<V>(arg1A));
		assert(arg1);
		std::shared_ptr<std::any> indexBoxPtr = std::make_shared<std::any>(std::move(indexBox));

		using prepare_data = std::shared_ptr<typename TileFunctor<V>::future_tile>;
		std::unique_ptr<AbstrDataObject> futureTileFunctor;

		auto prepareTileDataFunc = [arg1](tile_id t) { return arg1->GetFutureTile(t); }; // only depends on V

		visit<typelists::domain_objects>(arg2DomainA
		,	[&futureTileFunctor, &prepareTileDataFunc, resultAdi, lazy, arg2DomainRange, arg1, hasIndex, indexBoxPtr, tileRangeData MG_DEBUG_ALLOCATOR_SRC_PARAM]<typename E>(const Unit<E>*arg2Domain)
		{
			futureTileFunctor = make_unique_FutureTileFunctor<E, prepare_data, false>(resultAdi, lazy, tileRangeData.get(), get_range_ptr_of_valuesunit(arg2Domain)
				, std::move(prepareTileDataFunc)  // only depends on V
				, [arg2DomainRange = dynamic_cast<const typename Unit<E>::range_data_t*>(arg2DomainRange)->GetRange(), hasIndex, indexBoxPtr](typename sequence_traits<E>::seq_t resData, prepare_data arg1FutureData) // depends on E and V
				{
					auto arg1Data = arg1FutureData->GetTile(); // only depends on V
					CreateTileData<E>(resData, arg1Data, hasIndex, indexBoxPtr.get(), arg2DomainRange); // allready called
				}
				MG_DEBUG_ALLOCATOR_SRC_PARAM
			);
		});

		return futureTileFunctor.release();
	}

	template <typename E>
	static void CalcTileWithIndex(typename sequence_traits<E>::seq_t resData, typename sequence_traits<V>::cseq_t arg1Data, const indexed_tile_t<index_type_t<E>, V>* indexPtr, typename Unit<E>::range_t arg2DomainRange)
	{
		IndexApplicator applicator;
		applicator.template applyIndexedSearch<typename sequence_traits<E>::seq_t, typename sequence_traits<V>::cseq_t>(resData, std::move(arg1Data), indexPtr->second.get_view(), arg2DomainRange, indexPtr->first);

		assert(resData.size() == arg1Data.size());
	}

	template <typename E>
	static void CalcTileWithKeyValues(typename sequence_traits<E>::seq_t resData, typename sequence_traits<V>::cseq_t arg1Data, const typename DataArray<V>::locked_cseq_t* keyValuesPtr, typename Unit<E>::range_t arg2DomainRange)
	{
		IndexApplicator applicator;
		applicator.template applyBinarySearch<typename sequence_traits<E>::seq_t, typename sequence_traits<V>::cseq_t>(resData, std::move(arg1Data), keyValuesPtr->get_view(), arg2DomainRange);
		assert(resData.size() == arg1Data.size());
	}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* arg1A, const AbstrUnit* arg2DomainA, bool hasIndex, const std::any& indexBox, tile_id t) const override
	{
		visit<typelists::domain_objects>(arg2DomainA,
			[resObj, arg1Data = const_array_cast<V>(arg1A)->GetTile(t), hasIndex, &indexBox, t]<typename E>(const Unit<E>*arg2Domain)
			{
				auto resData = mutable_array_cast<E>(resObj)->GetWritableTile(t);

				using index_type = index_type_t<E>;
				using index_tile = indexed_tile_t<index_type, V>;
				using res_seq_t = sequence_traits<E>::seq_t;

				CreateTileData<E>(resData.get_view(), arg1Data.get_view(), hasIndex, &indexBox, arg2Domain->GetRange());
			}
		);
	}
};

// ---- shared operator groups (single instance across all RLookup*.cpp) ----
inline CommonOperGroup cog_rlookup  ("rlookup", oper_policy::dynamic_result_class);
inline CommonOperGroup cog_rlookupWN("rlookup_with_null", oper_policy::dynamic_result_class);
inline CommonOperGroup cog_classify ("classify", oper_policy::dynamic_result_class);

template <class V>
struct RLookupOperator
{
	SearchIndexOperatorImpl<V, rlookup_dispatcher, true> rlookup;
	RLookupOperator() : rlookup(&cog_rlookup) {}
};

template <class V>
struct RLookupWithNullOperator
{
	SearchIndexOperatorImpl<V, rlookup_with_null_dispatcher, false> rlookupWN;
	RLookupWithNullOperator() : rlookupWN(&cog_rlookupWN) {}
};

template <typename V>
struct ClassifyOperator : SearchIndexOperatorImpl<V, classify_dispatcher, true>
{
	ClassifyOperator() : SearchIndexOperatorImpl<V, classify_dispatcher, true>(&cog_classify) {}
};

#endif // __CLC_RLOOKUPIMPL_H
