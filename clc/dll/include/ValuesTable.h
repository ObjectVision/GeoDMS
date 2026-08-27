// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_VALUESTABLE_H)
#define __CLC_VALUESTABLE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ClcBase.h"

#include "set/CompareFirst.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "FutureTileArray.h"
#include "ParallelTiles.h"
#include "UnitProcessor.h"

#include "AggrFuncNum.h"
#include "AttrBinStruct.h"
#include "IndexGetterCreator.h"
#include "TileChannel.h"
#include "ValuesTableTypes.h"

const UInt32 BUFFER_SIZE = 1024;
const UInt32 MAX_PAIR_COUNT = 4096;

//----------------------------------------------------------------------

template<typename R> void SafeIncrementCounter(R& assignee)
{
	SafeIncrement(assignee);
}

inline void SafeIncrementCounter(SizeT& assignee)
{
	assignee++;
	assert(assignee); // SizeT cannot overflow when counting distict addressable elements
}
inline bool OnlyDefinedCheckRequired(const AbstrDataItem* adi)
{
	DataCheckMode dcm = adi->GetCheckMode();
	return !(dcm & DCM_CheckRange);
}

template <typename V>
auto GetValuesRange(const DataArray<V>* tileFunctor) -> typename Unit<V>::range_t
{
	assert(tileFunctor);
	auto vrd = tileFunctor->GetValueRangeData();
	MG_CHECK(vrd);
	return vrd->GetRange();
}

//----------------------------------------------------------------------

// Ordering of the keys of a (value, count) table.
//
// When nulls are counted too, such as by frequency_table_with_null, operator< is not
// good enough: null is MAX for the unsigned int types and a NaN for the float types.
// DataLessThanCompare orders null before all defined values, which is also where
// operator< puts it for the signed int types that use MIN as null, so the resulting
// order is the same for all value types.
// When nulls are excluded, operator< is used directly to keep the counting hot path cheap.
//
// compare_must_check_undefines_v is precisely the set of types for which operator< gets
// null wrong; for all others -- signed ints, SharedStr, and the bit types that have no
// null at all -- DataLessThanCompare *is* operator<, so the flag is not consulted and
// the comparator compiles down to a bare comparison.

template <typename K>
struct ValueCountKeyCompare
{
	ValueCountKeyCompare(bool valueMustBeDefined) : m_ValueMustBeDefined(valueMustBeDefined) {}

	bool operator ()(const K& lhs, const K& rhs) const
	{
		if constexpr (compare_must_check_undefines_v<K>)
			if (!m_ValueMustBeDefined)
				return DataLessThanCompare<K>()(lhs, rhs);

		if constexpr (has_undefines_v<K>)
		{
			assert(IsDefined(lhs) || !m_ValueMustBeDefined);
			assert(IsDefined(rhs) || !m_ValueMustBeDefined);
		}
		return lhs < rhs;
	}

	bool m_ValueMustBeDefined;
};

// for partitioned counts the partition index is the primary key; it is always defined
template <typename V>
struct ValueCountKeyCompare<Pair<SizeT, V> >
{
	ValueCountKeyCompare(bool valueMustBeDefined) : m_ValueComp(valueMustBeDefined) {}

	bool operator ()(const Pair<SizeT, V>& lhs, const Pair<SizeT, V>& rhs) const
	{
		return lhs.first < rhs.first
			|| (lhs.first == rhs.first && m_ValueComp(lhs.second, rhs.second));
	}

	ValueCountKeyCompare<V> m_ValueComp;
};

