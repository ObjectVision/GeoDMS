// Copyright (C) 2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// pareto_optimal(partition_rel, crit1, ..., critN): per row of the domain E of the
// arguments, true iff the row is Pareto-optimal within its partition on the N criteria,
// all of which are minimised. A row is dominated when another row of the same partition
// is less than or equal on every criterion and either strictly less on one of them or,
// when equal on all of them, earlier (a lower row id): exact duplicates keep their
// first occurrence, the weak-dominance rule with the id tie-break that the
// join_equal_values formulation of NetworkModel_PBL used. Issue #1281.
//
// Offline form of the label-setting rule of the pareto option of impedance_matrix
// (doc/bicriteria-impedance.md section 3): sort the rows lexicographically on
// (partition, crit1, ..., critN, id), the order in which the bi-criteria Dijkstra pops
// its labels, and sweep. Every earlier row of the group then has crit1 <= the row under
// test, so with two criteria the test collapses to one scalar per group, the minimum
// crit2 accepted so far (Hansen 1980), and with more criteria to a comparison against the
// rows accepted so far only: a row rejected earlier was dominated by an accepted row
// that, by transitivity, dominates whatever the rejected row would have dominated.
// n log n for the sort plus n times the front size for the sweep, instead of the k^2
// pairs per group of a self-join.
//
// pareto_optimal_eps(partition_rel, crit1, eps1, ..., critN, epsN): the same with
// epsilon-dominance (#1282, the offline form of pareto(imp2_epsilon)). Every criterion is
// bucketed to floor(crit / eps) (eps 0: exact), the sort is on (partition, bucket1, ...,
// bucketN, crit1, ..., critN, id) and the sweep compares buckets. Per box the row with the
// smallest raw values in criterion order survives: eps1 decides which rows count as equal
// on the first criterion, and among those the lower bucket of the second criterion wins,
// then the smaller raw first criterion. The result is a subset of the exact result, and
// every exact-optimal row that drops out has a surviving row of its partition that is
// less than eps better or equal on every criterion (its bucket is <= on all of them).
//
// A criterion may also be bool (#1287), minimised like the others: false before true, so a
// criterion where true is better is written not(x). A bool criterion has no epsilon, which
// makes pareto_optimal_eps read its arguments by type: a numeric criterion is followed by its
// epsilon, a bool criterion is not, as in pareto_optimal_eps(part, price, 10.0, needsCar,
// needsBike). The same number of arguments can therefore be read in more than one way (five:
// two numeric criteria, or one numeric and two bool ones), so there is one operator per number
// of arguments, and the criteria read from them may be at most eight. In the sort and the sweep
// a bool criterion is a criterion of 0 and 1 with epsilon 0.
//
// The criteria may have different value types, numeric or bool; they are compared as Float64.
// The partition relation is any attribute to a domain unit. A row with an undefined partition
// or an undefined (or NaN) criterion is never optimal and dominates nothing; a bool, uint2 or
// uint4 criterion is never undefined.
//
// Groups are independent, so the work is split by group (#1286). The rows are first brought
// together per partition: (partition, row) pairs, collected in row order and sorted with a
// stable radix sort on the partition only, so that the rows of a group stay in ascending
// order; a partition that is already ascending needs no sort at all. Then, per block of
// whole groups and in parallel, every group copies its keys and raw criteria into one
// contiguous record per row, sorts those records and sweeps them. The order within a group
// is the one given above, and the row id breaks the last tie, so the result does not depend
// on how the groups are numbered or ordered. The per-row passes (reading the arguments,
// collecting the pairs, writing the result) run in parallel per chunk of rows.

#include <algorithm>
#include <cmath>
#include <deque>
#include <execution>
#include <limits>
#include <memory>

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "ptr/OwningPtrSizedArray.h"
#include "set/VectorFunc.h"
#include "mem/MyContainers.h"
#include "utl/StrFormat.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "IndexGetterCreator.h"
#include "ParallelTiles.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "OperRelUni.h"

