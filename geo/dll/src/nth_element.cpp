// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
//										nth elem
// *****************************************************************************

CommonOperGroup cogNthElem        ("nth_element", oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogNthElemWeighted("nth_element_weighted", oper_policy::better_not_in_meta_scripting);


// *****************************************************************************
//											AbstrPthElementTot
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
	virtual void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, PosType p) const =0;
};


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
//											AbstrPthElementPart
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

		// check partitions domain with order domain
		argPartitionA->GetAbstrDomainUnit()->UnifyDomain(argRankingA->GetAbstrDomainUnit(), "Domain of the Partitioning", "Domain of the Ranking", UM_Throw);

		// check partitions values with pos domain
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

		// check partitions domain with order domain
		argPartitionA->GetAbstrDomainUnit()->UnifyDomain(argVA->GetAbstrDomainUnit(), "e4", "e1", UM_Throw);

		// check partitions values with pos domain
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrUnit*     e2    = arg2A->GetAbstrDomainUnit();
		bool e2Void = e2->GetValueType() == ValueWrap<Void>::GetStaticClass();
		if (!e2Void)
			e2->UnifyDomain(argPartitionA->GetAbstrValuesUnit(), "e2", "v4", UM_Throw);

		// check partitions domain with weight domain
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
//											Nth_Element
// *****************************************************************************

template <typename V, typename I=UInt32> 
struct NthElementTot: AbstrPthElementTot<I>
{
	typedef DataArray<V> ArgVType;
	typedef ArgVType     ResultType;

	NthElementTot() 
		: AbstrPthElementTot<I>(&cogNthElem, ArgVType::GetStaticClass())
	{}

	// Override Operator
	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, UInt32 n) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);

		ResultType* result = mutable_array_cast<V>(res);
		assert(result);
		auto resData = result->GetDataWrite();
		assert(resData.size() == 1);

		UInt32 N = 0;
		auto tn = argVA->GetAbstrDomainUnit()->GetNrTiles();
		for (tile_id t=0; t!=tn; ++t)
		{
			auto argVData = argV->GetTile(t);
			count_best_total(N, argVData.begin(), argVData.end());
		}
		if (n >= N)
		{
			resData[0] = UNDEFINED_OR_MAX(V);
			return;
		}
		assert(n < N);
		typename sequence_traits<V>::container_type copy;
		copy.reserve(N);

		for (tile_id t=0; t!=tn; ++t)
			for (auto argVi: argV->GetTile(t))
				if (IsDefined(argVi))
				{
					assert(copy.size() < N); // sufficient reservation
					copy.push_back(argVi);
				}
		assert(copy.size() == N); // neccesary reservation
		std::nth_element(copy.begin(), copy.begin() + n, copy.end());

		resData[0] = copy[n];
	}
};


template <typename V, typename I = UInt32> 
struct NthElementPart: AbstrPthElementPart<I>
{
	typedef DataArray<V>  ArgVType;
	typedef DataArray<I>  Arg2Type; // nth
	typedef AbstrDataItem Arg3Type; // Partitioning
	typedef DataArray<V>  ResultType;

	NthElementPart() 
		: AbstrPthElementPart<I>(&cogNthElem, ArgVType::GetStaticClass(), Arg3Type::GetStaticClass())
	{}

