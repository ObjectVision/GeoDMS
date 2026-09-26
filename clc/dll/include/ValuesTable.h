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
const UInt32 MAX_BIN_COUNT = 0x10000; // the counters of one BinCounts: every 16-bit value, in 512 KB

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

//----------------------------------------------------------------------
// Counting integral values in bins
//
// Bit values all fit in a table of their 2^N values, see GetWeededWallCounts. Other integral values could lie too
// far apart for that: where the defined values of a tile lie close together, GetWeededWallCounts counts them in bins,
// one counter per value from the lowest on, instead of sorting them. The bins are sized to the values, not to the
// values unit's range, which does not bound them: a value outside that range is data too. All tiles of a worker count
// into the same bins, which grow for a tile whose values reach beyond them, and the workers add up their bins. Values
// that would make the bins too large are sorted and merged as before. Bins never outnumber maxPairCount, so the pairs
// they yield are the ones sorting yields.
//----------------------------------------------------------------------

// the number of values from lo up to and including hi if that is at most maxSpan, 0 otherwise
template <typename V>
SizeT BinSpan(V lo, V hi, SizeT maxSpan)
{
	assert(!(hi < lo));
	SizeT diff = unsigned_type_t<V>(unsigned_type_t<V>(hi) - unsigned_type_t<V>(lo));
	return diff < maxSpan ? diff + 1 : 0;
}

template <ordered_value_type V, count_type C>
struct BinCounts
{
	using uint_type = unsigned_type_t<V>;

	static SizeT MaxBins(SizeT maxPairCount) { return Min<SizeT>(maxPairCount, MAX_BIN_COUNT); }

	bool HasBins() const { return !m_Bins.empty(); }
	bool IsEmpty() const { return !HasBins() && m_NullCount == C(); }
	V    Last()    const { assert(HasBins()); return V(uint_type(uint_type(m_First) + (m_Bins.size() - 1))); }

	// the bin of v, or at least the number of bins when v lies outside them: below m_First the difference wraps around
	SizeT Offset(V v) const { return uint_type(uint_type(v) - uint_type(m_First)); }

	// Counts the values of a tile in the bins and returns the (value, count) pairs, sorted, of those that do not fit.
	// A tile sizes absent bins to its own values and grows present ones for values beyond them, as long as the bins
	// stay within MaxBins and within the number of values counted, so that a small tile of widely spread values does
	// not pay for a large table.
	auto CountTile(typename sequence_traits<V>::cseq_t data, SizeT maxPairCount, bool valueMustBeDefined) -> ValueCountPairContainerT<V, C>
	{
		SizeT maxSpan = Min<SizeT>(MaxBins(maxPairCount), Max<SizeT>(m_NrCounted + data.size(), BUFFER_SIZE));
		if (!HasBins())
		{
			V lo = MinValue<V>(), hi = MaxValue<V>(); // the 255 defined values of an 8-bit type fit without looking
			if constexpr (sizeof(V) > 1)
			{
				auto valuePtr = std::find_if(data.begin(), data.end(), [](V v) { return IsDefined(v); });
				if (valuePtr == data.end())
				{
					if (!valueMustBeDefined)
						m_NullCount += data.size();
					return {};
				}
				lo = hi = *valuePtr;
				for (; valuePtr != data.end(); ++valuePtr)
					if (IsDefined(*valuePtr))
					{
						MakeMin(lo, *valuePtr);
						MakeMax(hi, *valuePtr);
					}
			}
			SizeT span = BinSpan(lo, hi, maxSpan);
			if (!span)
				return GetWeededTileCounts<V, C>(data, 0, data.size(), maxPairCount, valueMustBeDefined);
			m_First = lo;
			m_Bins = my_vector<C>(span MG_DEBUG_ALLOCATOR_SRC("BinCounts::CountTile")); // zeroed
		}

		my_vector<V> outside;
		SizeT nrNulls = 0;
		auto  bins = m_Bins.begin();
		SizeT nrBins = m_Bins.size();
		for (V v : data)
		{
			if (!IsDefined(v))
				++nrNulls;
			else if (SizeT i = Offset(v); i < nrBins)
				++bins[i];
			else
				outside.push_back(v MG_DEBUG_ALLOCATOR_SRC("BinCounts::CountTile outside"));
		}
		if (!valueMustBeDefined)
			m_NullCount += nrNulls;
		m_NrCounted += data.size() - nrNulls - outside.size();
		if (outside.empty())
			return {};

		V lo = m_First, hi = Last();
		for (V v : outside)
		{
			MakeMin(lo, v);
			MakeMax(hi, v);
		}
		if (!BinSpan(lo, hi, maxSpan))
			return GetWeededTileCounts<V, C>(typename sequence_traits<V>::cseq_t(outside.begin(), outside.end()), 0, outside.size(), maxPairCount, true);

		Grow(lo, hi, maxSpan);
		for (V v : outside)
			++m_Bins[Offset(v)];
		m_NrCounted += outside.size();
		return {};
	}