CommonOperGroup cog_pareto_optimal("pareto_optimal");
CommonOperGroup cog_pareto_optimal_eps("pareto_optimal_eps");

namespace pareto_impl
{
	// rows per work item of the per-row passes; a multiple of the 32-bit block of a Bool tile,
	// so that the chunks of one result tile never write the same block
	constexpr SizeT CHUNK_SIZE = SizeT(1) << 16;

	// rows per block of whole groups in the sweep; a group larger than this is a block of its own
	constexpr SizeT GROUP_BLOCK_SIZE = SizeT(1) << 16;

	// a group of at least this many rows sorts its records with std::execution::par
	constexpr SizeT LARGE_GROUP_SIZE = SizeT(1) << 18;

	// the most criteria of one call; pareto_optimal has an operator for each number of arguments up to
	// 1 + MAX_CRITERIA, pareto_optimal_eps up to 1 + 2 * MAX_CRITERIA and checks the number it reads
	constexpr arg_index MAX_CRITERIA = 8;

	// calls chunkFunc(c, first, last) for the chunks [first, last) of [0, count), in parallel
	template <typename ChunkFunc>
	void ForEachChunk(SizeT count, ChunkFunc&& chunkFunc)
	{
		SizeT nrChunks = (count + CHUNK_SIZE - 1) / CHUNK_SIZE;
		parallel_for<SizeT>(nrChunks, [count, &chunkFunc](SizeT c)
			{
				SizeT first = c * CHUNK_SIZE;
				chunkFunc(c, first, std::min(count, first + CHUNK_SIZE));
			}
		);
	}

	// one criterion of one tile, read as Float64; holds the tile for the chunks that read it
	struct AbstrCritTileReader
	{
		virtual ~AbstrCritTileReader() {}
		virtual void Read(SizeT first, SizeT count, Float64* out) const = 0;
	};

	template <typename V>
	struct CritTileReader : AbstrCritTileReader
	{
		CritTileReader(typename DataArray<V>::locked_cseq_t&& data)
			: m_Data(std::move(data))
		{}

		void Read(SizeT first, SizeT count, Float64* out) const override
		{
			assert(first + count <= m_Data.size());
			auto dataPtr = m_Data.begin() + first;
			for (SizeT i = 0; i != count; ++i, ++dataPtr)
			{
				V v = *dataPtr;
				out[i] = IsBitValueOrDefined(v) ? Float64(v) : UNDEFINED_VALUE(Float64); // a bit value (bool: 0 or 1) is always defined
			}
		}

		typename DataArray<V>::locked_cseq_t m_Data;
	};

	auto CreateCritTileReader(const AbstrDataItem* critA, tile_id t, SizeT tileSize) -> std::unique_ptr<AbstrCritTileReader>
	{
		std::unique_ptr<AbstrCritTileReader> result;
		visit<typelists::numerics>(critA->GetAbstrValuesUnit(), [critA, t, tileSize, &result] <typename V> (const Unit<V>*)
			{
				auto tileData = const_array_cast<V>(critA)->GetTile(t);
				MG_CHECK(tileData.size() == tileSize); // the criteria share the tiling of the partition relation
				result = std::make_unique<CritTileReader<V>>(std::move(tileData));
			}
		);
		MG_CHECK(result);
		return result;
	}

	struct PartRow
	{
		SizeT part;
		SizeT row;
	};