template <ordered_value_type V, count_type C>
auto GetCountsDirect(typename sequence_traits<V>::cseq_t data, tile_offset index, tile_offset size, bool valueMustBeDefined) -> ValueCountPairContainerT<V, C>
{
	assert(size <= BUFFER_SIZE);
	assert(size > 0);

	V buffer[BUFFER_SIZE];

	assert(index < data.size());
	assert(size <= data.size());
	assert(index + size <= data.size());

	auto pi = data.begin() + index;
	if constexpr (has_undefines_v<V>)
	{
		auto bufferPtr = &buffer[0];
		for (auto pe = pi + size; pi!=pe; ++pi)
		{
			if (IsDefined(*pi))
				*bufferPtr++ = *pi;
			else if (!valueMustBeDefined)
				MakeUndefined(*bufferPtr++); // don't assign: for sequence-backed V such as SharedStr
			                                 // the conversion turns null into an empty defined value
		}
		size = bufferPtr - buffer;
	}
	else
		fast_copy(pi, pi + size, buffer);
	// Postcondition: when valueMustBeDefined, all buffer ... buffer+size-1 are defined
	if (size == 0)
		return {};

	auto keyComp = ValueCountKeyCompare<V>(valueMustBeDefined);
	std::sort(buffer, buffer + size, keyComp);

	ValueCountPairContainerT<V, C> result;
	result.reserve(size MG_DEBUG_ALLOCATOR_SRC("GetCountsDirect"));

	tile_offset i = 0;
	V currValue = buffer[i++];

	auto currCount = C();
	++currCount;
	for (; i != size; ++i)
	{
		if (keyComp(currValue, buffer[i]))
		{
			result.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("GetCountsDirect") currValue, currCount);
			currValue = buffer[i];
			currCount = C();
		}
		++currCount;
	}
	result.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("GetCountsDirect") currValue, currCount);
	return result;
}

template <ordered_value_type V, count_type C>
auto GetPartitionedCountsDirect(typename sequence_traits<V>::cseq_t data, const IndexGetter* indexGetter, tile_offset index, tile_offset size, SizeT pCount, bool valueMustBeDefined) -> PartionedValueCountPairContainerT<V, C>
{
	assert(size <= BUFFER_SIZE);
	assert(size > 0);

	using partition_value_pair = Pair<SizeT, V>;
	partition_value_pair buffer[BUFFER_SIZE];

	assert(index < data.size());
	assert(size <= data.size());
	assert(index + size <= data.size());

	auto valuesIter = data.begin() + index;
	auto bufferPtr = &buffer[0];
	for (auto valuesEnd = valuesIter + size; valuesIter != valuesEnd; ++valuesIter, ++index)
	{
		if constexpr (has_undefines_v<V>)
		{
			if (valueMustBeDefined && !IsDefined(*valuesIter))
				continue;
		}
		SizeT part_i = indexGetter->Get(index);
		if (!IsDefined(part_i))
			continue;

		auto& target = *bufferPtr++;
		target.first = part_i;
		if constexpr (has_undefines_v<V>)
			if (!IsDefined(*valuesIter))
			{
				// don't convert: for sequence-backed V such as SharedStr
				// the conversion turns null into an empty defined value
				MakeUndefined(target.second);
				continue;
			}
		target.second = V(*valuesIter);
	}

	size = bufferPtr - buffer;
	// Postcondition: when valueMustBeDefined, all buffer ... buffer+size-1 have a defined value
	if (size == 0)
		return {};

	auto keyComp = ValueCountKeyCompare<partition_value_pair>(valueMustBeDefined);
	std::sort(buffer, buffer + size, keyComp);

	PartionedValueCountPairContainerT<V, C> result;
	result.reserve(size MG_DEBUG_ALLOCATOR_SRC("GetPartitionedCountsDirect result buffer"));

	tile_offset i = 0;
	partition_value_pair currPartitionValuePart = buffer[i++];

	auto currCount = C();
	++currCount;
	for (; i != size; ++i)
	{
		if (keyComp(currPartitionValuePart, buffer[i]))
		{
			result.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("GetPartitionedCountsDirect result buffer") currPartitionValuePart, currCount);
			currPartitionValuePart = buffer[i];
			currCount = C();
		}
		++currCount;
	}
	result.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("GetPartitionedCountsDirect result buffer") currPartitionValuePart, currCount);
	return result;
}

