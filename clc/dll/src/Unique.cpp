// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "vt/IsNotUndef.h"
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

#include "OperSignature.h"

#include "rlookup.h"
#include "ValuesTable.h"
#include "mem/MyContainers.h"

template <typename V> const UInt32 UNIQUE_BUFFER_SIZE = 4096 / sizeof(V);

// The unique values of a part of the elements, sorted. Bit values all fit in a table of their 2^N values; other values are
// sorted and merged in blocks, see block_vector, so that a merge frees what it has read.
template <typename V> using unique_values_t = std::conditional_t<is_bitvalue_v<V>, my_elem_vec_t<V>, block_vector<V>>;

template <typename V>
block_vector<V> GetUniqueValuesDirect(typename DataArray<V>::locked_cseq_t seq, tile_offset index, tile_offset size, bool mustBeDefined)
{
	// PRECONDITIONS
	assert(size <= UNIQUE_BUFFER_SIZE<V>);
	assert(size > 0);
	assert(index <= seq.size());
	assert(index + size <= seq.size());

	V buffer[UNIQUE_BUFFER_SIZE<V>];
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

				auto areEqual = [](auto const& a, auto const& b) {  return a == b || (!IsDefined(a) && !IsDefined(b)); };
				bufferCursor = std::unique_copy(i, e, buffer, areEqual);
				// use the ordering that handles null well
				std::sort(buffer, bufferCursor, cmp);
				bufferCursor = std::unique(buffer, bufferCursor, areEqual);
				return block_vector<V>(buffer, bufferCursor);
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
				return block_vector<V>(buffer, bufferCursor);
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
	return block_vector<V>(buffer, bufferCursor);
}

// The union of the sorted values of first and second, sorted, taking first's value of two equal ones. It reads both block
// by block and frees each block once read, so that it holds the values about once, not the inputs and the result together.
template <typename V, typename Pred>
block_vector<V> set_union_by_move(block_vector<V>&& first, block_vector<V>&& second, Pred pred)
{
	if (first.empty())
		return std::move(second);
	if (second.empty())
		return std::move(first);

	block_vector<V> result;
	result.reserve(first.size() + second.size());

	block_vector_reader<V> first1(std::move(first)), first2(std::move(second));
	while (!first1.AtEnd() && !first2.AtEnd())
	{
		if (pred(*first1, *first2)) { // copy first
			result.push_back(std::move(*first1));
			++first1;
		}
		else if (pred(*first2, *first1)) { // copy second
			result.push_back(std::move(*first2));
			++first2;
		}
		else { // advance both
			result.push_back(std::move(*first1));
			++first1;
			++first2;
		}
	}
	first1.MoveRestTo(result);
	first2.MoveRestTo(result);
	return result;
}

template <typename V>
block_vector<V> MergeToLeft(block_vector<V>&& left, block_vector<V>&& right, bool mustBeDefined)
{
	if constexpr (compare_must_check_undefines_v<V>)
		if (!mustBeDefined)
			return set_union_by_move(std::move(right), std::move(left), DataLessThanCompareImpl<V, true>());
	return set_union_by_move(std::move(right), std::move(left), std::less<void>());
}

template <typename V>
block_vector<V> GetTileUniqueValues(typename DataArray<V>::locked_cseq_t tileData, tile_offset index, tile_offset size, bool mustBeDefined)
{
	if (size <= UNIQUE_BUFFER_SIZE<V>)
		return GetUniqueValuesDirect<V>(tileData, index, size, mustBeDefined);

	tile_offset m = size / 2;

	auto firstHalf = throttled_async([&tileData, index, m, mustBeDefined]() {
		return GetTileUniqueValues<V>(tileData, index, m, mustBeDefined);
		}
	);

	auto secondHalf = GetTileUniqueValues<V>(tileData, index + m, size - m, mustBeDefined);

	return MergeToLeft<V>(firstHalf->get(), std::move(secondHalf), mustBeDefined);
}

template <typename V>
block_vector<V> GetUniqueWallValues(const DataArray<V>* ado, tile_id t, tile_id nrTiles, bool mustBeDefined)
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

