// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
// 
// File: nth_element.cpp
// Purpose:
//   Provides implementations of nth / ratio (percentile-like) element aggregate
//   operators, both total (single value over full domain) and partitioned
//   (per-partition) variants, including weighted versions.
//   Operators implemented:
//     - nth_element
//     - nth_element_weighted
//     - rth_element (ratio-th element; supports interpolation)
//   Supports both total-domain aggregation and partitioned aggregation,
//   including handling of tiled data, undefined values, and weight arrays.
//
// High-level architecture:
//   1. Abstract base operator adapters:
//        * AbstrPthElementTot              (binary: values, position)
//        * AbstrNthElementWeightedTot      (ternary: values, cumulative weight, weight array)
//        * AbstrPthElementPart             (ternary: ranking(values), nth/ratio vector, partition map)
//        * AbstrNthElementWeightedPart     (quaternary: values, cum weight(s), weight array, partition map)
//      These wrap argument validation, domain/value unit unification,
//      result allocation, and invoke a virtual Calculate() implementation.
//
//   2. Concrete operators for unweighted nth element:
//        * NthElementTot
//        * NthElementPart
//
//   3. Concrete operators for weighted nth element (interpreting the nth as a
//      cumulative weight threshold):
//        * NthElementWeightedTot
//        * NthElementWeightedPart
//
//   4. Concrete operators for ratio-th element (percentile-like) with optional
//      interpolation when the fractional position lies between two ranks:
//        * RthElementTot
//        * RthElementPart
//
//   5. Weighted selection uses a custom nth_element-like routine
//      (nth::nth_element_weighted) that repeatedly partitions indices by
//      pivot (median-of-medians guess via std::_Partition_by_median_guess_unchecked).
//
// Performance considerations:
//   - Counts of defined values collected first (count_best_total / count_best_partial_best)
//     to pre-size buffers exactly (avoid realloc).
//   - Work performed per tile to minimize cache misses and respect underlying
//     storage chunking.
//   - Partitioned versions build cumulative offsets (cumul) to scatter values
//     into a contiguous buffer per partition slice, enabling in-place
//     std::nth_element restricted to partition subranges.
//   - Weighted nth avoids copying value data; instead uses an index vector
//     referencing original arrays.
//
// Edge cases handled:
//   - n out of range -> UNDEFINED_OR_MAX(V)
//   - Empty partitions -> UNDEFINED_OR_MAX(V)
//   - Missing / undefined values skipped
//   - Weights <= 0 ignored for weighted nth
//   - Ratio r outside [0,1] effectively yields UNDEFINED_OR_MAX(V) if implied
//     position not in range
//
// Thread-safety / locking strategy:
//   - DataReadLock / GetLockedDataRead used to safely access tiled data.
//   - Writes guarded by DataWriteLock then committed.
//
// Plan (pseudocode for added documentation only):
//   - Add comprehensive file header summarizing purpose and design.
//   - For each abstract base struct: describe role, expected arguments,
//     and Calculate responsibility.
//   - For each concrete operator: explain algorithm steps succinctly.
//   - Document weighted nth internal helper functions.
//   - Clarify interpolation behavior for rth element.
//   - Preserve all original logic; only add comments.
//   - Keep comments concise but informative for future maintainers.
//
// No functional changes below—only comments added.
// ============================================================================

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "set/VectorFunc.h"
#include "mci/CompositeCast.h"
#include "geo/Conversions.h"

#include "Param.h"
#include "DataItemClass.h"

#include "AggrUniStructNum.h"
#include "OperAccUni.h"
#include "OperRelUni.h"

#include <iterator>