	// Override Operator
	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, const DataArray<I>* arg2, bool e2Void, const AbstrDataItem* argPA) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);

		const AbstrUnit* valuesDomain = argVA->GetAbstrDomainUnit();
		SizeT N = 0;
		for (tile_id t=0, te=valuesDomain->GetNrTiles(); t!=te; ++t)
			N += valuesDomain->GetTileCount(t);

		SizeT nrP = argPA->GetAbstrValuesUnit()->GetCount();
		auto arg2Data = arg2->GetDataRead();

		assert(arg2Data.size() == (e2Void ? 1 : nrP)); // domain of arg2 is domain compatible with values unit of argP

		ResultType* result = mutable_array_cast<V>(res);
		assert(result);
		auto resData = result->GetDataWrite();
		assert(resData.size() == nrP);

		std::vector<SizeT> partCount(nrP, 0);
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, t);
			count_best_partial_best(partCount.begin(), argVData.begin(), argVData.end(), indexGetter );
		}

		// cumulative counts per partition
		SizeT cumulativeN = 0;
		std::vector<SizeT> cumul; cumul.reserve(nrP);
		for (std::vector<SizeT>::const_iterator i = partCount.begin(), e = partCount.end(); i!=e; ++i)
		{
			cumul.push_back(cumulativeN);
			cumulativeN += *i;
		}
		assert(cumulativeN <= N);
		std::vector<SizeT> cumul2 = cumul;
		typename sequence_traits<V>::container_type copy(cumulativeN, V());
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
					{
						assert(p < nrP);
						copy[cumul2[p]++] = (*i);
						assert(cumul2[p] <= cumul[p] + partCount[p]);
					}
				}
			}
		}
		for (SizeT i = 0; i != nrP; ++i)
		{
			assert(cumul2[i] == cumul[i] + partCount[i]);
			assert(cumul2[i] <= ((i+1<nrP) ? cumul[i+1] : cumulativeN) );

			UInt32 pos = arg2Data[e2Void ? 0 : i], pCount = partCount[i];
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
//											Nth_Element_Weighted
// *****************************************************************************

namespace nth {

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

	template<typename RankType, typename WeightType, typename IndexIter, typename RankIter, typename WeightIter> 
	RankType nth_element_weighted(IndexIter first, IndexIter last, RankIter firstRank, WeightIter firstWeight, WeightType cumulWeight)
	{	// order Nth element, using operator<
		IndexIter originalLast = last;
		while(1 < last - first)
		{	// divide and conquer, ordering partition containing Nth
			assert(first < last);
			std::pair<IndexIter, IndexIter> mid = std::_Partition_by_median_guess_unchecked(first, last, IndexCompareOper<RankIter>(firstRank));

			assert(first <= mid.first);
			assert(mid.first < mid.second);
			assert(mid.second <= last);

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
					assert(cumulWeight >= 0);
				}
				else
					return firstRank[*mid.first]; // Nth inside fat pivot, done
			}
		}
		return (last != first) && (cumulWeight < firstWeight[*first]) || (originalLast != last)
			?	firstRank[*first]
			:	UNDEFINED_OR_MAX(RankType);
	}
} // namespace nth

template <typename V, typename W> 
struct NthElementWeightedTot: AbstrNthElementWeightedTot<W>
{
	typedef DataArray<V> ArgVType;
	typedef DataArray<W> ArgWType;
	typedef ArgVType     ResultType;
	
	NthElementWeightedTot() 
		:	AbstrNthElementWeightedTot<W>(&cogNthElemWeighted, ArgVType::GetStaticClass())
	{}

	// Override Operator
	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, W cumulWeight, const AbstrDataItem* argWA) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);
		const ArgWType* argW = const_array_cast<W>(argWA); assert(argW);

		ResultType* result = mutable_array_cast<V>(res);
		assert(result);
		auto resData = result->GetDataWrite();
		assert(resData.size() == 1);

		SizeT N = 0;
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);
			count_best_total(N, argVData.begin(), argVData.end());
		}

		sequence_traits<UInt32>::container_type indexVector; indexVector.reserve(N);

		SizeT i = 0;
		auto argVData = argV->GetLockedDataRead();
		auto argWData = argW->GetLockedDataRead();
		auto argW_ptr = argWData.begin();
		for (auto v: argVData)
		{
			if (IsDefined(v) && IsDefined(*argW_ptr) && *argW_ptr > 0)
			{
				assert(indexVector.size() < N); // sufficient reservation
				indexVector.push_back(i);
			}
			++argW_ptr; ++i;
		}
		assert(indexVector.size() <= N); // neccesary reservation

		assert(argVData.size() == argWData.size());
		resData[0] = nth::nth_element_weighted<V, W>(indexVector.begin(), indexVector.end(), argVData.begin(), argWData.begin(), cumulWeight);
	}
};

#include "TileIter.h"

template <typename V, typename W> 
struct NthElementWeightedPart: AbstrNthElementWeightedPart
{
	typedef DataArray<V>  ArgVType;
	typedef DataArray<W>  Arg2Type; // nth
	typedef AbstrDataItem Arg3Type; // Partitioning
	typedef DataArray<W>  ArgWType;
	typedef DataArray<V>  ResultType;