// *****************************************************************************
//                         Unique integral values, in bins
// *****************************************************************************

// A tile whose integral values all fit in the bins of its worker is counted there (see BinCounts in ValuesTable.h);
// any other tile is sorted as above, still split over threads. Bit values all fit in a table of their 2^N values.

template <typename V>
struct BinnedUniqueValues
{
	BinCounts<V, SizeT> bins;
	block_vector<V>     values; // of the tiles that did not fit in the bins, sorted
};

template <typename V>
block_vector<V> BinnedValues(const BinCounts<V, SizeT>& bins)
{
	auto counts = bins.ToPairs(); // null first, when counted, as MergeToLeft orders it
	block_vector<V> result;
	result.reserve(counts.size());
	for (const auto& valueCount : counts)
		result.push_back(valueCount.first);
	return result;
}

template <typename V>
block_vector<V> GetBinnedUniqueWallValues_ST(const DataArray<V>* ado, tile_id t, tile_id nrTiles, bool mustBeDefined, BinCounts<V, SizeT>& bins)
{
	if (nrTiles == 1)
	{
		auto tileData = ado->GetTile(t);
		if (bins.CountTileIfItFits(tileData, SizeT(-1), mustBeDefined))
			return {};
		auto tileSize = tileData.size();
		return GetTileUniqueValues<V>(std::move(tileData), 0, tileSize, mustBeDefined);
	}

	tile_id m = nrTiles / 2;
	dms_assert(m >= 1);

	auto firstHalf  = GetBinnedUniqueWallValues_ST<V>(ado, t, m, mustBeDefined, bins);
	auto secondHalf = GetBinnedUniqueWallValues_ST<V>(ado, t + m, nrTiles - m, mustBeDefined, bins);

	return MergeToLeft<V>(std::move(firstHalf), std::move(secondHalf), mustBeDefined);
}

template <typename V>
auto GetBinnedUniqueWallValues_MT(const DataArray<V>* ado, tile_id t, tile_id nrTiles, SizeT availableThreads, bool mustBeDefined) -> BinnedUniqueValues<V>
{
	dms_assert(availableThreads <= nrTiles);

	BinnedUniqueValues<V> result;
	if (availableThreads == 1)
	{
		result.values = GetBinnedUniqueWallValues_ST<V>(ado, t, nrTiles, mustBeDefined, result.bins);
		return result;
	}

	auto m = nrTiles / 2;
	auto rt = availableThreads / 2;

	auto firstHalf = throttled_async([ado, t, m, rt, mustBeDefined]() {
		return GetBinnedUniqueWallValues_MT<V>(ado, t, m, rt, mustBeDefined);
		});

	auto secondHalf = GetBinnedUniqueWallValues_MT<V>(ado, t + m, nrTiles - m, availableThreads - rt, mustBeDefined);

	result = firstHalf->get();
	result.values = MergeToLeft<V>(std::move(result.values), std::move(secondHalf.values), mustBeDefined);
	if (!result.bins.Absorb(std::move(secondHalf.bins), SizeT(-1)))
		result.values = MergeToLeft<V>(std::move(result.values), BinnedValues<V>(secondHalf.bins), mustBeDefined);
	return result;
}

template <typename V>
unique_values_t<V> GetBinnedUniqueWallValues(const DataArray<V>* ado, tile_id nrTiles, bool mustBeDefined)
{
	if constexpr (is_bitvalue_v<V>)
	{
		auto values_fta = GetFutureTileArray(ado);
		V buffer[1 << nrbits_of_v<V>];
		V* bufferCursor = buffer;
		for (const auto& valueCount : GetWeededWallCounts<V, SizeT>(values_fta, SizeT(-1), mustBeDefined))
			*bufferCursor++ = valueCount.first;
		return my_elem_vec_t<V>(buffer, bufferCursor);
	}
	else
	{
		SizeT maxNrThreads = MaxAllowedConcurrentTreads();
		MakeMin(maxNrThreads, nrTiles);
		MakeMax(maxNrThreads, 1);

		auto uniqueValues = GetBinnedUniqueWallValues_MT<V>(ado, 0, nrTiles, maxNrThreads, mustBeDefined);
		return MergeToLeft<V>(BinnedValues<V>(uniqueValues.bins), std::move(uniqueValues.values), mustBeDefined);
	}
}