// *****************************************************************************
// Operator groups (names are registered for lookup / overloading)
// *****************************************************************************
CommonOperGroup cogNthElem        ("nth_element",          oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogNthElemWeighted("nth_element_weighted", oper_policy::better_not_in_meta_scripting);

// *****************************************************************************
// AbstrPthElementTot
//   Base for total-domain nth-like operations with a single position parameter.
//   Args: (values, position)
//   Result: single value
// *****************************************************************************
template <typename PosType>
struct AbstrPthElementTot: BinaryOperator
{
	AbstrPthElementTot(AbstrOperGroup* gr, ClassCPtr argVCls)
		: BinaryOperator(gr, argVCls, argVCls, DataArray<PosType>::GetStaticClass())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* argVA = AsDataItem(args[0]);
		assert(argVA);

		// Create result (single scalar) in same values unit as source if not provided
		if (!resultHolder)
			resultHolder = CreateCacheDataItem(Unit<Void>::GetStaticClass()->CreateDefault(), argVA->GetAbstrValuesUnit());

		if (mustCalc)
		{
			const AbstrDataItem* arg2A = AsDataItem(args[1]);

			DataReadLock arg2Lock (arg2A);
			PosType p = arg2A->GetValue<PosType>(0);

			DataReadLock argVLock(argVA);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			assert(res);
			DataWriteLock resLock(res);

			Calculate(resLock, argVA, p);

			resLock.Commit();
		}
		return true;
	}
	// Implemented by concrete nth / ratio operator
	virtual void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, PosType p) const =0;
};

// *****************************************************************************
// AbstrNthElementWeightedTot
//   Base for weighted total nth-like operation where the "position" is given
//   as cumulative weight threshold.
//   Args: (values, cumulativeWeight, weightArray)
// *****************************************************************************
template <typename WeightType>
struct AbstrNthElementWeightedTot: TernaryOperator
{
	AbstrNthElementWeightedTot(AbstrOperGroup* gr, ClassCPtr argVCls)
		: TernaryOperator(gr, argVCls, argVCls, DataArray<WeightType>::GetStaticClass(), DataArray<WeightType>::GetStaticClass())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrDataItem* argVA = AsDataItem(args[0]);
		assert(argVA);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(Unit<Void>::GetStaticClass()->CreateDefault(), argVA->GetAbstrValuesUnit());

		if (mustCalc)
		{
			const AbstrDataItem* arg2A = AsDataItem(args[1]);

			DataReadLock arg2Lock (arg2A);
			WeightType cumulWeight = arg2A->GetValue<WeightType>(0);

			const AbstrDataItem* weightA = AsDataItem(args[2]);
			assert(weightA);
			// Validate domain/value compatibility
			weightA->GetAbstrDomainUnit()->UnifyDomain(argVA->GetAbstrDomainUnit(), "e3", "e1", UM_Throw);
			weightA->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit(), "v3", "v2", UM_Throw);

			DataReadLock argVLock  (argVA);
			DataReadLock weightLock(weightA);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			assert(res);

			DataWriteLock resLock(res);

			Calculate(resLock, argVA, cumulWeight, weightA);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, WeightType p, const AbstrDataItem* weightA) const =0;
};

// *****************************************************************************
// AbstrPthElementPart
//   Base for partitioned nth/ratio operations.
//   Args: (values, pos/ratio array or scalar, partitionMap)
//   pos/ratio argument can be scalar (domain Void) or per-partition vector.
// *****************************************************************************
template <typename PosType>
struct AbstrPthElementPart: TernaryOperator
{
	AbstrPthElementPart(AbstrOperGroup* gr, ClassCPtr argVCls, ClassCPtr arg3Cls)
		: TernaryOperator(gr, argVCls, argVCls, DataArray<PosType>::GetStaticClass(), arg3Cls)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrDataItem* argRankingA = AsDataItem(args[0]);
		assert(argRankingA);

		const AbstrDataItem* argPartitionA = AsDataItem(args[2]);
		assert(argPartitionA);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(argPartitionA->GetAbstrValuesUnit(), argRankingA->GetAbstrValuesUnit());

		// Ensure partition indices align with ranking domain
		argPartitionA->GetAbstrDomainUnit()->UnifyDomain(argRankingA->GetAbstrDomainUnit(), "Domain of the Partitioning", "Domain of the Ranking", UM_Throw);

		const AbstrDataItem* argTargetCountA = AsDataItem(args[1]);
		const AbstrUnit*     e2    = argTargetCountA->GetAbstrDomainUnit();
		bool e2Void = e2->GetValueType() == ValueWrap<Void>::GetStaticClass();
		if (!e2Void)
			e2->UnifyDomain(argPartitionA->GetAbstrValuesUnit(), "e2", "Partitioning values", UM_Throw);

