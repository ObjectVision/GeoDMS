// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "geo/IsNotUndef.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "set/DataCompare.h"
#include "utl/TypeListOper.h"
#include "RtcTypeLists.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "TileChannel.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "rlookup.h"

template <typename V> const UInt32 BUFFER_SIZE = 4096 / sizeof(V);

template <typename V>
std::vector<V> GetUniqueValuesDirect(typename DataArray<V>::locked_cseq_t seq, tile_offset index, tile_offset size, bool mustBeDefined)
{
	// PRECONDITIONS
	assert(size <= BUFFER_SIZE<V>);
	assert(size > 0);
	assert(index <= seq.size());
	assert(index + size <= seq.size());

	V buffer[BUFFER_SIZE<V>]; 
	V* bufferCursor = nullptr;
	//	fast_copy(seq.begin() + index, seq.begin() + index + size, buffer);

	//	copy only values that are defined and not equal to their predecessor
	auto i = seq.begin() + index, e = i + size;

	DataLessThanCompare<V> cmp;

	if constexpr (compare_must_check_undefines_v<V>)
	{
		static_assert(has_undefines_v<V>);
		if constexpr (equality_must_check_undefines_v<V>)
		{
			if (!mustBeDefined)
			{
				// NaN == NaN returns false
				static_assert(!(UNDEFINED_VALUE(V) == UNDEFINED_VALUE(V)));

				// areEqual returns true iff a and b are "the same" (i.e., not different)
				//				auto areEqual = [&cmp](auto const& a, auto const& b) {
				//					return a == b || !IsDefined(a) && !IsDefined(b); // a and b are "equal" if NOT (a != b), i.e. NOT (cmp(a,b) || cmp(b,a)).
				//				};

				auto areEqual = [](auto const& a, auto const& b) {  return a == b || !IsDefined(a) && !IsDefined(b); };
				bufferCursor = std::unique_copy(i, e, buffer, areEqual);
				// use the ordering that handles null well
				std::sort(buffer, bufferCursor, cmp);
				bufferCursor = std::unique(buffer, bufferCursor, areEqual);
				return std::vector<V>(buffer, bufferCursor);
			}
		}
		else
		{
			// NaN not an issue
			static_assert(UNDEFINED_VALUE(V) == UNDEFINED_VALUE(V)); // must be defined
			if (!mustBeDefined)
			{
				bufferCursor = std::unique_copy(i, e, buffer); // remove adjacent duplicates while copying to buffer
				// use the ordering that handles null well
				std::sort(buffer, bufferCursor, cmp); // sort with nulll on top
				bufferCursor = std::unique(buffer, bufferCursor); // use the normal equality compare operator
				return std::vector<V>(buffer, bufferCursor);
			}
		}
	}
	else
	{
		static_assert(!std::is_floating_point_v<V>); // must be in the first category
		static_assert(!equality_must_check_undefines_v<V>); // must be in the first category
	}

	if (!has_undefines_v<V> || !mustBeDefined)
	{
		bufferCursor = std::unique_copy(i, e, buffer); // remove adjacent duplicates while copying to buffer
	}
	else
	{
		bufferCursor = std::copy_if(i, e, buffer, [](const V& x) { return IsNotUndef(x); }); // copy only defined values
		bufferCursor = std::unique(buffer, bufferCursor); // remove duplicates
	}
	// use the simpler ordering as null has been filtered  out or is irrelevant
	std::sort(buffer, bufferCursor); // sort
	bufferCursor = std::unique(buffer, bufferCursor); // use the normal equality compare operator
	return std::vector<V>(buffer, bufferCursor);
}

template <typename Iter, typename Pred>
auto set_union_by_move(Iter first1, Iter last1, Iter first2, Iter last2, Iter dest, Pred pred) -> Iter
{
	for (; first1 != last1 && first2 != last2; ++dest) {
		if (pred(*first1, *first2)) { // copy first
			*dest = std::move(*first1);
			++first1;
		}
		else if (pred(*first2, *first1)) { // copy second
			*dest = std::move(*first2);
			++first2;
		}
		else { // advance both
			*dest = std::move(*first1);
			++first1;
			++first2;
		}
	}

	dest = fast_move(first1, last1, dest);
	dest = fast_move(first2, last2, dest);
	return dest;
}

template <typename V>
std::vector<V> MergeToLeft(std::vector<V> left, std::vector<V> right, bool mustBeDefined)
{
	std::vector<V> result;
	result.resize(left.size() + right.size());

	if (!result.empty())
	{
		if constexpr (compare_must_check_undefines_v<V>)
			if (!mustBeDefined)
			{
				auto actualEnd = set_union_by_move(right.begin(), right.end(), left.begin(), left.end(), result.begin(), DataLessThanCompareImpl<V, true>());
				result.erase(actualEnd, result.end());
				return result;
			}
		auto actualEnd = set_union_by_move(right.begin(), right.end(), left.begin(), left.end(), result.begin(), std::less<void>());
		result.erase(actualEnd, result.end());
	}
	return result;
}