	// Adds the counts of other if the bins of both fit in MaxBins together; returns false, changing nothing, otherwise.
	bool Absorb(BinCounts&& other, SizeT maxPairCount)
	{
		if (other.HasBins())
		{
			if (!HasBins())
			{
				m_First = other.m_First;
				m_Bins = std::move(other.m_Bins);
			}
			else
			{
				V lo = Min<V>(m_First, other.m_First), hi = Max<V>(Last(), other.Last());
				SizeT maxSpan = MaxBins(maxPairCount);
				if (!BinSpan(lo, hi, maxSpan))
					return false;
				if (lo < m_First || Last() < hi)
					Grow(lo, hi, maxSpan);
				auto bins = m_Bins.begin() + Offset(other.m_First);
				for (SizeT i = 0, n = other.m_Bins.size(); i != n; ++i)
					bins[i] += other.m_Bins[i];
			}
		}
		m_NullCount += other.m_NullCount;
		m_NrCounted += other.m_NrCounted;
		return true;
	}

	// the counts as (value, count) pairs, in the order GetCountsDirect gives them: null first
	auto ToPairs() const -> ValueCountPairContainerT<V, C>
	{
		ValueCountPairContainerT<V, C> result;
		SizeT n = (m_NullCount != C()) ? 1 : 0;
		for (C c : m_Bins)
			if (c != C())
				++n;
		result.reserve(n MG_DEBUG_ALLOCATOR_SRC("BinCounts::ToPairs"));
		if (m_NullCount != C())
			result.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("BinCounts::ToPairs") UNDEFINED_VALUE(V), m_NullCount);
		for (SizeT i = 0, e = m_Bins.size(); i != e; ++i)
			if (m_Bins[i] != C())
				result.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("BinCounts::ToPairs") V(uint_type(uint_type(m_First) + i)), m_Bins[i]);
		return result;
	}

private:
	// Makes the bins cover lo..hi, which covers them already, keeping the counts. Adds as many bins to spare as there
	// were, within maxSpan and the defined values, on the side that grew, so that tiles whose values each reach a
	// little further do not copy the bins every time.
	void Grow(V lo, V hi, SizeT maxSpan)
	{
		SizeT span = BinSpan(lo, hi, maxSpan);
		assert(span);
		SizeT room  = Min<SizeT>(m_Bins.size(), maxSpan - span);
		SizeT below = (lo < m_First) ? (Last() < hi ? room / 2 : room) : 0;
		MakeMin(below, SizeT(uint_type(uint_type(lo) - uint_type(MinValue<V>()))));
		SizeT above = Min<SizeT>(room - below, SizeT(uint_type(uint_type(MaxValue<V>()) - uint_type(hi))));
		V first = V(uint_type(uint_type(lo) - uint_type(below)));

		my_vector<C> bins(span + below + above MG_DEBUG_ALLOCATOR_SRC("BinCounts::Grow")); // zeroed
		std::copy(m_Bins.begin(), m_Bins.end(), bins.begin() + SizeT(uint_type(uint_type(m_First) - uint_type(first))));
		m_Bins = std::move(bins);
		m_First = first;
	}

	V              m_First = V();
	my_vector<C>   m_Bins;            // m_Bins[i] counts the value m_First + i
	C              m_NullCount = C(); // counted only when valueMustBeDefined is false
	SizeT          m_NrCounted = 0;   // the values counted in bins, which bounds how far they grow
};

// The counts of a range of tiles: in bins while all values fit, as sorted (value, count) pairs from then on. Turning
// to pairs where the first values do not fit, and not later, lets weeding happen at the merges it happens at without
// bins: a merge that weeds would otherwise weed some pairs once more than others.
template <ordered_value_type V, count_type C>
struct WallCounts
{
	BinCounts<V, C>                bins;
	ValueCountPairContainerT<V, C> pairs;

	void TurnToPairs(SizeT maxPairCount, bool valueMustBeDefined)
	{
		if (bins.IsEmpty())
			return;
		pairs = pairs.empty() ? bins.ToPairs() : WeededMergeToLeft(bins.ToPairs(), pairs, maxPairCount, ValueCountKeyCompare<V>(valueMustBeDefined));
		bins = {};
	}
};

template <ordered_value_type V, count_type C>
auto GetBinnedWallCounts_ST(future_tile_array<V>& values_fta, tile_id t, tile_id nrTiles, SizeT maxPairCount, bool valueMustBeDefined, BinCounts<V, C>& bins) -> ValueCountPairContainerT<V, C>
{
	if (nrTiles == 1)
	{
		auto tileData = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		return bins.CountTile(tileData, maxPairCount, valueMustBeDefined);
	}

	tile_id m = nrTiles / 2;
	assert(m >= 1);

	auto firstHalf  = GetBinnedWallCounts_ST<V, C>(values_fta, t, m, maxPairCount, valueMustBeDefined, bins);
	auto secondHalf = GetBinnedWallCounts_ST<V, C>(values_fta, t + m, nrTiles - m, maxPairCount, valueMustBeDefined, bins);

	return WeededMergeToLeft(firstHalf, secondHalf, maxPairCount, ValueCountKeyCompare<V>(valueMustBeDefined));
}