		if (mustCalc)
		{
			assert(argTargetCountA);
			DataReadLock argVLock(argRankingA);
			DataReadLock arg2Lock(argTargetCountA);
			DataReadLock arg3Lock(argPartitionA);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock, argRankingA, const_array_cast<PosType>(argTargetCountA), e2Void, argPartitionA);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, const DataArray<PosType>* arg2, bool e2Void, const AbstrDataItem* arg3a) const =0;
};

// *****************************************************************************
// AbstrNthElementWeightedPart
//   Base for partitioned weighted nth operations.
//   Args: (values, cumulWeight(s), weightArray, partitionMap)
// *****************************************************************************
struct AbstrNthElementWeightedPart: QuaternaryOperator
{
	AbstrNthElementWeightedPart(AbstrOperGroup* gr, ClassCPtr argVCls, ClassCPtr weightCls, ClassCPtr arg3Cls)
		: QuaternaryOperator(gr, argVCls, argVCls, weightCls, weightCls, arg3Cls)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 4);

		const AbstrDataItem* argVA = AsDataItem(args[0]);
		assert(argVA);

		const AbstrDataItem* argPartitionA = AsDataItem(args[3]);
		assert(argPartitionA);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(argPartitionA->GetAbstrValuesUnit(), argVA->GetAbstrValuesUnit());

		// Domain alignments
		argPartitionA->GetAbstrDomainUnit()->UnifyDomain(argVA->GetAbstrDomainUnit(), "e4", "e1", UM_Throw);

		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrUnit*     e2    = arg2A->GetAbstrDomainUnit();
		bool e2Void = e2->GetValueType() == ValueWrap<Void>::GetStaticClass();
		if (!e2Void)
			e2->UnifyDomain(argPartitionA->GetAbstrValuesUnit(), "e2", "v4", UM_Throw);

		const AbstrDataItem* weightA = AsDataItem(args[2]);
		assert(weightA);
		weightA->GetAbstrDomainUnit()->UnifyDomain(argVA->GetAbstrDomainUnit(), "e3", "e1", UM_Throw);
		weightA->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit(), "v4", "v2", UM_Throw);

		if (mustCalc)
		{
			assert(arg2A);
			DataReadLock argVLock(argVA);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(argPartitionA);
			DataReadLock arg4Lock(weightA);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock, argVA, arg2A, e2Void, weightA, argPartitionA);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, const AbstrDataItem* arg2, bool e2Void, const AbstrDataItem* weightA, const AbstrDataItem* arg3a) const =0;
};

// *****************************************************************************
// NthElementTot
//   Unweighted nth element over entire dataset.
//   Steps:
//     1. Count defined values to size copy buffer.
//     2. Copy defined values.
//     3. If n out of range -> UNDEFINED.
//     4. std::nth_element to isolate nth.
// *****************************************************************************
template <typename V, typename I=UInt32> 
struct NthElementTot: AbstrPthElementTot<I>
{
	typedef DataArray<V> ArgVType;
	typedef ArgVType     ResultType;

	NthElementTot() 
		: AbstrPthElementTot<I>(&cogNthElem, ArgVType::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, UInt32 n) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);

		ResultType* result = mutable_array_cast<V>(res);
		assert(result);
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
		assert(resData.size() == 1);

		// Count defined
		UInt32 N = 0;
		auto tn = argVA->GetAbstrDomainUnit()->GetNrTiles();
		for (tile_id t=0; t!=tn; ++t)
		{
			auto argVData = argV->GetTile(t);
			count_best_total(N, argVData.begin(), argVData.end());
		}
		// Bounds check
		if (n >= N)
		{
			resData[0] = UNDEFINED_OR_MAX(V);
			return;
		}
		typename sequence_traits<V>::container_type copy;
		copy.reserve(N MG_DEBUG_ALLOCATOR_SRC("NthElementTot buffer"));

		// Collect defined
		for (tile_id t=0; t!=tn; ++t)
			for (auto argVi: argV->GetTile(t))
				if (IsDefined(argVi))
					copy.push_back(argVi MG_DEBUG_ALLOCATOR_SRC("NthElementTot buffer"));

		std::nth_element(copy.begin(), copy.begin() + n, copy.end());

		resData[0] = copy[n];
	}
};