// reduce the number of (value, count) pairs by 50% by aggregating couples of pairs 
// assume all values are defined
template <ordered_value_type V, count_type C>
void WeedOutOddPairs(ValueCountPairContainerT<V, C>& vcpc, SizeT maxPairCount)
{
	if (vcpc.size() <= maxPairCount)
		return;

	auto
		currPair = vcpc.begin(),
		lastPair = vcpc.end();

	auto
		donePair = currPair;

	if ((lastPair - currPair) % 2)
		--lastPair;
	while (currPair != lastPair)
	{
		if constexpr (has_undefines_v<V>)
		{
			assert(IsDefined(currPair->first));
		}
		*donePair = *currPair;
		++currPair;
		assert(currPair != lastPair);

		donePair->second += currPair->second;
		++donePair;
		++currPair;
	}
	vcpc.erase(donePair, lastPair);
}

template <ordered_value_type V, count_type C>
auto MergeToLeft(const ValueCountPairContainerT<V, C>& left, const ValueCountPairContainerT<V, C>& right, ValueCountKeyCompare<V> keyComp) -> ValueCountPairContainerT<V, C>
{
	ValueCountPairContainerT<V, C> result;
	result.resize(left.size() + right.size() MG_DEBUG_ALLOCATOR_SRC("MergeToLeft"));

	if (!result.empty())
	{
		auto pairComp = [keyComp](const ValueCountPair<V, C>& lhs, const ValueCountPair<V, C>& rhs) { return keyComp(lhs.first, rhs.first); };
		std::merge(right.begin(), right.end(), left.begin(), left.end(), result.begin(), pairComp);

		auto
			currPair = result.begin(),
			lastPair = result.end(),
			index = currPair + 1;

		while (index != lastPair && keyComp(currPair->first, index->first))
		{
			currPair = index;
			++index;
		}

		V currValue = currPair->first;
		C currCount = currPair->second;
		for (; index != lastPair; ++index)
		{
			if (keyComp(currValue, index->first))
			{
				*currPair++ = ValueCountPair<V, C>(currValue, currCount);
				currValue = index->first;
				currCount = index->second;
			}
			else
				SafeAccumulate(currCount, index->second);
		}
		*currPair++ = ValueCountPair(currValue, currCount);
		result.erase(currPair, lastPair);
	}
	return result;
}

template <ordered_value_type V, count_type C>
auto WeededMergeToLeft(const ValueCountPairContainerT<V, C>& left, const ValueCountPairContainerT<V, C>& right, SizeT maxPairCount, ValueCountKeyCompare<V> keyComp) -> ValueCountPairContainerT<V, C>
{
	auto result = MergeToLeft(left, right, keyComp);
	WeedOutOddPairs(result, maxPairCount); // only weeds when values are known to be defined, see GetWeededCountsOfV
	return result;
}

template <ordered_value_type V, count_type C>
auto GetTileCounts(typename sequence_traits<V>::cseq_t data, SizeT index, SizeT size, bool valueMustBeDefined) -> ValueCountPairContainerT<V, C>
{
	if (size <= BUFFER_SIZE)
		return GetCountsDirect<V, C>(data, index, size, valueMustBeDefined);

	SizeT m = size / 2;

	auto firstHalf = GetTileCounts<V, C>(data, index, m, valueMustBeDefined);
	auto secondHalf = GetTileCounts<V, C>(data, index + m, size - m, valueMustBeDefined);
	return MergeToLeft(firstHalf, secondHalf, ValueCountKeyCompare<V>(valueMustBeDefined));
}

template <ordered_value_type V, count_type C>
auto GetPartitionedTileCounts(typename sequence_traits<V>::cseq_t data, const IndexGetter* indexGetter, SizeT index, SizeT size, SizeT pCount, bool valueMustBeDefined) -> PartionedValueCountPairContainerT<V, C>
{
	if (size <= BUFFER_SIZE)
		return GetPartitionedCountsDirect<V, C>(data, indexGetter, index, size, pCount, valueMustBeDefined);

	SizeT m = size / 2;

	auto firstHalf  = GetPartitionedTileCounts<V, C>(data, indexGetter, index    ,        m, pCount, valueMustBeDefined);
	auto secondHalf = GetPartitionedTileCounts<V, C>(data, indexGetter, index + m, size - m, pCount, valueMustBeDefined);

	return MergeToLeft(firstHalf, secondHalf, ValueCountKeyCompare<Pair<SizeT, V> >(valueMustBeDefined));
}