	NthElementWeightedPart() 
		: AbstrNthElementWeightedPart(&cogNthElemWeighted, ArgVType::GetStaticClass(), ArgWType::GetStaticClass(), Arg3Type::GetStaticClass())
	{}

	// Override Operator
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
		auto arg2Data = arg2->GetDataRead(); // NOT TILED.
		assert(arg2Data.size() == (e2Void ? 1 : nrP));

		ResultType* result = mutable_array_cast<V>(res);
		assert(result);
		auto resData = result->GetDataWrite();
		assert(resData.size() == nrP);

		// === make partCount
		std::vector<SizeT> partCount(nrP, 0);
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, t);
			count_best_partial_best(partCount.begin(), argVData.begin(), argVData.end(), indexGetter);
		}

		// === cumulative counts per partition
		SizeT cumulativeN = 0;
		std::vector<SizeT> cumul; cumul.reserve(nrP);
		for (auto pc: partCount)
		{
			cumul.push_back(cumulativeN);
			cumulativeN += pc;
		}
		assert(cumulativeN <= N);
		std::vector<SizeT> cumul2 = cumul;

		// === make indexvector
		std::vector<SizeT> indexVector(cumulativeN, SizeT()); // SIZE-N

		SizeT i = 0;
		auto argVData = argV->GetLockedDataRead();
		auto argWData = argW->GetLockedDataRead();
		SizeT ie = argVData.size();
		assert(ie == argWData.size());

		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, no_tile);
		for (SizeT ii = 0; ii != ie; ++i, ++ii)
		{
			if (IsDefined(argVData[ii]))  // V TILED
			{
				SizeT p = indexGetter->Get(ii);
				if (IsDefined(p))
				{
					assert(p < nrP);
					if (IsDefined(argWData[ii]) && (argWData[ii] > 0))
					{
						indexVector[ cumul2[p] ] = i;
						++cumul2[p];
					}
					else
						--partCount[p];
					assert(cumul2[p] <= cumul[p] + partCount[p]);
				}
			}
		}
		assert(argVData.size() == argWData.size());

		for (SizeT ii = 0; ii != nrP; ++ii) // FOR EACH P
		{
			assert(cumul2[ii] == cumul[ii] + partCount[ii]);
			assert(cumul2[ii] <= ((ii+1<nrP) ? cumul[ii+1] : cumulativeN) );

			W cumulWeight = arg2Data[e2Void ? 0 : ii];

			sequence_traits<SizeT>::container_type::iterator 
				cbi = indexVector.begin() + cumul[ii];
			resData[ii] = nth::nth_element_weighted<V>(cbi, cbi + partCount[ii], argVData.begin(), argWData.begin(), cumulWeight);
		}
	}
};


// *****************************************************************************
//										median
// *****************************************************************************

CommonOperGroup cogRthElem("rth_element", oper_policy::better_not_in_meta_scripting);
typedef Float32 RatioType;

// *****************************************************************************
//											RthElementTot
// *****************************************************************************




template <class V> 
struct RthElementTot: AbstrPthElementTot<RatioType>
{
	typedef DataArray<V> ArgVType;
	typedef ArgVType     ResultType;

	RthElementTot() 
		: AbstrPthElementTot(&cogRthElem, ArgVType::GetStaticClass())
	{}

