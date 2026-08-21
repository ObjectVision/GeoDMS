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
#include "vt/StringBounds.h"
#include "vt/GeoSequence.h"

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

#include "Explain.h"
#include "Lookup.h"

#include "rlookup.h"

class AbstrIndexedSearchOperator : public BinaryOperator
{
	typedef AbstrDataItem      ResultType;

public:
	// isExactMatchSearch: this member looks its argument's value up by equality (rlookup,
	// rlookup_with_null), so #612 can state that a requested value does not occur at all. classify
	// searches an interval, for which that statement would be wrong; it passes false.
	// skipsNullKeys: a null in the first argument never matches (plain rlookup). Then a null result at
	// a null key needs no explaining -- the page already shows that key as null.
	AbstrIndexedSearchOperator(AbstrOperGroup* gr, const Class* argClass, bool isExactMatchSearch, bool skipsNullKeys)
		:	BinaryOperator(gr, ResultType::GetStaticClass(), argClass, argClass)
		,	m_SkipsNullKeys(skipsNullKeys)
	{
		if (isExactMatchSearch)
			gr->SetCanExplainValue();
	}

	// Overridden because SetCanExplainValue() makes the Operator base refuse the default caller;
	// the creation itself is unchanged.
	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;

		auto argSeq = GetItems(args);
		MG_CHECK(CreateResult(resultHolder, argSeq, false));
		assert(resultHolder);
	}

	// Idem, plus the #612 explanation: this runs a second time, with a context, when a value-info page
	// explains one element of an already calculated result.
	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		assert(resultHolder);
		assert(args.size() == 2);

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
		assert(res);

		if (!res->m_DataObject)
		{
			auto argSeq = GetItems(args);
			if (!CreateResult(resultHolder, argSeq, true))
				return false;
		}
		if (context)
			ExplainResultElement(AsDataItem(args[0]), AsDataItem(args[1]), res, context);
		return true;
	}

	// The one thing a value-info page cannot already say about a reverse lookup. It shows the key that
	// was searched for (arg1 shares the result's domain, so the generic mechanism explains it at the
	// same row) and, for a key that WAS found, the matching row of arg2 (the result value is a row of
	// arg2's domain, which the generic mechanism follows). Neither of those covers a key that simply
	// does not occur in arg2: that is what is stated here.
	void ExplainResultElement(const AbstrDataItem* keysA, const AbstrDataItem* valuesA, const AbstrDataItem* res, Explain::Context* context) const
	{
		assert(keysA && valuesA && res);
		assert(context && context->m_Coordinate);

		SizeT i = context->m_Coordinate->first;

		// Read the result through m_DataObject rather than a DataReadLock: we are inside this very
		// item's calculation, and locking it here joins its own still-active OperationContext.
		// The explanation pass only runs on an already calculated result, so the object is there.
		const AbstrDataObject* resObj = res->m_DataObject.get();
		if (!resObj || !(i < resObj->GetTiledRangeData()->GetRangeSize()))
			return;
		if (!resObj->IsNull(i))
			return; // found: the matching row of arg2 already shows up as a supplier

		if (m_SkipsNullKeys)
		{
			DataReadLock keysLock(keysA);
			const AbstrDataObject* keysObj = keysLock.get_ptr();
			if (!keysObj || !(i < keysObj->GetTiledRangeData()->GetRangeSize()))
				return;
			if (keysObj->IsNull(i))
				return; // a null key cannot match here, and the page already shows the key as null
		}

		// Usually anonymous: a configured attribute is substituted by its definition, so what arrives
		// here is a cache item. The page prints the expression right next to this note, which names it.
		auto subjectName = SharedStr(valuesA->GetFullName());
		Explain::SetValueReason(context, subjectName.empty()
			? SharedStr("no row of the searched attribute has this value")
			: mySSPrintF("no row of {} has this value", subjectName));
	}

	// A reverse search builds an INDEX over its second argument (the values being searched), one
	// entry per value -- `indexed_tile_t = std::pair<std::vector<I>, locked_cseq_t>` with
	// `index_type_t` = UInt32 or UInt64 (rlookup.h). That cost is proportional to the VALUES
	// attribute and completely independent of the result, which is what made this the worst
	// mis-estimate measured on t405: an rlookup producing 171 elements (684 B predicted) built an
	// index over 23,007,329 values and held 184 MB -- 269,091x its prediction, and 184 MB /
	// 23,007,329 = exactly 8 B/element, confirming UInt64 entries (§8.1.16).
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = BinaryOperator::EstimatePerformance(resultHolder, args);

		if (args.size() >= 2)
			if (auto valuesItem = GetItem(args[1]); valuesItem && IsDataItem(valuesItem))
				if (auto valuesDomain = AsDataItem(valuesItem)->GetAbstrDomainUnit())
					try {
						auto valuesCount = valuesDomain->EstimateCount().expected;
						// Index entry width follows the values domain's cardinality type, exactly as
						// index_type_t does: <= 4 bytes of cardinality -> UInt32, else UInt64.
						auto entrySize = (valuesCount <= MAX_VALUE(UInt32)) ? sizeof(UInt32) : sizeof(UInt64);
						result.workingMemorySize += valuesCount * entrySize;
					}
					catch (...) {} // an uncountable values domain: leave the term out rather than fail

		return result;
	}

	// mirrors CreateResult below: rlookup(a: V1[D]; b: V2[E]) -> E[D] -- the
	// result's VALUES unit IS b's domain (K4; one variable E in b's domain role
	// and the result's values role -- reduction-honest: the result is flagged
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

private:
	bool m_SkipsNullKeys;
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
	SearchIndexOperatorImpl(AbstrOperGroup* og, bool isExactMatchSearch)
		: AbstrIndexedSearchOperator(og, DataArray<V>::GetStaticClass(), isExactMatchSearch, SkipNull)
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
	RLookupOperator() : rlookup(&cog_rlookup, true) {}
};

template <class V>
struct RLookupWithNullOperator
{
	SearchIndexOperatorImpl<V, rlookup_with_null_dispatcher, false> rlookupWN;
	RLookupWithNullOperator() : rlookupWN(&cog_rlookupWN, true) {}
};

template <typename V>
struct ClassifyOperator : SearchIndexOperatorImpl<V, classify_dispatcher, true>
{
	// false: classify searches the interval a value falls in, not an exact match (#612)
	ClassifyOperator() : SearchIndexOperatorImpl<V, classify_dispatcher, true>(&cog_classify, false) {}
};

#endif // __CLC_RLOOKUPIMPL_H