// Gives resSub, whose domain has the count of values, the values as its tiles. A block that holds exactly the elements of
// a tile becomes that tile; anything else is copied, and each block is freed once it is read. A domain sized by SetCount
// has tiles of 2^16 elements, as many as a block of values has, so that the values are neither copied nor held twice.
template <typename V>
void WriteBlocksAsTiles(AbstrDataItem* resSub, block_vector<V>&& values)
{
	DataWriteLock resLock(resSub);
	auto resArray = mutable_array_cast<V>(resLock);
	auto trd = resArray->GetTiledRangeData();
	block_vector_reader<V> reader(std::move(values));
	for (tile_id t = 0, tn = trd->GetNrTiles(); t != tn; ++t)
	{
		SizeT tileSize = trd->GetTileSize(t);
		auto resTile = std::make_shared<tile<V>>(); // before the block is taken, so that it always has an owner
		if (V* block = reader.TakeBlockIfWhole(tileSize))
		{
			resTile->reset(block, tileSize);
			resArray->AdoptTile(t, std::move(resTile));
			continue;
		}
		auto resData = resArray->GetWritableTile(t, dms_rw_mode::write_only_all);
		for (auto out = resData.begin(); out != resData.end(); )
		{
			MG_CHECK(!reader.AtEnd());
			SizeT n = Min<SizeT>(resData.end() - out, reader.BlockEnd() - reader.Curr());
			out = fast_copy(reader.Curr(), reader.Curr() + n, out);
			reader.Consume(n);
		}
	}
	assert(reader.AtEnd());
	resLock.Commit();
}

template<fixed_elem V>
void GetUniqueValues(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* adi, bool mustBeDefined)
{
	dms_assert(adi && adi->GetInterestCount());

	const DataArray<V>* ado = const_array_cast<V>(adi);
	unique_values_t<V> values;
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
			{
				if constexpr (is_integral_v<V>)
					values = GetBinnedUniqueWallValues<V>(ado, tn, mustBeDefined);
				else
					values = GetUniqueWallValues<V>(ado, 0, tn, mustBeDefined);
			}
		}
	}

	res->SetCount(values.size());

	if constexpr (is_bitvalue_v<V>)
	{
		locked_tile_write_channel<V> resWriter(resSub);
		resWriter.Write(values.begin(), values.end());
		dms_assert(resWriter.IsEndOfChannel());
		resWriter.Commit();
	}
	else
		WriteBlocksAsTiles<V>(resSub, std::move(values));
}

// *****************************************************************************
//                         Unique strings and other sequences
// *****************************************************************************

// A sequence is referred to by an element that has it: its tile and its position there. The references of each part of a
// tile are sorted by the sequences they refer to, those of the parts are merged as the values above, and only the sorted
// references of the distinct sequences are kept, in blocks that the merges free as they read. The sequences stay in the
// tiles of the argument, which remain locked until the result is written, so that neither they nor an index of all
// elements are copied. Of equal sequences the one that comes first in the argument is kept, as the sort of all elements in
// their order did.

struct elem_ref
{
	tile_id     t;
	tile_offset i;
};

template <typename V>
struct SequenceTiles
{
	using locked_cseq_t  = typename DataArray<V>::locked_cseq_t;
	using const_iterator = typename DataArrayBase<V>::const_iterator;
	using value_type     = typename std::iterator_traits<const_iterator>::value_type;

	SequenceTiles(const DataArray<V>* ado, bool mustBeDefined)
		: m_Data(ado)
		, m_Tiles(ado->GetTiledRangeData()->GetNrTiles())
		, m_TileBegins(m_Tiles.size())
		, m_MustBeDefined(mustBeDefined)
	{}