	// Override Operator
	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, RatioType r) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA);
		assert(argV);

		ResultType* result = mutable_array_cast<V>(res);
		assert(result);
		auto resData = result->GetDataWrite();
		assert(resData.size() == 1);

		SizeT N = 0;
		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);
			count_best_total(N, argVData.begin(), argVData.end());
		}
		typename sequence_traits<V>::container_type copy;
		copy.reserve(N);

		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetLockedDataRead(t);
			for (auto iv = argVData.begin(), ev = argVData.end(); iv != ev; ++iv)
			{
				if (IsDefined(*iv))
				{
					assert(copy.size() < N); // sufficient reservation
					copy.push_back(*iv);
				}
			}
		}
		assert(copy.size() == N); // neccesary reservation
		Float64 rr = r;
		rr *= (N - 1);

		SizeT pos = rr;
		if(pos < N)
		{
			auto
				cbi = copy.begin(),
				cbp = cbi + pos,
				cbe = copy.end();
			assert(cbp != cbe);
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
//											CLASSES
// *****************************************************************************


template <typename V, typename I=UInt32> 
struct RthElementPart: AbstrPthElementPart<RatioType>
{
	typedef RatioType    R;
	typedef DataArray<V> ArgVType;
	typedef DataArray<R> Arg2Type; // nth
	typedef DataArray<V> ResultType;

	RthElementPart() 
		: AbstrPthElementPart(&cogRthElem, ArgVType::GetStaticClass(), AbstrDataItem::GetStaticClass())
	{}

	// Override Operator
	void Calculate(DataWriteHandle& res, const AbstrDataItem* argVA, const DataArray<RatioType>* arg2, bool e2Void, const AbstrDataItem* argPA) const override
	{
		const ArgVType* argV = const_array_cast<V>(argVA); assert(argV);

		const AbstrUnit* valuesDomain = argVA->GetAbstrDomainUnit();
		SizeT N = 0;
		for (tile_id t=0, te=valuesDomain->GetNrTiles(); t!=te; ++t)
			N += valuesDomain->GetTileCount(t);

		SizeT nrP = argPA->GetAbstrValuesUnit()->GetCount();
		auto arg2Data = arg2->GetDataRead();
		assert(arg2Data.size() == (e2Void ? 1 : nrP));

		ResultType* result = mutable_array_cast<V>(res);
		assert(result);
		auto resData = result->GetDataWrite();
		assert(resData.size() == nrP);

		std::vector<I> partCount(nrP, 0);

		for (tile_id t=0, te=argVA->GetAbstrDomainUnit()->GetNrTiles(); t!=te; ++t)
		{
			auto argVData = argV->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(argPA, t);
			count_best_partial_best(partCount.begin(), argVData.begin(), argVData.end(), indexGetter);
		}

		// cumulative counts per partition
		I cumulativeN = 0;
		std::vector<I> cumul; cumul.reserve(nrP);
		for (auto i = partCount.begin(), e = partCount.end(); i!=e; ++i)
		{
			cumul.push_back(cumulativeN);
			cumulativeN += *i;
		}
		assert(cumulativeN <= N);
		std::vector<I> cumul2 = cumul;

		typename sequence_traits<V>::container_type copy(cumulativeN, V());
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
					{
						assert(p < nrP);

						copy[cumul2[p]++] = (*i);
						assert(cumul2[p] <= cumul[p] + partCount[p]);
					}
				}
			}
		}
		V resValue;
		for (SizeT i = 0; i != nrP; ++i)
		{
			assert(cumul2[i] == cumul[i] + partCount[i]);
			assert(cumul2[i] == ((i+1<nrP) ? cumul[i+1] : cumulativeN) );

			I pCount = partCount[i];
			Float64 rr = arg2Data[e2Void ? 0 : i];
			rr *= (pCount - 1);
			UInt32 pos = rr;
			if (pos<pCount)
			{
				typename sequence_traits<V>::container_type::iterator 
					cbi = copy.begin() + cumul[i],
					cbp = cbi + pos,
					cbe = cbi + pCount;
				assert(cbp != cbe);
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
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	template<typename ValueType>
	struct AggrOperInstances
	{
	private:
		NthElementTot<ValueType> m_NElemOperator;
		NthElementWeightedTot<ValueType, Float32> m_NElemWeighted32Operator;
		NthElementWeightedTot<ValueType, Float64> m_NElemWeighted64Operator;
		RthElementTot<ValueType> m_RElemOperator;

		NthElementPart<ValueType> m_NPartOperator;
		NthElementWeightedPart<ValueType, Float32> m_NPartW32Operator;
		NthElementWeightedPart<ValueType, Float64> m_NPartW64Operator;
		RthElementPart<ValueType>                  m_RPartOperator;
	};

	tl_oper::inst_tuple_templ<typelists::num_objects, AggrOperInstances> s_PthElemOperators;
}