	// Stable LSD radix sort of the pairs on their partition, RADIX_BITS per pass and only as many
	// passes as the spread maxPart - minPart needs, with the chunk histograms and the scatter in
	// parallel. Stability keeps the rows of a group in the ascending order in which they were
	// collected, so no row id has to be compared.
	void SortOnPartition(OwningPtrSizedArray<PartRow>& pairs, SizeT minPart, SizeT maxPart)
	{
		constexpr UInt32 RADIX_BITS = 11;
		constexpr SizeT  RADIX = SizeT(1) << RADIX_BITS;

		UInt32 nrPasses = 0;
		for (SizeT spread = maxPart - minPart; spread; spread >>= RADIX_BITS)
			++nrPasses;
		if (!nrPasses)
			return;

		const SizeT m = pairs.size();
		const SizeT nrChunks = (m + CHUNK_SIZE - 1) / CHUNK_SIZE;
		OwningPtrSizedArray<PartRow> buffer(m, dont_initialize MG_DEBUG_ALLOCATOR_SRC("pareto_optimal: radix buffer"));
		OwningPtrSizedArray<SizeT>   offsets(nrChunks * RADIX, dont_initialize MG_DEBUG_ALLOCATOR_SRC("pareto_optimal: radix offsets"));

		PartRow* src = pairs.begin();
		PartRow* dst = buffer.begin();
		for (UInt32 pass = 0; pass != nrPasses; ++pass)
		{
			const UInt32 shift = RADIX_BITS * pass;
			auto digit = [minPart, shift](const PartRow& pr) { return ((pr.part - minPart) >> shift) & (RADIX - 1); };

			ForEachChunk(m, [src, &offsets, &digit](SizeT c, SizeT first, SizeT last)
				{
					SizeT* hist = offsets.begin() + c * RADIX;
					std::fill(hist, hist + RADIX, SizeT(0));
					for (SizeT i = first; i != last; ++i)
						++hist[digit(src[i])];
				}
			);

			// exclusive offsets per (digit, chunk), digit-major: the chunks of one digit keep their order
			SizeT sum = 0;
			bool allOneDigit = false;
			for (SizeT b = 0; b != RADIX; ++b)
			{
				SizeT digitCount = 0;
				for (SizeT c = 0; c != nrChunks; ++c)
				{
					SizeT& cell = offsets[c * RADIX + b];
					SizeT count = cell;
					cell = sum;
					sum += count;
					digitCount += count;
				}
				if (digitCount == m)
					allOneDigit = true;
			}
			assert(sum == m);
			if (allOneDigit)
				continue; // this pass would only copy

			ForEachChunk(m, [src, dst, &offsets, &digit](SizeT c, SizeT first, SizeT last)
				{
					SizeT* offset = offsets.begin() + c * RADIX;
					for (SizeT i = first; i != last; ++i)
						dst[offset[digit(src[i])]++] = src[i];
				}
			);
			std::swap(src, dst);
		}
		if (src != pairs.begin())
			pairs.swap(buffer);
	}
}

struct ParetoOptimalOperator : VariadicOperator
{
	// The result class is the concrete DataArray<Bool>: the operator lookup of an expression that
	// uses the result (a != on it, a uint32() of it) matches on that class before anything is calculated.
	// withEps: a numeric criterion is followed by its epsilon, a bool criterion is not (#1287).
	ParetoOptimalOperator(arg_index nrArgs, bool withEps)
		: VariadicOperator(withEps ? &cog_pareto_optimal_eps : &cog_pareto_optimal, DataArray<Bool>::GetStaticClass(), nrArgs)
		, m_WithEps(withEps)
	{
		fast_fill(m_ArgClasses.get(), m_ArgClasses.get() + nrArgs, AbstrDataItem::GetStaticClass());
	}

	CharPtr Name() const { return m_WithEps ? "pareto_optimal_eps" : "pareto_optimal"; }

	// the arguments of a criterion and of its epsilon; NO_EPS when it has none, as every criterion of
	// pareto_optimal and every bool criterion of pareto_optimal_eps
	static constexpr arg_index NO_EPS = 0;
	struct CritArgs { arg_index crit, eps; };