// *****************************************************************************
// NthElementPart
//   Per-partition unweighted nth.
//   Each partition gets its own subrange in a contiguous buffer.
// *****************************************************************************
template <typename V, typename I = UInt32> 
struct NthElementPart: AbstrPthElementPart<I>
{
	typedef DataArray<V>  ArgVType;
	typedef DataArray<I>  Arg2Type; // nth indices
	typedef AbstrDataItem Arg3Type; // Partition mapping
	typedef DataArray<V>  ResultType;

	NthElementPart() 
		: AbstrPthElementPart<I>(&cogNthElem, ArgVType::GetStaticClass(), Arg3Type::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, const DataArray<I>* arg2, bool e2Void, const AbstrDataItem* argPA) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);

		const AbstrUnit* valuesDomain = argVA->GetAbstrDomainUnit();
		// Total raw slots (not all defined)
		SizeT N = 0;
		for (tile_id t=0, te=valuesDomain->GetNrTiles(); t!=te; ++t)
			N += valuesDomain->GetTileCount(t);

		SizeT nrP = argPA->GetAbstrValuesUnit()->GetCount();
		auto arg2Data = arg2->GetDataRead();

		ResultType* result = mutable_array_cast<V>(res);
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);

		// Count defined per partition
		std::vector<SizeT> partCount(nrP, 0);
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, t);
			count_best_partial_best(partCount.begin(), argVData.begin(), argVData.end(), indexGetter );
		}

		// Compute cumulative offsets
		SizeT cumulativeN = 0;
		sequence_traits<SizeT>::container_type cumul; cumul.reserve(nrP MG_DEBUG_ALLOCATOR_SRC("NthElementPart buffer"));
		for (auto i = partCount.begin(), e = partCount.end(); i!=e; ++i)
		{
			cumul.push_back(cumulativeN MG_DEBUG_ALLOCATOR_SRC_EMPTY);
			cumulativeN += *i;
		}
		sequence_traits<SizeT>::container_type cumul2 = cumul;

		// Copy defined values into partitioned buffer
		typename sequence_traits<V>::container_type copy(cumulativeN, V() MG_DEBUG_ALLOCATOR_SRC("NthElementPart buffer copy"));
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, t);

			SizeT j=0;
			for (auto i = argVData.begin(), e = argVData.end(); i != e; ++j, ++i)
			{
				if (IsDefined(*i))
				{
					SizeT p = indexGetter->Get(j);
					if (IsDefined(p))
						copy[cumul2[p]++] = (*i);
				}
			}
		}

		// Per partition nth selection
		for (SizeT i = 0; i != nrP; ++i)
		{
			SizeT pos = arg2Data[e2Void ? 0 : i], pCount = partCount[i];
			if (pos<pCount)
			{
				auto
					cbi = copy.begin() + cumul[i],
					cbp = cbi + pos;
				std::nth_element(cbi, cbp, cbi + pCount);
				resData[i] = *cbp;
			}
			else
				resData[i] = UNDEFINED_OR_MAX(V);
		}
	}
};

// *****************************************************************************
// Weighted nth helper namespace
// *****************************************************************************
namespace nth {

	// accumulate_ptr:
	//   Sums weights of elements addressed by index iterator range.
	template<typename WeightType, typename IndexIter, typename WeightPtr > 
	WeightType accumulate_ptr(IndexIter first, IndexIter last, WeightPtr firstWeight)
	{
		WeightType w = 0;
		for (; first != last; ++first)
		{
			WeightType elemWeight = firstWeight[*first];
			assert(IsDefined(elemWeight));
			assert(elemWeight >= 0); 
			w += elemWeight;
		}
		return w;
	}

