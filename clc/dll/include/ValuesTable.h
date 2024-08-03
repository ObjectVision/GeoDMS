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
#include "ASync.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "FutureTileArray.h"
#include "UnitProcessor.h"

#include "AggrFuncNum.h"
#include "AttrBinStruct.h"
#include "IndexGetterCreator.h"
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

//----------------------------------------------------------------------


template <ordered_value_type V, count_type C>
ValueCountPairContainerT<V, C> GetCountsDirect(typename sequence_traits<V>::cseq_t data, tile_offset index, tile_offset size)
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
			if (IsDefined(*pi))
				*bufferPtr++ = *pi;

		size = bufferPtr - buffer;
	}
	else
		fast_copy(pi, pi + size, buffer);
	// Postcondition: all buffer ... buffer+size-1 are defined
	if (size == 0)
		return {};

	std::sort(buffer, buffer + size);

	ValueCountPairContainerT<V, C> result;
	result.reserve(size);

	tile_offset i = 0;
	V currValue = buffer[i++];

	auto currCount = C();
	++currCount;
	for (; i != size; ++i)
	{
		if (currValue < buffer[i])
		{
			result.emplace_back(currValue, currCount);
			currValue = buffer[i];
			currCount = C();
		}
		++currCount;
	}
	result.emplace_back(currValue, currCount);
	return result;
}

template <ordered_value_type V, count_type C>
auto GetIndexedCountsDirect(typename sequence_traits<V>::cseq_t data
	, const IndexGetter* indexGetter
	, tile_offset index, tile_offset size, SizeT pCount)