	// the criteria in argument order, read by type: in pareto_optimal_eps a numeric criterion takes the
	// next argument as its epsilon and a bool criterion does not
	auto GetCritArgs(const ArgSeqType& args) const -> std::vector<CritArgs>
	{
		std::vector<CritArgs> result;
		for (arg_index i = 1; i != args.size(); )
		{
			const SizeT k = result.size() + 1; // the number of the criterion in the messages
			const AbstrDataItem* critA = AsDataItem(args[i]);
			MG_USERCHECK2(critA, mySSPrintF("{}: argument {} must be a numeric or bool attribute (criterion {})", Name(), i + 1, k).c_str());
			const ValueClass* vc = critA->GetAbstrValuesUnit()->GetValueType();
			MG_USERCHECK2(vc->IsNumericOrBool(), mySSPrintF("{}: criterion {} (argument {}) must be numeric or bool", Name(), k, i + 1).c_str());
			CritArgs critArgs{ i++, NO_EPS };
			if (m_WithEps && vc->GetValueClassID() != ValueClassID::VT_Bool)
			{
				MG_USERCHECK2(i != args.size(), mySSPrintF("{}: criterion {} (argument {}) is numeric and must be followed by its epsilon; only a bool criterion has none", Name(), k, critArgs.crit + 1).c_str());
				critArgs.eps = i++;
			}
			result.emplace_back(critArgs);
		}
		MG_USERCHECK2(result.size() <= pareto_impl::MAX_CRITERIA, mySSPrintF("{}: {} criteria given, at most {} are supported", Name(), result.size(), pareto_impl::MAX_CRITERIA).c_str());
		return result;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		const AbstrDataItem* partA = AsDataItem(args[0]);
		MG_USERCHECK2(partA, mySSPrintF("{}: the first argument must be the partition relation, an attribute to a domain unit", Name()).c_str());
		const AbstrUnit* e = partA->GetAbstrDomainUnit();
		MG_USERCHECK2(partA->GetAbstrValuesUnit()->CanBeDomain(), mySSPrintF("{}: the first argument must be a relation to a domain unit (unsigned integer values)", Name()).c_str());

		const std::vector<CritArgs> critArgs = GetCritArgs(args);
		const arg_index nrCriteria = arg_index(critArgs.size());
		for (arg_index k = 0; k != nrCriteria; ++k)
		{
			const AbstrDataItem* critA = AsDataItem(args[critArgs[k].crit]);
			e->UnifyDomain(critA->GetAbstrDomainUnit(), "e1", mySSPrintF("e{}", critArgs[k].crit + 1).c_str(), UM_Throw);
			if (critArgs[k].eps != NO_EPS)
			{
				const AbstrDataItem* epsA = AsDataItem(args[critArgs[k].eps]);
				MG_USERCHECK2(epsA && epsA->HasVoidDomainGuarantee() && epsA->GetAbstrValuesUnit()->GetValueType()->IsNumeric()
				,	mySSPrintF("{}: argument {} must be a numeric parameter, the epsilon of criterion {} (argument {}): a numeric criterion is followed by its epsilon, a bool criterion is not"
					,	Name(), critArgs[k].eps + 1, k + 1, critArgs[k].crit + 1
					).c_str()
				);
			}
		}

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, Unit<Bool>::GetStaticClass()->CreateDefault());

		if (mustCalc)
		{
			std::deque<DataReadLock> argLocks;
			for (arg_index i = 0; i != args.size(); ++i)
				argLocks.emplace_back(AsDataItem(args[i]));

			std::vector<const AbstrDataItem*> critItems(nrCriteria);
			std::vector<Float64> eps(nrCriteria, 0.0); // 0: exact, as for every bool criterion
			for (arg_index k = 0; k != nrCriteria; ++k)
			{
				critItems[k] = AsDataItem(args[critArgs[k].crit]);
				if (critArgs[k].eps != NO_EPS)
				{
					eps[k] = AsDataItem(args[critArgs[k].eps])->GetCurrRefObj()->GetValueAsFloat64(0); // argLocks holds it; GetRefObj is meta-thread only and this may run on a worker
					MG_USERCHECK2(IsDefined(eps[k]) && eps[k] >= 0.0, mySSPrintF("{}: the epsilon of criterion {} must be a defined, nonnegative value", Name(), k + 1).c_str());
				}
			}

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_all);

			Calculate(mutable_array_cast<Bool>(resLock), partA, critItems, eps);