	// nth_element_weighted:
	//   Similar to nth_element but using cumulative weights to locate which
	//   value covers the desired cumulative threshold.
	//   Uses partitioning by pivot range. If target lies inside pivot band,
	//   returns pivot value early.
	template<typename RankType, typename WeightType, typename IndexIter, typename RankIter, typename WeightIter> 
	RankType nth_element_weighted(IndexIter first, IndexIter last, RankIter firstRank, WeightIter firstWeight, WeightType cumulWeight)
	{
		IndexIter originalLast = last;
		while(1 < last - first)
		{
			std::pair<IndexIter, IndexIter> mid = std::_Partition_by_median_guess_unchecked(first, last, IndexCompareOper<RankIter>(firstRank));

			WeightType midFirstWeight = accumulate_ptr<WeightType>(first, mid.first, firstWeight);

			if (cumulWeight < midFirstWeight)
				last = mid.first;
			else
			{
				WeightType midLastWeight  = midFirstWeight + accumulate_ptr<WeightType>(mid.first, mid.second, firstWeight);
				if (cumulWeight >= midLastWeight)
				{
					first = mid.second;
					cumulWeight -= midLastWeight;
				}
				else
					return firstRank[*mid.first]; // Inside pivot band
			}
		}
		// Terminal case: single element or empty; verify cumulative threshold
		return (last != first) && (cumulWeight < firstWeight[*first]) || (originalLast != last)
			?	firstRank[*first]
			:	UNDEFINED_OR_MAX(RankType);
	}
} // namespace nth

// *****************************************************************************
// NthElementWeightedTot
//   Weighted nth over entire dataset.
//   Builds index vector for defined & positively weighted entries and applies
//   weighted partition search (does not copy value data).
// *****************************************************************************
template <typename V, typename W> 
struct NthElementWeightedTot: AbstrNthElementWeightedTot<W>
{
	typedef DataArray<V> ArgVType;
	typedef DataArray<W> ArgWType;
	typedef ArgVType     ResultType;
	
	NthElementWeightedTot() 
		:	AbstrNthElementWeightedTot<W>(&cogNthElemWeighted, ArgVType::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, W cumulWeight, const AbstrDataItem* argWA) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);
		const ArgWType* argW = const_array_cast<W>(argWA); assert(argW);

		ResultType* result = mutable_array_cast<V>(res);
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		SizeT N = 0;
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);
			count_best_total(N, argVData.begin(), argVData.end());
		}

		sequence_traits<UInt32>::container_type indexVector; indexVector.reserve(N MG_DEBUG_ALLOCATOR_SRC("NthElementWeightedTot buffer"));

		SizeT i = 0;
		auto argVData = argV->GetLockedDataRead();
		auto argWData = argW->GetLockedDataRead();
		auto argW_ptr = argWData.begin();
		for (auto v: argVData)
		{
			if (IsDefined(v) && IsDefined(*argW_ptr) && *argW_ptr > 0)
				indexVector.push_back(i MG_DEBUG_ALLOCATOR_SRC("NthElementWeightedTot buffer"));
			++argW_ptr; ++i;
		}

		resData[0] = nth::nth_element_weighted<V, W>(indexVector.begin(), indexVector.end(), argVData.begin(), argWData.begin(), cumulWeight);
	}
};

#include "TileIter.h"

// *****************************************************************************
// NthElementWeightedPart
//   Partitioned weighted nth allowing distinct cumulative weight target per
//   partition (or global if scalar).
// *****************************************************************************
template <typename V, typename W> 
struct NthElementWeightedPart: AbstrNthElementWeightedPart
{
	typedef DataArray<V>  ArgVType;
	typedef DataArray<W>  Arg2Type; // cumulative weight targets
	typedef AbstrDataItem Arg3Type; // Partitioning
	typedef DataArray<W>  ArgWType;
	typedef DataArray<V>  ResultType;