template <ordered_value_type V, count_type C>
auto GetWeededTileCounts(typename sequence_traits<V>::cseq_t data, SizeT index, SizeT size, SizeT maxPairCount, bool valueMustBeDefined) -> ValueCountPairContainerT<V, C>
{
	if (size <= BUFFER_SIZE)
		return GetCountsDirect<V, C>(data, index, size, valueMustBeDefined);

	SizeT m = size / 2;

	auto firstHalf = GetWeededTileCounts<V, C>(data, index, m, maxPairCount, valueMustBeDefined);
	auto secondHalf = GetWeededTileCounts<V, C>(data, index + m, size - m, maxPairCount, valueMustBeDefined);
	return WeededMergeToLeft(firstHalf, secondHalf, maxPairCount, ValueCountKeyCompare<V>(valueMustBeDefined));
}

template <ordered_value_type V, count_type C>
auto GetWeededWallCounts_ST(future_tile_array<V>& values_fta, tile_id t, tile_id nrTiles, SizeT maxPairCount, bool valueMustBeDefined) -> ValueCountPairContainerT<V, C>
{
	if (nrTiles == 1)
	{
		auto tileData = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		return GetWeededTileCounts<V, C>(tileData, 0, tileData.size(), maxPairCount, valueMustBeDefined);
	}

	tile_id m = nrTiles / 2;
	assert(m >= 1);

	auto firstHalf  = GetWeededWallCounts_ST<V, C>(values_fta, t, m, maxPairCount, valueMustBeDefined);
	auto secondHalf = GetWeededWallCounts_ST<V, C>(values_fta, t + m, nrTiles - m, maxPairCount, valueMustBeDefined);

	return WeededMergeToLeft(firstHalf, secondHalf, maxPairCount, ValueCountKeyCompare<V>(valueMustBeDefined));
}

template <ordered_value_type V, count_type C>
auto GetWeededWallCounts_MT(future_tile_array<V>& values_fta, tile_id t, tile_id nrTiles, SizeT maxPairCount, SizeT availableThreads, bool valueMustBeDefined) -> ValueCountPairContainerT<V, C>
{
	assert(nrTiles);
	assert(availableThreads <= nrTiles);

	if (availableThreads == 1)
	{
		return GetWeededWallCounts_ST<V, C>(values_fta, t, nrTiles, maxPairCount, valueMustBeDefined);
	}

	auto m = nrTiles / 2;
	auto rt = availableThreads / 2;

	auto firstHalf = throttled_async([&values_fta, t, m, maxPairCount, rt, valueMustBeDefined]
		{
			return GetWeededWallCounts_MT<V, C>(values_fta, t, m, maxPairCount, rt, valueMustBeDefined);
		}
	);

	auto secondHalf = GetWeededWallCounts_MT<V, C>(values_fta, t + m, nrTiles - m, maxPairCount, availableThreads - rt, valueMustBeDefined);

	return WeededMergeToLeft(firstHalf->get(), secondHalf, maxPairCount, ValueCountKeyCompare<V>(valueMustBeDefined));
}

template <ordered_value_type V, count_type C>
auto GetWeededWallCounts(future_tile_array<V>& values_fta, SizeT maxPairCount, bool valueMustBeDefined) -> ValueCountPairContainerT<V, C>
{
	auto nrTiles = values_fta.size();
	if (!nrTiles)
		return {};

	SizeT maxNrThreads = MaxAllowedConcurrentTreads();
	MakeMin(maxNrThreads, nrTiles);
	MakeMax(maxNrThreads, 1);

	return GetWeededWallCounts_MT<V, C>(values_fta, 0, nrTiles, maxPairCount, maxNrThreads, valueMustBeDefined);
}

