// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_CLASSBREAKS_H)
#define __SHV_CLASSBREAKS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ClcBase.h"

#include "cpc/types.h"
#include "set/CompareFirst.h"
#include "ASync.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataLocks.h"
#include "UnitProcessor.h"

#define MG_DEBUG_CLASSBREAKS false

//----------------------------------------------------------------------
// typedefs
//----------------------------------------------------------------------

using ClassBreakValueType = Float64;
template<typename CB> using break_array_t = std::vector<CB>;
using break_array = break_array_t<ClassBreakValueType>;


template<typename C>
concept ordered_value_type = requires(C a, C b)
{
	{ a < b } -> std::convertible_to<bool>;
};

template<typename C>
concept count_type = requires(C a, C b)
{
	++a;
	a += b;
};

using CountType = SizeT;

template <typename V> using my_vector = std::vector<V, my_allocator<V>>;

template<ordered_value_type V, count_type C = CountType> using ValueCountPair = std::pair<V, C>;
template<ordered_value_type V, count_type C = CountType> using ValueCountPairContainerT= my_vector< ValueCountPair<V, C> >;

template<ordered_value_type V, count_type C>
auto GetTotalCount(const ValueCountPairContainerT<V, C>& vcpc) -> SizeT 
{ 
	SizeT total = 0;
	for (const auto& vcp : vcpc)
		total += vcp.second;
	return total;
}

template<ordered_value_type V>
auto GetTotalCount(const ValueCountPairContainerT<V, Void>& vcpc) -> SizeT
{
	return vcpc.size(); // each value counts as one
}


using ValueCountPairContainer = ValueCountPairContainerT<ClassBreakValueType, CountType>;

using AbstrValuesRangeDataPtrType = SharedPtr<const SharedObj>;
template<ordered_value_type V, count_type C> using CountsResultTypeT = std::pair<ValueCountPairContainerT<V, C>, AbstrValuesRangeDataPtrType>;
using CountsResultType = CountsResultTypeT<ClassBreakValueType, CountType>;
const UInt32 MAX_PAIR_COUNT = 4096;

//----------------------------------------------------------------------

const UInt32 BUFFER_SIZE = 1024;

template <ordered_value_type V, count_type C>
ValueCountPairContainerT<V, C> GetCountsDirect(typename sequence_traits<V>::cseq_t data, typename DataArray<V>::value_range_ptr_t valuesRangePtr, tile_offset index, tile_offset size)
{
	assert(size <= BUFFER_SIZE);
	assert(size > 0);

	ValueCountPairContainerT<V, C> result;

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

	std::sort(buffer, buffer + size);

	result.reserve(size);

	tile_offset i = 0;
	V currValue = buffer[i++];

	auto currCount = C();
	++currCount;
	for (; i != size; ++i)
	{
		if (currValue < buffer[i])
		{
			result.push_back(ValueCountPair(currValue, currCount));
			currValue = buffer[i];
			currCount = C();
		}
		++currCount;
	}
	result.push_back(ValueCountPair(currValue, currCount));
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
auto MergeToLeft(const ValueCountPairContainerT<V, C>& left, const ValueCountPairContainerT<V, C>& right, SizeT maxNrPairs) -> ValueCountPairContainerT<V, C>
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
				currCount += index->second;
		}
		*currPair++ = ValueCountPair(currValue, currCount);
		result.erase(currPair, lastPair);

		WeedOutOddPairs(result, maxNrPairs);
	}
	return result;
}

#include <future>

template <ordered_value_type V, count_type C>
auto GetTileCounts(typename sequence_traits<V>::cseq_t data, typename DataArray<V>::value_range_ptr_t valuesRangePtr, SizeT index, SizeT size, SizeT maxPairCount)
-> ValueCountPairContainerT<V, C>
{
	if (size <= BUFFER_SIZE)
		return GetCountsDirect<V, C>(data, valuesRangePtr, index, size);

	SizeT m = size / 2;

	auto firstHalf = GetTileCounts<V, C>(data, valuesRangePtr, index, m, maxPairCount);
	auto secondHalf = GetTileCounts<V, C>(data, valuesRangePtr, index + m, size - m, maxPairCount);
	return MergeToLeft(firstHalf, secondHalf, maxPairCount);
}