	NthElementWeightedPart() 
		: AbstrNthElementWeightedPart(&cogNthElemWeighted, ArgVType::GetStaticClass(), ArgWType::GetStaticClass(), Arg3Type::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, const AbstrDataItem* arg2A, bool e2Void, const AbstrDataItem* argWA, const AbstrDataItem* argPA) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);
		const ArgWType* arg2 = const_array_cast<W>(arg2A); assert(arg2);
		const ArgWType* argW = const_array_cast<W>(argWA); assert(argW);

		const AbstrUnit* valuesDomain = argVA->GetAbstrDomainUnit();
		SizeT N = 0;
		for (tile_id t=0, te=valuesDomain->GetNrTiles(); t!=te; ++t)
			N += valuesDomain->GetTileCount(t);

		SizeT nrP = argPA->GetAbstrValuesUnit()->GetCount();
		auto arg2Data = arg2->GetDataRead(); // non-tiled cumulative target(s)

		ResultType* result = mutable_array_cast<V>(res);
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);

		// Count defined per partition (ignores weight at this stage)
		auto partCount = sequence_traits<SizeT>::container_type(nrP, 0 MG_DEBUG_ALLOCATOR_SRC("NthElementWeightedPath index buffer"));
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, t);
			count_best_partial_best(partCount.begin(), argVData.begin(), argVData.end(), indexGetter);
		}

		// Build cumulative offsets (pre-weights filter)
		SizeT cumulativeN = 0;
		sequence_traits<SizeT>::container_type cumul; cumul.reserve(nrP MG_DEBUG_ALLOCATOR_SRC("NthElementWeightedPath buffer"));
		for (auto pc: partCount)
		{
			cumul.push_back(cumulativeN MG_DEBUG_ALLOCATOR_SRC_EMPTY);
			cumulativeN += pc;
		}
		auto cumul2 = sequence_traits<SizeT>::container_type(cumul MG_DEBUG_ALLOCATOR_SRC("NthElementWeightedPath bufferCopy"));

		// Allocate index vector (will shrink effective counts if weight invalid)
		auto indexVector = sequence_traits<SizeT>::container_type(cumulativeN, SizeT() MG_DEBUG_ALLOCATOR_SRC("NthElementWeightedPath index buffer"));

		SizeT i = 0;
		auto argVData = argV->GetLockedDataRead();
		auto argWData = argW->GetLockedDataRead();
		SizeT ie = argVData.size();

		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, no_tile);
		for (SizeT ii = 0; ii != ie; ++i, ++ii)
		{
			if (IsDefined(argVData[ii]))
			{
				SizeT p = indexGetter->Get(ii);
				if (IsDefined(p))
				{
					if (IsDefined(argWData[ii]) && (argWData[ii] > 0))
						indexVector[ cumul2[p]++ ] = i;
					else
						--partCount[p]; // adjust for weight exclusion
				}
			}
		}

		// For each partition perform weighted nth selection
		for (SizeT ii = 0; ii != nrP; ++ii)
		{
			W cumulWeight = arg2Data[e2Void ? 0 : ii];
			auto cbi = begin_ptr(indexVector) + cumul[ii];
			resData[ii] = nth::nth_element_weighted<V>(cbi, cbi + partCount[ii], argVData.begin(), argWData.begin(), cumulWeight);
		}
	}
};

// *****************************************************************************
// Ratio type operator group (rth_element) + typedef for ratio
// *****************************************************************************
CommonOperGroup cogRthElem("rth_element", oper_policy::better_not_in_meta_scripting);
typedef Float32 RatioType;

// *****************************************************************************
// RthElementTot
//   Ratio-th element over entire dataset with linear interpolation:
//     position = r * (N - 1)
//     floor/ceil used; fractional part interpolates if next element exists.
// *****************************************************************************
template <class V> 
struct RthElementTot: AbstrPthElementTot<RatioType>
{
	typedef DataArray<V> ArgVType;
	typedef ArgVType     ResultType;

	RthElementTot() 
		: AbstrPthElementTot(&cogRthElem, ArgVType::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, RatioType r) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA);
		assert(argV);

		ResultType* result = mutable_array_cast<V>(res);
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		// Count defined and fill buffer
		SizeT N = 0;
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);
			count_best_total(N, argVData.begin(), argVData.end());
		}
		typename sequence_traits<V>::container_type copy;
		copy.reserve(N MG_DEBUG_ALLOCATOR_SRC("RthElementTot buffer"));

		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);
			for (auto iv = argVData.begin(), ev = argVData.end(); iv != ev; ++iv)
				if (IsDefined(*iv))
					copy.push_back(*iv MG_DEBUG_ALLOCATOR_SRC("RthElementTot buffer"));
		}
		Float64 rr = r;
		rr *= (N - 1);

		SizeT pos = rr;
		if(pos < N)
		{
			auto cbi = copy.begin();
			auto cbp = cbi + pos;
			auto cbe = copy.end();
			std::nth_element(cbi, cbp, cbe);
			resData[0] = *cbp;
			rr -= pos;
			if (rr && ++cbp != cbe)
			{
				std::nth_element(cbi, cbp, cbe);
				resData[0] = V(resData[0]) + rr*(V(*cbp)-V(resData[0]));
			}
		}
		else
			resData[0] = UNDEFINED_OR_MAX(V);
	}
};