template <ordered_value_type V, count_type C>
auto GetPartitionedWallCounts(future_tile_array<V>& values_fta, const AbstrDataItem* indicesItem, abstr_future_tile_array& part_fta, tile_id t, tile_id nrTiles, SizeT pCount, bool valueMustBeDefined) -> PartionedValueCountPairContainerT<V, C>
{
	if (!nrTiles)
		return {};

	if (nrTiles == 1)
	{
		auto tileData = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		auto indexGetter = std::unique_ptr<IndexGetter>( IndexGetterCreator::Create(indicesItem, part_fta[t])); part_fta[t] = nullptr;
		return GetPartitionedTileCounts<V, C>(tileData, indexGetter.get(), 0, tileData.size(), pCount, valueMustBeDefined);
	}

	tile_id m = nrTiles / 2;
	assert(m >= 1);

	auto firstHalf = throttled_async([&values_fta, indicesItem, &part_fta, t, m, pCount, valueMustBeDefined]
		{
			return GetPartitionedWallCounts<V, C>(values_fta, indicesItem, part_fta, t, m, pCount, valueMustBeDefined);
		}
	);

	auto secondHalf = GetPartitionedWallCounts<V, C>(values_fta, indicesItem, part_fta, t + m, nrTiles - m, pCount, valueMustBeDefined);

	return MergeToLeft(firstHalf->get(), secondHalf, ValueCountKeyCompare<Pair<SizeT, V> >(valueMustBeDefined));
}

inline auto GetDomain(const AbstrDataItem* adi)  { return adi->GetAbstrDomainUnit(); }
//auto GetDomain(Couple<const AbstrDataItem*> adis) { return adis.first->GetAbstrDomainUnit(); }

template<typename V>
struct WallCountsAsArrayInfo
{
	typename Unit<V>::range_t valuesRange;
	SizeT vCount;
	future_tile_ptr<V>* values_fta;
};


template<typename V, typename C>
auto GetWallCountsAsArray(WallCountsAsArrayInfo<V>& info, tile_id t, tile_id te, SizeT availableThreads) -> std::vector<C>
{
	assert(t + availableThreads <= te);
	if (availableThreads > 1)
	{
		auto m = te - (te - t) / 2;
		auto rt = availableThreads / 2;

		auto futureSecondHalfValue = throttled_async([m, te, rt, &info]()
			{
				return GetWallCountsAsArray<V, C>(info, m, te, rt);
			});
		auto firstHalfValue = GetWallCountsAsArray<V, C>(info, t, m, availableThreads - rt);

		auto secondHalfValue = futureSecondHalfValue->get();

		for (SizeT i = 0, e = info.vCount; i < e; ++i)
			firstHalfValue[i] += secondHalfValue[i];
		return firstHalfValue;
	}

	auto localInfo = info;
	if constexpr (!has_undefines_v<V>)
	{
		MG_CHECK(localInfo.vCount == (1 << nrbits_of_v<V>));
	}
	std::vector<C> buffer(localInfo.vCount, 0);
	auto bufferB = buffer.begin();
	for (; t != te; ++t)
	{
		auto valuesLock = localInfo.values_fta[t]->GetTile(); localInfo.values_fta[t] = nullptr;
		auto valuesIter = valuesLock.begin(),
			valuesEnd = valuesLock.end();
		for (; valuesIter != valuesEnd; ++valuesIter)
		{
			if constexpr (has_undefines_v<V>)
			{
				if (!IsDefined(*valuesIter))
					continue;
			}
			auto i = Range_GetIndex_naked(localInfo.valuesRange, *valuesIter);
			if constexpr (has_undefines_v<V>)
			{
				if (i >= localInfo.vCount)
					throwErrorF("Range Error", "Value {} not in expected range from {} till {}", *valuesIter, localInfo.valuesRange.first, localInfo.valuesRange.second);
			}
			SafeIncrementCounter(bufferB[i]);
		}
	}
	return buffer;
}