template <ordered_value_type V, count_type C>
auto GetWallCounts(const DataArray<V>* tileFunctor, tile_id t, tile_id nrTiles, SizeT maxPairCount) -> ValueCountPairContainerT<V, C>
{
	assert(nrTiles >= 1); // PRECONDITION
	if (nrTiles == 1)
	{
		auto tileData = tileFunctor->GetTile(t);
		return GetTileCounts<V, C>(tileData, tileFunctor->m_ValueRangeDataPtr, 0, tileData.size(), maxPairCount);
	}

	tile_id m = nrTiles / 2;
	assert(m >= 1);

	auto firstHalf = throttled_async([tileFunctor, t, m, maxPairCount]
		{
			return GetWallCounts<V, C>(tileFunctor, t, m, maxPairCount);
		}
	);

	auto secondHalf = GetWallCounts<V, C>(tileFunctor, t + m, nrTiles - m, maxPairCount);

	return MergeToLeft(firstHalf.get(), secondHalf, maxPairCount);
}

template <ordered_value_type R, count_type C>
auto GetCounts_Impl(const AbstrDataItem* adi, SizeT maxPairCount) -> ValueCountPairContainerT<R, C>
{
	SizeT count = adi->GetAbstrDomainUnit()->GetCount();
	if (count)
	{
		tile_id tn = adi->GetAbstrDomainUnit()->GetNrTiles();
		if (tn)
		{
			auto avu = adi->GetAbstrValuesUnit();
			return visit_and_return_result<typelists::num_objects, ValueCountPairContainerT<R, C> >(avu
				, [adi, tn, maxPairCount]<typename V>(const Unit<V>*valuesUnit) 
					{
						auto tileFunctor = const_array_cast<V>(adi);
						auto vcxxx = GetWallCounts<V, C>(tileFunctor, 0, tn, maxPairCount);
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
			);
		}
	}
	return {};
}

template <ordered_value_type R, count_type C> auto GetCounts(const AbstrDataItem* adi, SizeT maxPairCount)
-> CountsResultTypeT<R, C>
{
	assert(adi && adi->GetInterestCount());

	MG_CHECK(BUFFER_SIZE <= maxPairCount);

	MG_CHECK(adi->GetAbstrValuesUnit()->GetValueType()->IsNumeric());
	MG_CHECK(adi->GetValueComposition() == ValueComposition::Single);

	DataReadLock lck(adi);

	return { GetCounts_Impl<R, C>(adi, maxPairCount), adi->GetCurrRefObj()->GetAbstrValuesRangeData() }; // from lock
}

inline CountsResultType PrepareCounts(const AbstrDataItem* adi, SizeT maxPairCount)
{
	PreparedDataReadLock lck(adi, "PrepareCounts");

	return GetCounts<ClassBreakValueType, CountType>(adi, maxPairCount);
}



//----------------------------------------------------------------------
// class break functions
//----------------------------------------------------------------------

CLC_CALL void ClassifyLogInterval(break_array& faLimits, SizeT k, const ValueCountPairContainer& vcpc);

CLC_CALL break_array ClassifyJenksFisher(const ValueCountPairContainer& vcpc, SizeT kk, bool separateZero);
CLC_CALL void FillBreakAttrFromArray(AbstrDataItem* breakAttr, const break_array& data, const SharedObj* abstrValuesRangeData);

//----------------------------------------------------------------------
// breakAttr functions
//----------------------------------------------------------------------

CLC_CALL break_array ClassifyUniqueValues (AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData);
CLC_CALL break_array ClassifyEqualCount(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData);
CLC_CALL break_array ClassifyNZEqualCount   (AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData);
CLC_CALL break_array ClassifyEqualInterval(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData);
CLC_CALL break_array ClassifyNZEqualInterval(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData);
CLC_CALL break_array ClassifyLogInterval  (AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData);
CLC_CALL break_array ClassifyJenksFisher  (AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData, bool separateZero);
inline break_array ClassifyNZJenksFisher(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData) { return ClassifyJenksFisher(breakAttr, vcpc, abstrValuesRangeData, true ); }
inline break_array ClassifyCRJenksFisher(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData) { return ClassifyJenksFisher(breakAttr, vcpc, abstrValuesRangeData, false); }


using ClassBreakFunc = break_array (*)(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData);

#endif // !defined(__SHV_CLASSBREAKS_H)