// *****************************************************************************
// RthElementPart
//   Partitioned ratio-th selection with interpolation per partition.
// *****************************************************************************
template <typename V, typename I=UInt32> 
struct RthElementPart: AbstrPthElementPart<RatioType>
{
	typedef RatioType    R;
	typedef DataArray<V> ArgVType;
	typedef DataArray<R> Arg2Type; // ratio(s)
	typedef DataArray<V> ResultType;

	RthElementPart() 
		: AbstrPthElementPart(&cogRthElem, ArgVType::GetStaticClass(), AbstrDataItem::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, const DataArray<RatioType>* arg2, bool e2Void, const AbstrDataItem* argPA) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);

		const AbstrUnit* valuesDomain = argVA->GetAbstrDomainUnit();
		SizeT N = 0;
		for (tile_id t=0, te=valuesDomain->GetNrTiles(); t!=te; ++t)
			N += valuesDomain->GetTileCount(t);

		SizeT nrP = argPA->GetAbstrValuesUnit()->GetCount();
		auto arg2Data = arg2->GetDataRead();

		ResultType* result = mutable_array_cast<V>(res);
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);

		std::vector<I> partCount(nrP, 0);

		// Count defined per partition
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, t);
			count_best_partial_best(partCount.begin(), argVData.begin(), argVData.end(), indexGetter);
		}

		// Cumulative offsets
		I cumulativeN = 0;
		std::vector<I> cumul; cumul.reserve(nrP);
		for (auto i = partCount.begin(), e = partCount.end(); i!=e; ++i)
		{
			cumul.push_back(cumulativeN);
			cumulativeN += *i;
		}
		std::vector<I> cumul2 = cumul;

		// Copy defined values into partition slices
		typename sequence_traits<V>::container_type copy(cumulativeN, V() MG_DEBUG_ALLOCATOR_SRC("rth_element buffer"));
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);

			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, t);
			SizeT j=0;
			for (auto i = argVData.begin(), e = argVData.end(); i != e; ++j, ++i)
			{
				if (IsDefined(*i))
				{
					SizeT p = indexGetter->Get(j);
					if (IsDefined(p))
						copy[cumul2[p]++] = (*i);
				}
			}
		}
		V resValue;
		for (SizeT i = 0; i != nrP; ++i)
		{
			I pCount = partCount[i];
			Float64 rr = arg2Data[e2Void ? 0 : i];
			rr *= (pCount - 1);
			UInt32 pos = rr;
			if (pos<pCount)
			{
				auto cbi = copy.begin() + cumul[i];
				auto cbp = cbi + pos;
				auto cbe = cbi + pCount;
				std::nth_element(cbi, cbp, cbe);
				resValue = *cbp;
				rr -= pos;
				if (rr && ++cbp != cbe)
				{
					std::nth_element(cbi, cbp, cbe);
					resValue = resValue + rr*(V(*cbp)-V(resValue));
				}
			}
			else
				resValue = UNDEFINED_OR_MAX(V);
			resData[i] = resValue;
		}
	}
};

// *****************************************************************************
// Instantiation section: For each numeric ValueType in typelists::num_objects
// we instantiate operator objects so they self-register in their groups.
// *****************************************************************************
#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	template<typename ValueType>
	struct AggrOperInstances
	{
	private:
		NthElementTot<ValueType>              m_NElemOperator;
		NthElementWeightedTot<ValueType, Float32> m_NElemWeighted32Operator;
		NthElementWeightedTot<ValueType, Float64> m_NElemWeighted64Operator;
		RthElementTot<ValueType>              m_RElemOperator;

		NthElementPart<ValueType>             m_NPartOperator;
		NthElementWeightedPart<ValueType, Float32> m_NPartW32Operator;
		NthElementWeightedPart<ValueType, Float64> m_NPartW64Operator;
		RthElementPart<ValueType>             m_RPartOperator;
	};

	tl_oper::inst_tuple_templ<typelists::num_objects, AggrOperInstances> s_PthElemOperators;
}