	// locks tile t, by the one task that sorts it, and returns its size
	tile_offset Lock(tile_id t)
	{
		m_Tiles[t] = m_Data->GetTile(t);
		m_TileBegins[t] = m_Tiles[t].begin();
		SizeT n = m_Tiles[t].size();
		MG_CHECK(n == tile_offset(n));
		return n;
	}
	auto Elem(elem_ref r) const { return m_TileBegins[r.t][r.i]; }

	const DataArray<V>*         m_Data;
	std::vector<locked_cseq_t>  m_Tiles;
	std::vector<const_iterator> m_TileBegins;
	bool                        m_MustBeDefined;
};

// the sorted references of the distinct sequences of positions index .. index+size-1 of tile t
template <typename V, typename Comp>
block_vector<elem_ref> GetUniqueSequencesDirect(const SequenceTiles<V>& tiles, tile_id t, tile_offset index, tile_offset size, Comp comp)
{
	assert(size <= UNIQUE_BUFFER_SIZE<tile_offset>);
	tile_offset buffer[UNIQUE_BUFFER_SIZE<tile_offset>];
	auto data = tiles.m_TileBegins[t];
	tile_offset* bufferEnd = buffer;
	for (tile_offset i = index, e = index + size; i != e; ++i)
		if (!tiles.m_MustBeDefined || IsDefined(data[i]))
			*bufferEnd++ = i;

	std::stable_sort(buffer, bufferEnd, [&](tile_offset a, tile_offset b) { return comp(data[a], data[b]); });
	bufferEnd = std::unique(buffer, bufferEnd, [&](tile_offset a, tile_offset b) { return !comp(data[a], data[b]); }); // keeps the first

	block_vector<elem_ref> result;
	result.reserve(bufferEnd - buffer);
	for (tile_offset* i = buffer; i != bufferEnd; ++i)
		result.push_back(elem_ref{ t, *i });
	return result;
}

// The union of left and right, sorted references that each refer to distinct sequences, with left coming first in the
// argument: of two that refer to equal sequences, the one of left. It frees both inputs block by block as it reads them.
template <typename V, typename Comp>
block_vector<elem_ref> MergeUniqueSequences(block_vector<elem_ref>&& left, block_vector<elem_ref>&& right, const SequenceTiles<V>& tiles, Comp comp)
{
	if (left.empty())
		return std::move(right);
	if (right.empty())
		return std::move(left);

	auto less = [&](elem_ref a, elem_ref b) { return comp(tiles.Elem(a), tiles.Elem(b)); };
	block_vector<elem_ref> result;
	result.reserve(left.size() + right.size());
	block_vector_reader<elem_ref> leftReader(std::move(left)), rightReader(std::move(right));
	while (!leftReader.AtEnd() && !rightReader.AtEnd())
	{
		if (less(*rightReader, *leftReader))
		{
			result.push_back(*rightReader);
			++rightReader;
			continue;
		}
		if (!less(*leftReader, *rightReader)) // equal
			++rightReader;
		result.push_back(*leftReader);
		++leftReader;
	}
	leftReader.MoveRestTo(result);
	rightReader.MoveRestTo(result);
	return result;
}

template <typename V, typename Comp>
block_vector<elem_ref> GetTileUniqueSequences(const SequenceTiles<V>& tiles, tile_id t, tile_offset index, tile_offset size, Comp comp)
{
	if (size <= UNIQUE_BUFFER_SIZE<tile_offset>)
		return GetUniqueSequencesDirect<V>(tiles, t, index, size, comp);

	tile_offset m = size / 2;

	auto firstHalf = throttled_async([&tiles, t, index, m, comp]() {
		return GetTileUniqueSequences<V>(tiles, t, index, m, comp);
		}
	);

	auto secondHalf = GetTileUniqueSequences<V>(tiles, t, index + m, size - m, comp);

	return MergeUniqueSequences<V>(firstHalf->get(), std::move(secondHalf), tiles, comp);
}