template <typename V>
std::vector<V> GetTileUniqueValues(typename DataArray<V>::locked_cseq_t tileData, tile_offset index, tile_offset size, bool mustBeDefined)
{
	if (size <= BUFFER_SIZE<V>)
		return GetUniqueValuesDirect<V>(tileData, index, size, mustBeDefined);

	std::vector<V> result;
	tile_offset m = size / 2;

	auto firstHalf = throttled_async([&tileData, index, m, mustBeDefined]() {
		return GetTileUniqueValues<V>(tileData, index, m, mustBeDefined);
		}
	);

	auto secondHalf = GetTileUniqueValues<V>(tileData, index + m, size - m, mustBeDefined);

	return MergeToLeft<V>(std::move(firstHalf->get()), std::move(secondHalf), mustBeDefined);
}

template <typename V>
std::vector<V> GetUniqueWallValues(const DataArray<V>* ado, tile_id t, tile_id nrTiles, bool mustBeDefined)
{
	dms_assert(nrTiles >= 1); // PRECONDITION
	if (nrTiles == 1)
	{
		auto tileSize = ado->GetTiledRangeData()->GetTileSize(t);
		return GetTileUniqueValues<V>(ado->GetTile(t), 0, tileSize, mustBeDefined);
	}

	tile_id m = nrTiles / 2;
	dms_assert(m >= 1);

	auto firstHalf = throttled_async([ado, t, m, mustBeDefined]() {
		return GetUniqueWallValues<V>(ado, t, m, mustBeDefined);
		});

	auto secondHalf = GetUniqueWallValues<V>(ado, t + m, nrTiles - m, mustBeDefined);

	return MergeToLeft<V>(firstHalf->get(), std::move(secondHalf), mustBeDefined);
}


template<fixed_elem V>
void GetUniqueValues(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* adi, bool mustBeDefined)
{
	dms_assert(adi && adi->GetInterestCount());

	const DataArray<V>* ado = const_array_cast<V>(adi);
	std::vector<V> values;
	SizeT count = ado->GetTiledRangeData()->GetElemCount();
	if (count)
	{
		if (adi->m_StatusFlags.HasSortedValues())
		{
			auto tileChannel = tile_read_channel<V>(ado);
			while (!tileChannel.AtEnd())
			{
				auto currValue = *tileChannel;
				if (mustBeDefined && !IsDefined(currValue))
				{
					++tileChannel;
					continue;
				}
				do
				{
					++tileChannel;
				} while (!tileChannel.AtEnd() && *tileChannel == currValue);
				assert(tileChannel.AtEnd() || !IsDefined(*tileChannel) || currValue < *tileChannel); // we were guarteeded that values are sorted
				values.push_back(currValue);
			}
		}
		else
		{
			tile_id tn = ado->GetTiledRangeData()->GetNrTiles();
			if (tn)
				values = GetUniqueWallValues<V>(ado, 0, tn, mustBeDefined);
		}
	}

	res->SetCount(values.size());

	locked_tile_write_channel<V> resWriter(resSub);
	resWriter.Write(values.begin(), values.end());
	dms_assert(resWriter.IsEndOfChannel());
	resWriter.Commit();
}

template <typename Iter, typename Pred>
Iter make_strict_monotonous(Iter first, Iter last, Pred pred)
{
	if (first == last)
		return last;
	Iter result = first;
	while (++first != last && pred(*result, *first))
		++result;
	if (first == last)
		return last;
	assert(result != first);
	while (++first != last)
		if (pred(*result, *first))
			*++result = std::move(*first);

	return ++result;
}

template<sequence_or_string V>
void GetUniqueValues(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* adi, bool mustBeDefined)
{
	auto allValues = const_array_cast<V>(adi)->GetDataRead();

	using ConstDataIter = DataArrayBase<V>::const_iterator;
	visit<typelists::domain_elements>(adi->GetAbstrDomainUnit(), [res, resSub, allValues, mustBeDefined]<typename E>(const Unit<E>*arg2Domain)
	{
		using index_type = typename cardinality_type<E>::type;
		std::vector<index_type> index;
		if (mustBeDefined)
			make_index_skip_null(index, allValues.size(), allValues.begin());
		else
			make_index_all_values(index, allValues.size(), allValues.begin());

		auto indexEnd = make_strict_monotonous(index.begin(), index.end(), IndexCompareOper<ConstDataIter, index_type>(allValues.begin()));
		index.erase(indexEnd, index.end());

		SizeT nrUndefined = 0;
		if (mustBeDefined)
		{
			for (auto i : index)
				if (!IsDefined(allValues[i]))
					++nrUndefined;
		}

		res->SetCount(index.size() - nrUndefined);

		locked_tile_write_channel<V> resWriter(resSub);

		if (mustBeDefined)
		{
			for (auto i : index)
				if (IsDefined(allValues[i]))
					resWriter.Write(allValues[i]);
		}
		else
			for (auto i : index)
				resWriter.Write(allValues[i]);

		assert(resWriter.IsEndOfChannel());
		resWriter.Commit();
	});
}