template<typename V, typename C>
auto GetCountsAsArray(const DataArray<V> * valuesDataArray, typename Unit<V>::range_t valuesRange) -> std::vector<C>
{
	SizeT vCount = Cardinality(valuesRange);
	SizeT maxNrThreads = MaxAllowedConcurrentTreads();
	if (vCount)
		MakeMin(maxNrThreads, valuesDataArray->GetNrFeaturesNow() / vCount);
	MakeMax(maxNrThreads, 1);

	tile_id tn = valuesDataArray->GetTiledRangeData()->GetNrTiles();
	if (!tn)
		return {};
	MakeMin(maxNrThreads, tn);

	auto values_fta = GetFutureTileArray(valuesDataArray);
	WallCountsAsArrayInfo<V> info = { valuesRange, vCount, values_fta.begin() };
	return GetWallCountsAsArray<V, C>(info, 0, tn, maxNrThreads);
}

template <ordered_value_type R, count_type C>
auto MakeValueCountContainer(std::vector<C>&& freqTable) -> ValueCountPairContainerT<R, C>
{
	ValueCountPairContainerT<R, C> result;
	SizeT c = 0;
	for (SizeT i = 0, n = freqTable.size(); i != n; ++i)
		if (freqTable[i] > 0)
			c++;
	result.reserve(c MG_DEBUG_ALLOCATOR_SRC("MakeValueCountContainer"));

	for (SizeT i = 0, n = freqTable.size(); i != n; ++i)
		if (freqTable[i] > 0)
			result.push_back({ i, freqTable[i] } MG_DEBUG_ALLOCATOR_SRC("MakeValueCountContainer"));
	return result;
}

template <ordered_value_type R, typename V, count_type C>
auto GetWeededCountsOfV(const DataArray<V>* valuesTF, bool noOutOfRangeValues,  const Unit<V>* valuesUnit, SizeT maxPairCount) -> ValueCountPairContainerT<R, C>
{
	if constexpr (is_bitvalue_v<scalar_of_t<V>>)
	{
		auto freqTable = GetCountsAsArray<V, C>(valuesTF, valuesUnit->GetRange());
		return MakeValueCountContainer<R, C>(std::move(freqTable));
	}
	else
	{
		if constexpr (is_integral_v<scalar_of_t<V>>)
		{
			if (noOutOfRangeValues)
			{
				SizeT v = valuesUnit->GetDataCount();
				if (IsDefined(v) && v <= maxPairCount)
				{
					SizeT n = valuesTF->GetTiledRangeData()->GetElemCount();
					if (v <= n) // Countable values; go for Table if sensible
					{
						auto range = valuesUnit->GetRange();
						auto freqTable = GetCountsAsArray<V, C>(valuesTF, range);
						auto vcc = MakeValueCountContainer<R, C>(std::move(freqTable));
						R offset = Convert<R>(range.first);
						if (offset != R())
						{
							for (auto& vcPair : vcc)
								vcPair.first += offset;
						}

					}
				}
			}
		}
		auto values_fta = GetFutureTileArray(valuesTF);
		auto vcxxx = GetWeededWallCounts<V, C>(values_fta, maxPairCount, true); // class breaks and unique counts never count nulls
		if constexpr (std::is_same_v<R, V>)
			return vcxxx;
		else
		{
			ValueCountPairContainerT<R, C> result; result.reserve(vcxxx.size() MG_DEBUG_ALLOCATOR_SRC("GetWeededCountsOfV"));
			CountablePointConverter<V> conv(valuesTF->m_ValueRangeDataPtr);
			for (const auto& vcp : vcxxx)
				result.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("GetWeededCountsOfV") conv.template GetScalar<R>(vcp.first), vcp.second);
			return result;
		}
	}
}