template <typename V, typename Comp>
block_vector<elem_ref> GetUniqueSequenceRefs(SequenceTiles<V>& tiles, tile_id t, tile_id nrTiles, Comp comp)
{
	assert(nrTiles >= 1);
	if (nrTiles == 1)
		return GetTileUniqueSequences<V>(tiles, t, 0, tiles.Lock(t), comp);

	tile_id m = nrTiles / 2;

	auto firstHalf = throttled_async([&tiles, t, m, comp]() {
		return GetUniqueSequenceRefs<V>(tiles, t, m, comp);
		});

	auto secondHalf = GetUniqueSequenceRefs<V>(tiles, t + m, nrTiles - m, comp);

	return MergeUniqueSequences<V>(firstHalf->get(), std::move(secondHalf), tiles, comp);
}

template<sequence_or_string V>
void GetUniqueValues(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* adi, bool mustBeDefined)
{
	SequenceTiles<V> tiles(const_array_cast<V>(adi), mustBeDefined);
	block_vector<elem_ref> refs;
	if (tile_id tn = tile_id(tiles.m_Tiles.size()))
	{
		// as the sort of all elements did: operator< where nulls are left out, an order that puts null first otherwise
		if (mustBeDefined)
			refs = GetUniqueSequenceRefs<V>(tiles, 0, tn, std::less<void>());
		else
			refs = GetUniqueSequenceRefs<V>(tiles, 0, tn, DataLessThanCompare<typename SequenceTiles<V>::value_type>());
	}

	res->SetCount(refs.size());

	locked_tile_write_channel<V> resWriter(resSub);
	for (block_vector_reader<elem_ref> ref(std::move(refs)); !ref.AtEnd(); ++ref) // frees the references once written
		resWriter.Write(tiles.Elem(*ref));
	assert(resWriter.IsEndOfChannel());
	resWriter.Commit();
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

static StaticLateTokenID s_Values("Values");

class AbstrUniqueOperator : public UnaryOperator
{
public:
	// Override Operator
	AbstrUniqueOperator(ClassCPtr argCls, AbstrOperGroup& og, const UnitClass* resDomainCls, bool mustBeDefined)
		: UnaryOperator(&og, AbstrUnit::GetStaticClass(), argCls) 
		, m_ResDomainClass(resDomainCls)
		, m_MustBeDefined(mustBeDefined)
	{}

	// K6: unique(values: attribute<V>(D)) -> a FRESH unit U [new]. The unit's
	// own identity is existential (GeneratedUnit -> a fresh flexible node per
	// application, deduplicated across repeated occurrences by the walker's
	// LispPtr memo); its value class is crd(D), knowable at definition only for
	// the typed unique_uintN groups (m_ResDomainClass fixed), unconstrained for
	// the dynamic-result-class cog_unique. §12.7 slSubItemCall tranche: the ONE
	// sub-item CreateResult makes -- Values : attribute<V>(U), the argument's
	// composition -- is a described, complete member set: unique(x)/Values types
	// at definition (values class linked through V; identity per application)
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		auto argCls = dynamic_cast<const DataItemClass*>(GetArgClass(0));
		if (!argCls)
			return false;
		sig_var V = sb.UnitVar("V"), D = sb.UnitVar("D"), U = sb.GeneratedUnit("U");
		sb.MemberValueClass(V, argCls->GetValuesType());
		if (m_ResDomainClass)
			if (auto rvt = m_ResDomainClass->GetValueType())
				sb.MemberValueClass(U, rvt);
		sb.ArgName(0, "values");
		sb.ArgAttr(0, V, D, argCls->GetValuesType()->GetValueComposition());
		sb.ResultUnit(U);
		sb.ResultContainerMember("Values", V, U, argCls->GetValuesType()->GetValueComposition());
		sb.ResultMembersComplete();
		return true;
	}

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

		auto res_owner = resDomainCls->CreateResultUnit(resultHolder.GetNew()); AbstrUnit* res = res_owner.get();
		assert(res);
		res->SetTSF(TSF_Categorical);
		resultHolder = res;

		AbstrDataItem* resSub = CreateDataItem(res, s_Values, res, arg1Values, arg1->GetValueComposition() ).get(); // owned by res
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