// *****************************************************************************
//                         Unique
// *****************************************************************************

CommonOperGroup cog_unique("unique", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique64("unique_uint64", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique32("unique_uint32", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique16("unique_uint16", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique08("unique_uint8", oper_policy::dynamic_result_class);

CommonOperGroup cog_uniqueWN("unique_with_null", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique64WN("unique_uint64_with_null", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique32WN("unique_uint32_with_null", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique16WN("unique_uint16_with_null", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique08WN("unique_uint8_with_null", oper_policy::dynamic_result_class);

static TokenID s_Values = GetTokenID_st("Values");

class AbstrUniqueOperator : public UnaryOperator
{
public:
	// Override Operator
	AbstrUniqueOperator(ClassCPtr argCls, AbstrOperGroup& og, const UnitClass* resDomainCls, bool mustBeDefined)
		: UnaryOperator(&og, AbstrUnit::GetStaticClass(), argCls) 
		, m_ResDomainClass(resDomainCls)
		, m_MustBeDefined(mustBeDefined)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrDataItem*  arg1 = debug_cast<const AbstrDataItem*>(args[0]);

		assert(arg1);
		const AbstrUnit* arg1Values   = arg1->GetAbstrValuesUnit();
		const UnitClass* resDomainCls = m_ResDomainClass;
		if (!resDomainCls)
		{
			const ValueClass* vc = arg1->GetAbstrDomainUnit()->GetUnitClass()->GetValueType();
			resDomainCls = UnitClass::Find(vc->GetCrdClass());
		}

		AbstrUnit* res = resDomainCls->CreateResultUnit(resultHolder).release();
		assert(res);
		res->SetTSF(TSF_Categorical);
		resultHolder = res;

		AbstrDataItem* resSub = CreateDataItem(res, s_Values, res, arg1Values, arg1->GetValueComposition() );
		MG_PRECONDITION(resSub);
		resSub->m_StatusFlags.SetHasSortedValues();

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1);
			Calculate(res, resSub, arg1);
		}
		return true;
	}
	virtual void Calculate(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* arg1) const =0;

	const UnitClass* m_ResDomainClass = nullptr;
	bool m_MustBeDefined;
};

template <typename V>
class UniqueOperator : public AbstrUniqueOperator
{
	typedef DataArray<V>      ArgType;
	typedef AbstrUnit         ResultType;
	typedef DataArray<V>      ResultSubType;

public:
	// Override Operator
	UniqueOperator(AbstrOperGroup& og, const UnitClass* resDomainCls, bool mustBeDefined)
		:	AbstrUniqueOperator(ArgType::GetStaticClass(), og, resDomainCls, mustBeDefined)
	{}

	void Calculate(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* arg1A) const override
	{
		GetUniqueValues<V>(res, resSub, arg1A, m_MustBeDefined);
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	tl_oper::inst_tuple_templ<typelists::value_elements, UniqueOperator>
		uniqueOperatorsXX(cog_unique, nullptr, true)
		, uniqueOperators64(cog_unique64, Unit<UInt64>::GetStaticClass(), true)
		, uniqueOperators32(cog_unique32, Unit<UInt32>::GetStaticClass(), true)
		, uniqueOperators16(cog_unique16, Unit<UInt16>::GetStaticClass(), true)
		, uniqueOperators08(cog_unique08, Unit<UInt8>::GetStaticClass(), true);

	tl_oper::inst_tuple_templ<typelists::value_elements, UniqueOperator>
		uniqueOperatorsWN(cog_uniqueWN, nullptr, false)
		, uniqueOperators64WN(cog_unique64WN, Unit<UInt64>::GetStaticClass(), false)
		, uniqueOperators32WN(cog_unique32WN, Unit<UInt32>::GetStaticClass(), false)
		, uniqueOperators16WN(cog_unique16WN, Unit<UInt16>::GetStaticClass(), false)
		, uniqueOperators08WN(cog_unique08WN, Unit<UInt8>::GetStaticClass(), false);

} // end anonymous namespace