template <ordered_value_type V, count_type C>
auto GetBinnedWallCounts_MT(future_tile_array<V>& values_fta, tile_id t, tile_id nrTiles, SizeT maxPairCount, SizeT availableThreads, bool valueMustBeDefined) -> WallCounts<V, C>
{
	assert(nrTiles);
	assert(availableThreads <= nrTiles);

	WallCounts<V, C> result;
	if (availableThreads == 1)
	{
		result.pairs = GetBinnedWallCounts_ST<V, C>(values_fta, t, nrTiles, maxPairCount, valueMustBeDefined, result.bins);
		if (!result.pairs.empty())
			result.TurnToPairs(maxPairCount, valueMustBeDefined);
		return result;
	}

	auto m = nrTiles / 2;
	auto rt = availableThreads / 2;

	auto firstHalf = throttled_async([&values_fta, t, m, maxPairCount, rt, valueMustBeDefined]
		{
			return GetBinnedWallCounts_MT<V, C>(values_fta, t, m, maxPairCount, rt, valueMustBeDefined);
		}
	);

	auto secondHalf = GetBinnedWallCounts_MT<V, C>(values_fta, t + m, nrTiles - m, maxPairCount, availableThreads - rt, valueMustBeDefined);

	result = firstHalf->get();
	if (result.pairs.empty() && secondHalf.pairs.empty() && result.bins.Absorb(std::move(secondHalf.bins), maxPairCount))
		return result;

	result.TurnToPairs(maxPairCount, valueMustBeDefined);
	secondHalf.TurnToPairs(maxPairCount, valueMustBeDefined);
	result.pairs = WeededMergeToLeft(result.pairs, secondHalf.pairs, maxPairCount, ValueCountKeyCompare<V>(valueMustBeDefined));
	return result;
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

// The (value, count) pairs of all tiles, sorted: from bins for integral values, as far as they fit, merged otherwise.
template <ordered_value_type V, count_type C>
auto GetWeededWallCounts(future_tile_array<V>& values_fta, SizeT maxPairCount, bool valueMustBeDefined) -> ValueCountPairContainerT<V, C>
{
	auto nrTiles = values_fta.size();
	if (!nrTiles)
		return {};

	SizeT maxNrThreads = MaxAllowedConcurrentTreads();
	MakeMin(maxNrThreads, nrTiles);
	MakeMax(maxNrThreads, 1);

	if constexpr (is_bitvalue_v<V>)
	{
		// a bin for each of the 2^N values fits them all: bit values are never sorted, nor null
		WallCountsAsArrayInfo<V> info = { typename Unit<V>::range_t(0, 1 << nrbits_of_v<V>), SizeT(1) << nrbits_of_v<V>, values_fta.begin() };
		auto bins = GetWallCountsAsArray<V, C>(info, 0, nrTiles, maxNrThreads);

		ValueCountPairContainerT<V, C> result;
		result.reserve(bins.size() - SizeT(std::count(bins.begin(), bins.end(), C())) MG_DEBUG_ALLOCATOR_SRC("GetWeededWallCounts"));
		for (SizeT i = 0, n = bins.size(); i != n; ++i)
			if (bins[i] != C())
				result.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("GetWeededWallCounts") V(typename V::base_type(i)), bins[i]);
		return result;
	}
	else if constexpr (is_integral_v<V>)
	{
		auto counts = GetBinnedWallCounts_MT<V, C>(values_fta, 0, nrTiles, maxPairCount, maxNrThreads, valueMustBeDefined);
		counts.TurnToPairs(maxPairCount, valueMustBeDefined);
		return std::move(counts.pairs);
	}
	else
		return GetWeededWallCounts_MT<V, C>(values_fta, 0, nrTiles, maxPairCount, maxNrThreads, valueMustBeDefined);
}

template <ordered_value_type R, typename V, count_type C>
auto GetWeededCountsOfV(const DataArray<V>* valuesTF, SizeT maxPairCount) -> ValueCountPairContainerT<R, C>
{
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

template <ordered_value_type R, typename TypeList, count_type C>
auto GetWeededCounts_Impl(const AbstrDataItem* adi, SizeT maxPairCount) -> ValueCountPairContainerT<R, C>
{
	return visit_and_return_result<TypeList, ValueCountPairContainerT<R, C> >(adi->GetAbstrValuesUnit()
		, [adi, maxPairCount]<typename V>(const Unit<V>*)
			{
				auto tileFunctor = const_array_cast<V>(adi);
				return GetWeededCountsOfV<R, V, C>(tileFunctor, maxPairCount);
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