template <ordered_value_type R, typename TypeList, count_type C>
auto GetWeededCounts_Impl(const AbstrDataItem* adi, SizeT maxPairCount) -> ValueCountPairContainerT<R, C>
{
	return visit_and_return_result<TypeList, ValueCountPairContainerT<R, C> >(adi->GetAbstrValuesUnit()
		, [adi, maxPairCount]<typename V>(const Unit<V>*valuesUnit) 
			{
				auto tileFunctor = const_array_cast<V>(adi);
				return GetWeededCountsOfV<R, V, C>(tileFunctor, OnlyDefinedCheckRequired(adi), valuesUnit, maxPairCount);
			}
	);
}

template <ordered_value_type R, typename TypeList, count_type C>
auto GetSequentialCounts_impl(const AbstrDataItem* adi) -> ValueCountPairContainerT<R, C>
{
	return visit_and_return_result<TypeList, ValueCountPairContainerT<R, C> >(adi->GetAbstrValuesUnit()
		, [adi]<typename V>(const Unit<V>*valuesUnit)
			{

				ValueCountPairContainerT<R, C> results;

				auto tileFunctor = const_array_cast<V>(adi);
				auto tileChannel = tile_read_channel<V>(tileFunctor);

				while (!tileChannel.AtEnd())
				{
					auto currValue = *tileChannel;
					if (!IsDefined(currValue))
					{
						++tileChannel;
						continue;
					}
					C  c = 0;
					do
					{
						++tileChannel;
						++c;
					} while (!tileChannel.AtEnd() && *tileChannel == currValue);
					assert(tileChannel.AtEnd() || !IsDefined(*tileChannel) || currValue < *tileChannel); // we were guarteeded that values are sorted
					results.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("GetSequentialCounts_impl") currValue, c);
				}
				return results;
			}
	);
}

template <ordered_value_type R, typename TypeList, count_type C>
auto GetCounts_Impl(const AbstrDataItem* adi) -> ValueCountPairContainerT<R, C>
{
	if (adi->m_StatusFlags.HasSortedValues())
		return GetSequentialCounts_impl<R, TypeList, C>(adi);
	return GetWeededCounts_Impl<R, TypeList, C>(adi, SizeT(-1));
}

template <ordered_value_type R, typename TypeList, count_type C>
auto GetCounts(const AbstrDataItem* adi) -> CountsResultTypeT<R, C>
{
	assert(adi);
	assert(adi->GetInterestCount());

	MG_CHECK(adi->GetAbstrValuesUnit()->GetValueType()->IsNumeric());
	MG_CHECK(adi->GetValueComposition() == ValueComposition::Single);

	auto lck = DataReadLock(adi);

	return { GetCounts_Impl<R, TypeList, C>(adi)
		, adi->GetCurrRefObj()->GetAbstrValuesRangeData()   // from lock
	};
}

template <ordered_value_type R, typename TypeList, count_type C>
auto GetWeededCounts(const AbstrDataItem* adi, SizeT maxPairCount) -> CountsResultTypeT<R, C>
{
	assert(adi);
	assert(adi->GetInterestCount());

	MG_CHECK(BUFFER_SIZE <= maxPairCount);

	MG_CHECK(adi->GetAbstrValuesUnit()->GetValueType()->IsNumeric());
	MG_CHECK(adi->GetValueComposition() == ValueComposition::Single);

	auto lck = DataReadLock(adi);

	return { GetWeededCounts_Impl<R, TypeList, C>(adi, maxPairCount)
		, adi->GetCurrRefObj()->GetAbstrValuesRangeData()   // from lock
	};
}

inline auto PrepareWeededCounts(const AbstrDataItem* adi, SizeT maxPairCount) -> CountsResultType
{
	PreparedDataReadLock lck(adi, "PrepareWeededCounts");

	return { GetWeededCounts_Impl<ClassBreakValueType, typelists::num_objects, CountType>(adi, maxPairCount)
	,	adi->GetCurrRefObj()->GetAbstrValuesRangeData() // from lock
	};
}


#endif // !defined(__CLC_VALUESTABLE_H)