			resLock.Commit();
		}
		return true;
	}

	static void Calculate(DataArray<Bool>* res, const AbstrDataItem* partA, const std::vector<const AbstrDataItem*>& critItems, const std::vector<Float64>& eps)
	{
		using namespace pareto_impl;

		const AbstrUnit* e = partA->GetAbstrDomainUnit();
		const SizeT     n  = e->GetCount();
		const tile_id   nt = e->GetNrTiles();
		const arg_index d  = critItems.size();
		assert(eps.size() == d);

		// the chunks of the per-row passes, numbered tile by tile
		std::vector<SizeT> tileChunkBase(nt + 1, 0);
		for (tile_id t = 0; t != nt; ++t)
			tileChunkBase[t + 1] = tileChunkBase[t] + (e->GetTileSize(t) + CHUNK_SIZE - 1) / CHUNK_SIZE;
		const SizeT nrChunks = tileChunkBase[nt];

		struct ChunkInfo
		{
			SizeT rowFirst = 0, rowLast = 0; // the rows of the chunk
			SizeT count = 0;                 // the rows that take part
			SizeT firstPart = 0, lastPart = 0, minPart = 0, maxPart = 0;
			bool  ascending = true;          // the partition does not decrease within the chunk
		};
		std::vector<ChunkInfo> chunkInfo(nrChunks);

		// 1. per chunk of rows, in parallel: the criteria as Float64 columns, whatever their numeric or
		//    bool value types, and the partition of the rows that take part, those with a defined partition
		//    and defined criteria; UNDEFINED for the others
		OwningPtrSizedArray<SizeT> part(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("pareto_optimal: partition"));
		std::vector<OwningPtrSizedArray<Float64>> crit;
		crit.reserve(d);
		std::vector<Float64*> critCol(d);
		for (arg_index k = 0; k != d; ++k)
		{
			crit.emplace_back(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("pareto_optimal: criterion"));
			critCol[k] = crit.back().begin();
		}

		parallel_for<SizeT>(nt, [&](SizeT tt)
			{
				tile_id t = tile_id(tt);
				const SizeT tileFirst = e->GetTileFirstIndex(t);
				const SizeT tileSize  = e->GetTileSize(t);
				std::unique_ptr<IndexGetter> partGetter(IndexGetterCreator::Create(partA, t));
				std::vector<std::unique_ptr<AbstrCritTileReader>> readers;
				for (arg_index k = 0; k != d; ++k)
					readers.emplace_back(CreateCritTileReader(critItems[k], t, tileSize));

				ForEachChunk(tileSize, [&](SizeT c, SizeT first, SizeT last)
					{
						for (arg_index k = 0; k != d; ++k)
							readers[k]->Read(first, last - first, critCol[k] + tileFirst + first);

						ChunkInfo& ci = chunkInfo[tileChunkBase[t] + c];
						ci.rowFirst = tileFirst + first;
						ci.rowLast  = tileFirst + last;
						for (SizeT i = first; i != last; ++i)
						{
							const SizeT row = tileFirst + i;
							SizeT p = partGetter->Get(i);
							if (IsDefined(p))
								for (arg_index k = 0; k != d; ++k)
								{
									Float64 v = critCol[k][row];
									if (!(IsDefined(v) && v == v)) // v == v: not NaN, which has no place in a strict weak order
									{
										p = UNDEFINED_VALUE(SizeT);
										break;
									}
								}
							part[row] = p;
							if (!IsDefined(p))
								continue;
							if (!ci.count)
								ci.firstPart = ci.minPart = ci.maxPart = p;
							else
							{
								if (p < ci.lastPart)
									ci.ascending = false;
								ci.minPart = std::min(ci.minPart, p);
								ci.maxPart = std::max(ci.maxPart, p);
							}
							ci.lastPart = p;
							++ci.count;
						}
					}
				);
			}
		);

		// 2. the (partition, row) pairs of the rows that take part, in row order, collected per chunk
		//    in parallel; the partition may already be ascending, which makes the groups the runs
		SizeT m = 0;
		SizeT minPart = std::numeric_limits<SizeT>::max(), maxPart = 0;
		bool  ascending = true;
		const ChunkInfo* prevFilled = nullptr;
		std::vector<SizeT> chunkOffset(nrChunks);
		for (SizeT c = 0; c != nrChunks; ++c)
		{
			const ChunkInfo& ci = chunkInfo[c];
			chunkOffset[c] = m;
			m += ci.count;
			if (!ci.count)
				continue;
			minPart = std::min(minPart, ci.minPart);
			maxPart = std::max(maxPart, ci.maxPart);
			if (!ci.ascending || (prevFilled && ci.firstPart < prevFilled->lastPart))
				ascending = false;
			prevFilled = &ci;
		}

		OwningPtrSizedArray<PartRow> pairs(m, dont_initialize MG_DEBUG_ALLOCATOR_SRC("pareto_optimal: pairs"));
		parallel_for<SizeT>(nrChunks, [&](SizeT c)
			{
				PartRow* out = pairs.begin() + chunkOffset[c];
				for (SizeT row = chunkInfo[c].rowFirst, rowLast = chunkInfo[c].rowLast; row != rowLast; ++row)
					if (IsDefined(part[row]))
						*out++ = PartRow{ part[row], row };
				assert(out == pairs.begin() + chunkOffset[c] + chunkInfo[c].count);
			}
		);
		part.reset();

		// 3. the rows together per partition, in ascending row order within each group
		if (!ascending)
			SortOnPartition(pairs, minPart, maxPart);

		// 4. per block of whole groups, in parallel: per group, the dominance keys and the raw criteria
		//    in one contiguous record per row, the records sorted lexicographically, and the sweep.
		//    The keys are the buckets floor(crit / eps) where eps > 0 (#1282; a quotient within a
		//    millionth of a bucket width below an edge counts as the higher bucket, as in the engine's
		//    Imp2Bucket) and the raw values elsewhere; after the d keys a record holds the raw values
		//    of the bucketed criteria, which order the rows within equal buckets, so that the best row
		//    of a box comes first. The raw value of an unbucketed criterion equals its key and orders
		//    nothing more. The group index breaks the last tie; it is the row order.
		//    Sweep: every earlier record of the group has key1 <= the record under test (and, when
		//    equal on all keys, a lower raw value or row), so dominance reduces to the remaining keys:
		//    with one criterion only the first row of the group survives, with two the row survives iff
		//    its key2 is strictly below the minimum key2 accepted so far, with more it must escape every
		//    accepted row of the group.
		OwningPtrSizedArray<UInt8> keep(n, value_construct MG_DEBUG_ALLOCATOR_SRC("pareto_optimal: keep"));
		const SizeT recSize = d + SizeT(std::count_if(eps.begin(), eps.end(), [](Float64 epsK) { return epsK > 0.0; }));

		// the first position at or after p that starts a group
		auto groupStart = [&pairs, m](SizeT p) -> SizeT
			{
				if (p == 0 || p >= m)
					return std::min(p, m);
				SizeT prevPart = pairs[p - 1].part;
				if (pairs[p].part != prevPart)
					return p;
				return std::upper_bound(pairs.begin() + p, pairs.end(), prevPart, [](SizeT v, const PartRow& pr) { return v < pr.part; }) - pairs.begin();
			};

		const SizeT nrBlocks = (m + GROUP_BLOCK_SIZE - 1) / GROUP_BLOCK_SIZE;
		parallel_for<SizeT>(nrBlocks, [&](SizeT b)
			{
				SizeT gs = groupStart(b * GROUP_BLOCK_SIZE);
				const SizeT blockEnd = groupStart((b + 1) * GROUP_BLOCK_SIZE);

				my_vec_t<Float64> rec;
				my_vec_t<SizeT>   perm;
				my_vec_t<SizeT>   front; // the accepted records of the current group, d >= 3 only
				while (gs != blockEnd)
				{
					SizeT ge = gs + 1;
					while (ge != blockEnd && pairs[ge].part == pairs[gs].part)
						++ge;
					const SizeT g = ge - gs;
					const PartRow* groupPairs = pairs.begin() + gs;
					gs = ge;

					if (g == 1)
					{
						keep[groupPairs[0].row] = 1;
						continue;
					}

					rec.resize(g * recSize);
					perm.resize(g);
					for (SizeT j = 0; j != g; ++j)
					{
						const SizeT row = groupPairs[j].row;
						Float64* recJ = rec.data() + j * recSize;
						SizeT rawPos = d;
						for (arg_index k = 0; k != d; ++k)
						{
							Float64 v = critCol[k][row];
							if (eps[k] > 0.0)
							{
								recJ[k] = std::floor(v / eps[k] + 1e-6);
								recJ[rawPos++] = v;
							}
							else
								recJ[k] = v;
						}
						perm[j] = j;
					}

					auto recLess = [recData = rec.data(), recSize](SizeT a, SizeT b) -> bool
						{
							const Float64* recA = recData + a * recSize;
							const Float64* recB = recData + b * recSize;
							for (SizeT k = 0; k != recSize; ++k)
								if (recA[k] != recB[k])
									return recA[k] < recB[k];
							return a < b;
						};
					if (g >= LARGE_GROUP_SIZE)
						std::sort(std::execution::par, perm.begin(), perm.end(), recLess);
					else
						std::sort(perm.begin(), perm.end(), recLess);

					bool    hasAccepted = false;
					Float64 minKey2 = 0.0;
					front.clear();
					for (SizeT j : perm)
					{
						const Float64* recJ = rec.data() + j * recSize;
						bool dominated;
						if (d == 1)
							dominated = hasAccepted;
						else if (d == 2)
							dominated = hasAccepted && recJ[1] >= minKey2;
						else
						{
							dominated = false;
							for (SizeT f : front)
							{
								const Float64* recF = rec.data() + f * recSize;
								bool dom = true;
								for (arg_index k = 1; k != d && dom; ++k)
									dom = recF[k] <= recJ[k];
								if (dom)
								{
									dominated = true;
									break;
								}
							}
						}
						if (dominated)
							continue;
						keep[groupPairs[j].row] = 1;
						if (d == 1)
							break;
						hasAccepted = true;
						if (d == 2)
							minKey2 = recJ[1];
						else
							front.push_back(j);
					}
				}
			}
		);
		pairs.reset();
		crit.clear();

		// 5. the result, per chunk of rows of each tile in parallel
		parallel_for<SizeT>(nt, [&](SizeT tt)
			{
				tile_id t = tile_id(tt);
				auto resData = res->GetWritableTile(t, dms_rw_mode::write_only_all);
				const SizeT tileFirst = e->GetTileFirstIndex(t);
				ForEachChunk(resData.size(), [&resData, &keep, tileFirst](SizeT, SizeT first, SizeT last)
					{
						auto resI = resData.begin() + first;
						for (SizeT i = first; i != last; ++i, ++resI)
							*resI = (keep[tileFirst + i] != 0);
					}
				);
			}
		);
	}

	bool m_WithEps;
};

namespace
{
	// one instance per number of arguments: the partition relation plus one to eight criteria, and in
	// pareto_optimal_eps an epsilon after each numeric criterion, so 2 (one bool criterion) to 17
	ParetoOptimalOperator po2 (2, false), po3 (3, false), po4 (4, false), po5 (5, false), po6 (6, false), po7 (7, false), po8 (8, false), po9 (9, false);
	ParetoOptimalOperator pe2 (2, true ), pe3 (3, true ), pe4 (4, true ), pe5 (5, true ), pe6 (6, true ), pe7 (7, true ), pe8 (8, true ), pe9 (9, true );
	ParetoOptimalOperator pe10(10, true), pe11(11, true), pe12(12, true), pe13(13, true), pe14(14, true), pe15(15, true), pe16(16, true), pe17(17, true);
}