-> PartionedValueCountPairContainerT<V, C>
{
	assert(size <= BUFFER_SIZE);
	assert(size > 0);

	using value_type = Pair<SizeT, V>;
	value_type buffer[BUFFER_SIZE];

	assert(index < data.size());
	assert(size <= data.size());
	assert(index + size <= data.size());

	auto valuesIter = data.begin() + index;
	auto bufferPtr = &buffer[0];
	for (auto valuesEnd = valuesIter + size; valuesIter != valuesEnd; ++valuesIter, ++index)
	{
		if constexpr (has_undefines_v<V>)
		{
			if (!IsDefined(*valuesIter))
				continue;
		}
		SizeT part_i = indexGetter->Get(index);
		if (!IsDefined(part_i))
			continue;

		*bufferPtr++ = value_type(part_i, *valuesIter);
	}

	size = bufferPtr - buffer;
	// Postcondition: all buffer ... buffer+size-1 are defined
	if (size == 0)
		return {};

	std::sort(buffer, buffer + size);

	PartionedValueCountPairContainerT<V, C> result;
	result.reserve(size);

	tile_offset i = 0;
	value_type currValue = buffer[i++];

	auto currCount = C();
	++currCount;
	for (; i != size; ++i)
	{
		if (currValue < buffer[i])
		{
			result.emplace_back(currValue, currCount);
			currValue = buffer[i];
			currCount = C();
		}
		++currCount;
	}
	result.emplace_back(currValue, currCount);
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
auto MergeToLeft(const ValueCountPairContainerT<V, C>& left, const ValueCountPairContainerT<V, C>& right) -> ValueCountPairContainerT<V, C>
{
	ValueCountPairContainerT<V, C> result;
	result.resize(left.size() + right.size());

	if (!result.empty())
	{
		std::merge(right.begin(), right.end(), left.begin(), left.end(), result.begin(), CompareFirst());

		auto
			currPair = result.begin(),
			lastPair = result.end(),
			index = currPair + 1;

		while (index != lastPair && currPair->first < index->first)
		{
			currPair = index;
			++index;
		}

		V currValue = currPair->first;
		C currCount = currPair->second;
		for (; index != lastPair; ++index)
		{
			if (currValue < index->first)
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
auto WeededMergeToLeft(const ValueCountPairContainerT<V, C>& left, const ValueCountPairContainerT<V, C>& right, SizeT maxPairCount) -> ValueCountPairContainerT<V, C>
{
	auto result = MergeToLeft(left, right);
	WeedOutOddPairs(result, maxPairCount);
	return result;
}

#include <future>

template <ordered_value_type V, count_type C>
auto GetTileCounts(typename sequence_traits<V>::cseq_t data, SizeT index, SizeT size)
-> ValueCountPairContainerT<V, C>
{
	if (size <= BUFFER_SIZE)
		return GetCountsDirect<V, C>(data, index, size);

	SizeT m = size / 2;

	auto firstHalf = GetTileCounts<V, C>(data, index, m);
	auto secondHalf = GetTileCounts<V, C>(data, index + m, size - m);
	return MergeToLeft(firstHalf, secondHalf);
}

template <ordered_value_type V, count_type C>
auto GetIndexedTileCounts(typename sequence_traits<V>::cseq_t data
	, const IndexGetter* indexGetter, SizeT index, SizeT size, SizeT pCount)
-> PartionedValueCountPairContainerT<V, C>
{
	if (size <= BUFFER_SIZE)
		return GetIndexedCountsDirect<V, C>(data, indexGetter, index, size, pCount);

	SizeT m = size / 2;

	auto firstHalf  = GetIndexedTileCounts<V, C>(data, indexGetter, index    ,        m, pCount);
	auto secondHalf = GetIndexedTileCounts<V, C>(data, indexGetter, index + m, size - m, pCount);

	return MergeToLeft(firstHalf, secondHalf);
}

template <ordered_value_type V, count_type C>
auto GetWeededTileCounts(typename sequence_traits<V>::cseq_t data, SizeT index, SizeT size, SizeT maxPairCount)
-> ValueCountPairContainerT<V, C>
{
	if (size <= BUFFER_SIZE)
		return GetCountsDirect<V, C>(data, index, size);

	SizeT m = size / 2;

	auto firstHalf = GetWeededTileCounts<V, C>(data, index, m, maxPairCount);
	auto secondHalf = GetWeededTileCounts<V, C>(data, index + m, size - m, maxPairCount);
	return WeededMergeToLeft(firstHalf, secondHalf, maxPairCount);
}
/* REMOVE
template <ordered_value_type V, count_type C>
auto GetWallCounts_ST(future_tile_array<V>& values_fta, tile_id t, tile_id nrTiles)
-> ValueCountPairContainerT<V, C>
{
	assert(nrTiles);

	if (nrTiles == 1)
	{
		auto tileData = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		return GetTileCounts<V, C>(tileData, 0, tileData.size());
	}

	tile_id m = nrTiles / 2;
	assert(m >= 1);

	auto firstHalf = GetWallCounts_ST<V, C>(values_fta, t, m);
	auto secondHalf = GetWallCounts_ST<V, C>(values_fta, t + m, nrTiles - m);

	return MergeToLeft(firstHalf, secondHalf);
}

template <ordered_value_type V, count_type C>
auto GetWallCounts_MT(future_tile_array<V>& values_fta, tile_id t, tile_id nrTiles, SizeT availableThreads)
	-> ValueCountPairContainerT<V, C>
{
	assert(nrTiles);
	assert(availableThreads <= nrTiles);
	if (availableThreads <= 1)
	{
		return GetWallCounts_ST<V, C>(values_fta, t, nrTiles);
	}

	tile_id m = nrTiles / 2;
	assert(m >= 1);
	auto rt = availableThreads / 2;

	auto firstHalf = throttled_async([&values_fta, t, m, rt]
		{
			return GetWallCounts_MT<V, C>(values_fta, t, m, rt);
		}
	);

	auto secondHalf = GetWallCounts_MT<V, C>(values_fta, t + m, nrTiles - m, availableThreads - rt);

	return MergeToLeft(firstHalf.get(), secondHalf);
}

template <ordered_value_type V, count_type C>
auto GetWallCounts(future_tile_array<V>& values_fta)
-> ValueCountPairContainerT<V, C>
{
	auto nrTiles = values_fta.size();
	if (!nrTiles)
		return {};

	SizeT maxNrThreads = MaxAllowedConcurrentTreads();
	MakeMin(maxNrThreads, nrTiles);
	MakeMax(maxNrThreads, 1);

	return GetWallCounts_MT<V, C>(values_fta, 0, nrTiles, maxNrThreads);
}
*/

template <ordered_value_type V, count_type C>
auto GetWeededWallCounts_ST(future_tile_array<V>& values_fta, tile_id t, tile_id nrTiles, SizeT maxPairCount)
	-> ValueCountPairContainerT<V, C>
{
	if (nrTiles == 1)
	{
		auto tileData = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		return GetWeededTileCounts<V, C>(tileData, 0, tileData.size(), maxPairCount);
	}

	tile_id m = nrTiles / 2;
	assert(m >= 1);

	auto firstHalf  = GetWeededWallCounts_ST<V, C>(values_fta, t, m, maxPairCount);
	auto secondHalf = GetWeededWallCounts_ST<V, C>(values_fta, t + m, nrTiles - m, maxPairCount);

	return WeededMergeToLeft(firstHalf, secondHalf, maxPairCount);
}

template <ordered_value_type V, count_type C>
auto GetWeededWallCounts_MT(future_tile_array<V>& values_fta, tile_id t, tile_id nrTiles, SizeT maxPairCount, SizeT availableThreads)
-> ValueCountPairContainerT<V, C>
{
	assert(nrTiles);
	assert(availableThreads <= nrTiles);

	if (availableThreads == 1)
	{
		return GetWeededWallCounts_ST<V, C>(values_fta, t, nrTiles, maxPairCount);
	}

	auto m = nrTiles / 2;
	auto rt = availableThreads / 2;

	auto firstHalf = throttled_async([&values_fta, t, m, maxPairCount, rt]
		{
			return GetWeededWallCounts_MT<V, C>(values_fta, t, m, maxPairCount, rt);
		}
	);

	auto secondHalf = GetWeededWallCounts_MT<V, C>(values_fta, t + m, nrTiles - m, maxPairCount, availableThreads - rt);

	return WeededMergeToLeft(firstHalf.get(), secondHalf, maxPairCount);
}

template <ordered_value_type V, count_type C>
auto GetWeededWallCounts(future_tile_array<V>& values_fta, SizeT maxPairCount)
-> ValueCountPairContainerT<V, C>
{
	auto nrTiles = values_fta.size();
	if (!nrTiles)
		return {};

	SizeT maxNrThreads = MaxAllowedConcurrentTreads();
	MakeMin(maxNrThreads, nrTiles);
	MakeMax(maxNrThreads, 1);

	return GetWeededWallCounts_MT<V, C>(values_fta, 0, nrTiles, maxPairCount, maxNrThreads);
}

template <ordered_value_type V, count_type C>
auto GetIndexedWallCounts(future_tile_array<V>& values_fta, const AbstrDataItem* indicesItem, abstr_future_tile_array& part_fta, tile_id t, tile_id nrTiles, SizeT pCount) 
	-> PartionedValueCountPairContainerT<V, C>
{
	if (!nrTiles)
		return {};

	if (nrTiles == 1)
	{
		auto tileData = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, part_fta[t]); part_fta[t] = nullptr;
		return GetIndexedTileCounts<V, C>(tileData, indexGetter.get(), 0, tileData.size(), pCount);
	}

	tile_id m = nrTiles / 2;
	assert(m >= 1);

	auto firstHalf = throttled_async([&values_fta, indicesItem, &part_fta, t, m, pCount]
		{
			return GetIndexedWallCounts<V, C>(values_fta, indicesItem, part_fta, t, m, pCount);
		}
	);

	auto secondHalf = GetIndexedWallCounts<V, C>(values_fta, indicesItem, part_fta, t + m, nrTiles - m, pCount);

	return MergeToLeft(firstHalf.get(), secondHalf);
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
		auto secondHalfValue = futureSecondHalfValue.get();

		for (SizeT i = 0, e = info.vCount; i < e; ++i)
			firstHalfValue[i] += secondHalfValue[i];
		return firstHalfValue;
	}

	auto localInfo = info;
	std::vector<C> buffer(localInfo.vCount, 0);
	auto bufferB = buffer.begin();
	for (; t != te; ++t)
	{
		auto valuesLock = localInfo.values_fta[t]->GetTile(); localInfo.values_fta[t] = nullptr;
		auto valuesIter = valuesLock.begin(),
			valuesEnd = valuesLock.end();
		for (; valuesIter != valuesEnd; ++valuesIter)
		{
			if (IsDefined(*valuesIter))
			{
				auto i = Range_GetIndex_naked(localInfo.valuesRange, *valuesIter);
				assert(i < localInfo.vCount);
				SafeIncrementCounter(bufferB[i]);
			}
		}
	}
	return buffer;
}


template<typename V, typename C>
auto GetCountsAsArray(const DataArray<V> * valuesDataArray, typename Unit<V>::range_t valuesRange) -> std::vector<C>
{
	SizeT vCount = Cardinality(valuesRange);
//	auto valuesDataArray = const_array_cast<V>(valuesItem);
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
	for (SizeT i = 0, n = freqTable.size(); i != n; ++i)
		if (freqTable[i] > 0)
			result.insert(result.end(), { i, freqTable[i] });
	return result;
}

template <ordered_value_type R, typename V, count_type C>
auto GetWeededCountsOfV(const DataArray<V>* tileFunctor, const Unit<V>* valuesUnit, SizeT maxPairCount) -> ValueCountPairContainerT<R, C>
{
	if constexpr (is_integral_v<scalar_of_t<V>>)
	{
		SizeT v = valuesUnit->GetCount();
		if (IsDefined(v) && v <= maxPairCount)
		{
			SizeT n = tileFunctor->GetTiledRangeData()->GetDataSize();
			if (v <= n) // Countable values; go for Table if sensible
			{
				auto freqTable = GetCountsAsArray<V, C>(tileFunctor, valuesUnit->GetRange());
				return MakeValueCountContainer<R, C>(std::move(freqTable));
			}
		}
	}
	auto values_fta = GetFutureTileArray(tileFunctor);
	auto vcxxx = GetWeededWallCounts<V, C>(values_fta, maxPairCount);
	if constexpr (std::is_same_v<R, V>)
		return vcxxx;
	else
	{
		ValueCountPairContainerT<R, C> result; result.reserve(vcxxx.size());
		CountablePointConverter<V> conv(tileFunctor->m_ValueRangeDataPtr);
		for (const auto& vcp : vcxxx)
			result.emplace_back(conv.GetScalar<R>(vcp.first), vcp.second);
		return result;
	}
}

template <ordered_value_type R, typename TypeList, count_type C>
auto GetWeededCounts_Impl(const AbstrDataItem* adi, SizeT maxPairCount) -> ValueCountPairContainerT<R, C>
{
	return visit_and_return_result<TypeList, ValueCountPairContainerT<R, C> >(adi->GetAbstrValuesUnit()
		, [adi, maxPairCount]<typename V>(const Unit<V>*valuesUnit) 
			{
				auto tileFunctor = const_array_cast<V>(adi);
				return GetWeededCountsOfV<R, V, C>(tileFunctor, valuesUnit, maxPairCount);
			}
	);
}

template <ordered_value_type R, typename TypeList, count_type C>
auto GetCounts_Impl(const AbstrDataItem* adi) -> ValueCountPairContainerT<R, C>
{
	return GetWeededCounts_Impl<R, TypeList, C>(adi, SizeT(-1));
}

template <ordered_value_type R, typename TypeList, count_type C>
auto GetCounts(const AbstrDataItem* adi)
-> CountsResultTypeT<R, C>
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
auto GetWeededCounts(const AbstrDataItem* adi, SizeT maxPairCount)
-> CountsResultTypeT<R, C>
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

inline auto PrepareWeededCounts(const AbstrDataItem* adi, SizeT maxPairCount)
-> CountsResultType
{
	PreparedDataReadLock lck(adi, "PrepareWeededCounts");

	return { GetWeededCounts_Impl<ClassBreakValueType, typelists::num_objects, CountType>(adi, maxPairCount)
	,	adi->GetCurrRefObj()->GetAbstrValuesRangeData() // from lock
	};
}


#endif // !defined(__CLC_VALUESTABLE_H)
