// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <numbers>
#include <cmath>

#include "geo/CheckedCalc.h"
#include "geo/Conversions.h"
#include "geo/IsNotUndef.h"
#include "mci/CompositeCast.h"
#include "set/VectorFunc.h"

#include "DataItemClass.h"
#include "Param.h"
#include "TreeItemClass.h"
#include "UnitCreators.h"

#include "OperAccUni.h"
#include "OperAccBin.h"
#include "OperRelUni.h"
#include "ValuesTable.h"
#include "IndexGetterCreator.h"



// *****************************************************************************
//											Modus Helper funcs
// *****************************************************************************

template <typename Counter, typename CIter>
CIter arg_max(CIter b, CIter e, auto countF)
{
	Counter maxC = 0; // MIN_VALUE(ACC);
	CIter maxP = e;
	for (; b != e; ++b)
	{
		auto c = countF(b);
		if (c <= maxC)
			continue;
		maxC = c; 
		maxP = b;
	}
	return maxP;
}

template <typename Counter, typename Value>
struct modusFunc {
	using result_type = Value;

	template <typename CIter>
	auto operator ()(CIter b, CIter e, auto countF, auto valueF) -> result_type
	{
		CIter p = arg_max<Counter>(b, e, countF);
		if (p == e)
			return UNDEFINED_OR_ZERO(Value);
		return valueF(p);
	}
};

template <typename Counter>
struct modusCountFunc {
	using result_type = Counter;

	template <typename CIter>
	auto operator ()(CIter b, CIter e, auto countF, auto valueF) -> result_type
	{
		CIter p = arg_max<Counter>(b, e, countF);
		if (p == e)
			return 0;
		return countF(p);
	}
};

template <typename Counter>
struct uniqueCountFunc {
	using result_type = Counter;

	template <typename CIter>
	auto operator ()(CIter b, CIter e, auto countF, auto valueF) -> result_type
	{
		Counter uniqueValueCount = 0;
		for (auto i = b; i != e; ++i)
			if (countF(i))
				SafeIncrement(uniqueValueCount);
		return uniqueValueCount;
	}
};

constexpr Float64 log2_inv = 1.0 / std::numbers::ln2_v<Float64>;

template <typename Counter>
struct entropyFunc {
	using result_type = Float64;

	template <typename CIter>
	auto operator ()(CIter b, CIter e, auto countF, auto valueF) -> result_type
	{
		Counter totalCount = 0;
		for (auto i = b; i != e; ++i)
			totalCount += countF(i);
		if (!totalCount)
			return 0;
		Float64 result = totalCount * std::log(totalCount), result2 = 0;
		for (auto i = b; i != e; ++i)
		{
			Counter c = countF(i);
			if (c)
				result2 += c * std::log(c);
		}	
		return (result - result2) * log2_inv;
	}
};

template <typename Counter>
struct average_entropyFunc {
	using result_type = Float64;

	template <typename CIter>
	auto operator ()(CIter b, CIter e, auto countF, auto valueF) -> result_type
	{
		Counter totalCount = 0;
		for (auto i = b; i != e; ++i)
			totalCount += countF(i);
		if (!totalCount)
			return 0;
		Float64 result = std::log(totalCount), result2 = 0;
		for (auto i = b; i != e; ++i)
		{
			Counter c = countF(i);
			if (c)
				result2 += c * std::log(c);
		}
		return (result - result2 / totalCount) * log2_inv;
	}
};

// *****************************************************************************
//											ModusTot
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename R, typename AggrFunc>
void ModusTotBySet(const DataArray<V>* tileFunctor, typename sequence_traits<R>::reference resData, AggrFunc aggrFunc)
{
	auto values_fta = GetFutureTileArray(tileFunctor);
	auto counters = GetWeededWallCounts<V, SizeT>(values_fta, SizeT(-1));

	resData = aggrFunc(counters.begin(), counters.end()
	,	[](auto i) { return i->second; }
	,	[](auto i) { return i->first; }
	);
}

template<typename V, typename R, typename AggrFunc>
void ModusTotByTable(const DataArray<V>* tileFunctor, typename sequence_traits<R>::reference resData,  typename Unit<V>::range_t valuesRange, AggrFunc aggrFunc)
{
	auto buffer = GetCountsAsArray<V, SizeT>(tileFunctor, valuesRange);

	resData = aggrFunc(buffer.begin(), buffer.end()
	, [ ](auto i) { return *i; }
	, [&](auto i) { return Range_GetValue_naked(valuesRange, i - buffer.begin()); }
	);
}

// make tradeoff between 
//      ModusPartTable: O(n+v*p)           processing with O(v*p) temp memory
//	and ModusPartSet:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
// when v is not countable(such as string, float), always choose the second method
//
// Note that ModusTotal is a special case of ModusPatial with p=1
//
// When memory condition doesn't favour Set: n >= v*p
// then Table time O(n+v*p) <= O(2n) < O(n*log(min(n,v))
// Thus, tradeof is made at v*p <= n.

template<typename V>template <typename V> using map_node_type = std::_Tree_node<std::pair<std::pair<SizeT, V>, SizeT>, void*>;
template <typename V> constexpr UInt32 map_node_type_size = sizeof(map_node_type<V>);

template <typename V, typename R, typename AggrFunc>
void ModusTotDispatcher(const DataArray<V>* valuesTF, bool noOutOfRangeValues, typename sequence_traits<R>::reference resData, AggrFunc aggrFunc)
{
	if constexpr (is_bitvalue_v<scalar_of_t<V>>)
	{
		ModusTotByTable<V, R>(valuesTF, resData, GetValuesRange<V>(valuesTF), aggrFunc);
	}
	else
	{
		if constexpr (is_integral_v<scalar_of_t<V>>)
		{
			if (noOutOfRangeValues)
			{
				typename Unit<V>::range_t valuesRange = GetValuesRange<V>(valuesTF);
				// Countable values; go for Table if sensible
				auto n = valuesTF->GetTiledRangeData()->GetDataSize();
				auto v = Cardinality(valuesRange);

				if (IsDefined(v) && (v / map_node_type_size<V> <= n / sizeof(V)))  // memory condition v*p<=n, thus TableTime <= 2n.
				{
					ModusTotByTable<V, R>(valuesTF, resData, valuesRange, aggrFunc);
					return;
				}
			}
		}
		ModusTotBySet<V, R>(valuesTF, resData, aggrFunc);
	}
}

// *****************************************************************************
//									ModusPart
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV, typename AggrFunc>
void ModusPartBySet(const AbstrDataItem* indicesItem, abstr_future_tile_array part_fta
	, future_tile_array<V> values_fta
	, OIV resBegin, SizeT pCount, AggrFunc aggrFunc)  // countable dommain unit of result; P can be Void.
{
	assert(values_fta.size() == part_fta.size());

	using value_type = std::pair<SizeT, V>;
	auto tn = values_fta.size();

	auto counters = GetIndexedWallCounts<V, SizeT>(values_fta
		, indicesItem, part_fta
		, 0, tn, pCount);

	auto i = counters.begin(), e = counters.end();
	auto ri = 0;
	auto getCount = [](auto counterPtr) { return counterPtr->second; };
	auto getValue = [](auto counterPtr) { return counterPtr->first.second; };
	while (i != e)
	{
		SizeT p = i->first.first;
		auto pb = i;
		while (++i != e)
			if (i->first.first != p)
				break;
		while (ri < p)
			resBegin[ri++] = aggrFunc(pb, pb, getCount, getValue);
		resBegin[p] = aggrFunc(pb, i, getCount, getValue);
		ri = p + 1;
	}
	while (ri < pCount)
		resBegin[ri++] = aggrFunc(e, e, getCount, getValue);
}

template<typename V, typename OIV, typename AggrFunc>
void ModusPartByTable(const AbstrDataItem* indicesItem, future_tile_array<V> values_fta, abstr_future_tile_array part_afta
	, OIV resBegin, typename Unit<V>::range_t valuesRange, SizeT pCount  // countable dommain unit of result; P can be Void.
	, AggrFunc aggrFunc)
{
	SizeT vCount = Cardinality(valuesRange);
	std::vector<SizeT> buffer(vCount*pCount, 0);
	auto bufferB = buffer.begin();

	for (tile_id t =0, tn = values_fta.size(); t != tn; ++t)
	{
		auto values = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		auto valuesIter = values.begin(),
		     valuesEnd  = values.end();

		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, part_afta[t]); part_afta[t] = nullptr;
		SizeT i=0;

		for (; valuesIter != valuesEnd; ++i, ++valuesIter)
		{
			UInt32 vi = Range_GetIndex_naked(valuesRange, *valuesIter);
			if (vi >= vCount)
				continue;
			SizeT pi = indexGetter->Get(i);
			if (pi >= pCount)
				continue;
			SafeIncrementCounter(bufferB[ pi * vCount + vi]);
		}
	}

	for (OIV resEnd = resBegin + pCount; resBegin != resEnd; ++resBegin)
	{
		*resBegin = aggrFunc(bufferB, bufferB + vCount
		, [ ](auto i) { return *i; }
		, [&](auto i) { return Range_GetValue_naked(valuesRange, i - bufferB); }
		);

		bufferB += vCount;
	}
	assert(bufferB == buffer.end());
}

// *****************************************************************************
//											WeightedModusTot
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V>
void WeightedModusTotBySet(const DataArray<V>* valuesTF, const AbstrDataItem* weightItem, typename sequence_traits<V>::reference resData)
{
	std::map<V, Float64> counters;

	for (tile_id t =0, tn = valuesTF->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = valuesTF->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(), valuesEnd   = valuesLock.end();
		OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem, t);

		SizeT weightsIter = 0;
		for (; valuesIter != valuesEnd; ++weightsIter, ++valuesIter)
			if (IsDefined(*valuesIter))
				counters[*valuesIter] += weightsGetter->Get(weightsIter);
	}


	modusFunc<Float64, V> aggrFunc;

	resData = aggrFunc(counters.begin(), counters.end()
	, [](auto i) { return i->second; }
	, [](auto i) { return i->first; }
	);

}

template<typename V>
void WeightedModusTotByTable(const DataArray<V>* valuesTF, const AbstrDataItem* weightItem, typename sequence_traits<V>::reference resData, const typename Unit<V>::range_t& valuesRange)
{
	auto vCount = Cardinality(valuesRange);
	std::vector<Float64> buffer(vCount, 0);

	for (tile_id t =0, tn = valuesTF->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = valuesTF->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();
		OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator(weightItem, t).Create();

		SizeT weightIter  = 0;

		for (; valuesIter != valuesEnd; ++weightIter, ++valuesIter)
		{
			UInt32 v = Range_GetIndex_checked(valuesRange, *valuesIter);
			if (v < vCount)
				buffer[v] += weightsGetter->Get(weightIter);
		}
	}

	modusFunc<Float64, V> aggrFunc;

	resData = aggrFunc(buffer.begin(), buffer.end()
	, [ ](auto i) { return *i; }
	, [&](auto i) { return Range_GetValue_naked(valuesRange, i - buffer.begin()); }
	);
}

// make tradeoff between 
//      ModusPartTable: O(n+v*p)           processing with O(v*p) temp memory
//	and ModusPartSet:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
// when v is not countable(such as string, float), always choose the second method
//
// Note that ModusTotal is a special case of ModusPatial with p=1
//
// When memory condition doesnt favour Set: n >= v*p
// then Table time O(n+v*p) <= O(2n) < O(n*log(min(n,v))
// Thus, tradeof is made at v*p <= n.

template<typename V>
void WeightedModusTotDispatcher(const DataArray<V>* valuesTF, bool noOutOfRangeValues, const AbstrDataItem* weightItem, typename sequence_traits<V>::reference resData)
{
	if constexpr (is_bitvalue_v<scalar_of_t<V>>)
	{
		WeightedModusTotByTable<V>(valuesTF, weightItem, resData, GetValuesRange<V>(valuesTF));
	}
	else
	{
		if constexpr (is_integral_v<scalar_of_t<V>>)
		{
			if (noOutOfRangeValues)
			{
				auto valuesRange = GetValuesRange<V>(valuesTF);

				// Countable values; go for Table if sensible
				auto n = valuesTF->GetTiledRangeData()->GetDataSize();
				auto v = Cardinality(valuesRange);

				if (IsDefined(v) && (v / map_node_type_size<V> <= n / sizeof(V))) // memory condition v*p<=n, thus TableTime <= 2n.
				{
					WeightedModusTotByTable<V>(valuesTF, weightItem, resData, valuesRange);
					return;
				}
			}
		}
		WeightedModusTotBySet<V>(valuesTF, weightItem, resData);
	}
}

// *****************************************************************************
//											WeightedModusPart
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV>
void WeightedModusPartBySet(const DataArray<V>* valuesTF, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem,
	OIV resBegin, 
	SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	typedef std::pair<SizeT, V> value_type;
	std::map<value_type, Float64> wieghtAccumulators;

	for (tile_id t=0, tn= valuesTF->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = valuesTF->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
			 valuesEnd   = valuesLock.end();

		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, t);
		OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem, t);

		SizeT i=0;
		for (; valuesIter != valuesEnd; ++i, ++valuesIter)
			if (IsDefined(*valuesIter))
			{
				Float64 weight = weightsGetter->Get(i);
				if (IsDefined(weight))
				{
					SizeT p = indexGetter->Get(i);
					if (IsDefined(p))
					{
						assert(p < pCount);
						wieghtAccumulators[value_type(p, *valuesIter)] += weight;
					}
				}
			}
	}
	modusFunc<SizeT, V> aggrFunc;
	auto getCount = [](auto counterPtr) { return counterPtr->second; };
	auto getValue = [](auto counterPtr) { return counterPtr->first.second; };

	auto i = wieghtAccumulators.begin(), e = wieghtAccumulators.end();
	auto ri = 0;
	while (i != e)
	{
		SizeT p = i->first.first;
		auto pb = i;
		while (++i != e)
			if (i->first.first != p)
				break;
		while (ri < p)
			resBegin[ri++] = aggrFunc(pb, pb, getCount, getValue);
		resBegin[p] = aggrFunc(pb, i, getCount, getValue);
		ri = p + 1;
	}
	while (ri < pCount)
		resBegin[ri++] = aggrFunc(e, e, getCount, getValue);
}

template<typename V, typename OIV>
void WeightedModusPartByTable(const DataArray<V>* valuesTF, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem, OIV resBegin, typename Unit<V>::range_t valuesRange, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	SizeT vCount = Cardinality(valuesRange);
	std::vector<Float64> buffer(vCount*pCount, 0);
	std::vector<Float64>::iterator bufferB = buffer.begin();

	for (tile_id t =0, tn = valuesTF->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = valuesTF->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();

		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, t);
		OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem, t);

		SizeT weightIter = 0;
		Float64 weight;
		SizeT i=0;
		for (; valuesIter != valuesEnd; ++weightIter, ++i, ++valuesIter)
			if (IsDefined(*valuesIter) && IsDefined(weight = weightsGetter->Get(weightIter)))
			{
				assert(IsIncluding(valuesRange, *valuesIter)); // PRECONDITION
				UInt32 vi = Range_GetIndex_naked(valuesRange, *valuesIter);
				assert(vi < vCount);
				SizeT pi = indexGetter->Get(i);
				if (IsNotUndef(pi))
				{
					assert(pi < pCount);
					bufferB[pi * vCount + vi] += weight;
				}
			}
	}

	modusFunc<Float64, V> aggrFunc;

	for (OIV resEnd = resBegin + pCount; resBegin != resEnd; ++resBegin)
	{
		*resBegin = aggrFunc(bufferB, bufferB + vCount
		, [ ](auto i) { return *i; }
		, [&](auto i) { return Range_GetValue_naked(valuesRange, i - bufferB); }
		);

		bufferB += vCount;
	}
	assert(bufferB == buffer.end());
}

// make tradeoff between 
//      ModusPartTable: O(n+v*p)           processing with O(v*p) temp memory
//	and ModusPartSet:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
// when v is not countable(such as string, float), always choose the second method
//
// Note that ModusTotal is a special case of ModusPatial with p=1
//
// When memory condition doesnt favour Set: n >= v*p
// then Table time O(n+v*p) <= O(2n) < O(n*log(min(n,v))
// Thus, tradeof is made at v*p <= n.

template<typename V, typename OIV>
void WeightedModusPartDispatcher(const DataArray<V>* valuesTF, bool noOutOfRangeValues, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem, OIV resBegin, SizeT nrP)
{
	if constexpr (is_bitvalue_v<scalar_of_t<V>>)
	{
		WeightedModusPartByTable<V>(valuesTF, weightItem, indicesItem, resBegin, GetValuesRange<V>(valuesTF), nrP);
	}
	else
	{
		if constexpr (is_integral_v<scalar_of_t<V>>)
		{
			if (noOutOfRangeValues)
			{
				auto valuesRange = GetValuesRange<V>(valuesTF);

				assert(IsDefined(valuesRange)); //we already made result with p as domainUnit, thus count must be known and managable.
				assert(!valuesRange.empty());   //we already made result with p as domainUnit, thus count must be known and managable.

				// Countable values; go for Table if sensible
				auto n = valuesTF->GetTiledRangeData()->GetDataSize();
				auto v = valuesRange.empty() ? MAX_VALUE(row_id) : Cardinality(valuesRange);

				assert(IsNotUndef(nrP)); //consequence of the checks on indexRange: values Unit of index has been used as domain of the result

				if (IsDefined(v) && (!nrP || v <= n / nrP)) // memory condition v*p<=n, thus TableTime <= 2n.
				{
					WeightedModusPartByTable<V>(valuesTF, weightItem, indicesItem, resBegin, valuesRange, nrP);
					return;
				}
			}
		}
		WeightedModusPartBySet<V>(valuesTF, weightItem, indicesItem, resBegin, nrP);
	}
}

template <typename V, typename AggrFunc>
struct ModusTotal : AbstrOperAccTotUni
{
	using ValueType = V;
	using ResultValueType = typename AggrFunc::result_type;
	using Arg1Type = DataArray<ValueType>;   // value vector
	using ResultType = DataArray<ResultValueType>; // will contain the first most occuring value
			
public:
	ModusTotal(AbstrOperGroup* gr, UnitCreatorPtr ucp)
		:	AbstrOperAccTotUni(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), ucp, COMPOSITION(ResultType))
	{}

	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, ArgRefs args, std::vector<ItemReadLock> readLocks) const override
	{
		ResultType* result = mutable_array_cast<ResultValueType>(res);
		assert(result);
		auto  resData = result->GetDataWrite();

		ModusTotDispatcher<V, ResultValueType>(const_array_cast<V>(arg1A), OnlyDefinedCheckRequired(arg1A), resData[0], m_AggrFunc);
	}
	AggrFunc m_AggrFunc;
};

template <typename V> 
struct WeightedModusTotal : AbstrOperAccTotBin
{
	typedef V             ValueType;
	typedef DataArray<V>  Arg1Type;   // value vector
	typedef AbstrDataItem Arg2Type;   // weight vector
	typedef DataArray<V>  ResultType; // will contain the first most occuring value
			
public:
	WeightedModusTotal(AbstrOperGroup* gr) 
		:	AbstrOperAccTotBin(gr
			,	ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			,	arg1_values_unit, COMPOSITION(V)
			)
	{}

	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A) const override
	{
		ResultType* result = mutable_array_cast<V>(res);
		dms_assert(result);
		auto resData = result->GetDataWrite();

		WeightedModusTotDispatcher<V>(const_array_cast<V>(arg1A), OnlyDefinedCheckRequired(arg1A), arg2A, resData[0]);
	}
};

template <typename V, typename AggrFunc>
struct ModusPart : OperAccPartUniWithCFTA<V, typename AggrFunc::result_type>
{
	typedef V                     ValueType;
	typedef DataArray<ValueType>  Arg1Type;   // value vector
	typedef AbstrDataItem         Arg2Type;   // index vector
	using ResultValueType = typename AggrFunc::result_type;
	typedef DataArray<ResultValueType>  ResultType; // will contain the first most occuring value per index value
	using base_type = OperAccPartUniWithCFTA<V, ResultValueType>;
	using ProcessDataInfo = base_type::ProcessDataInfo;

	ModusPart(AbstrOperGroup* gr, UnitCreatorPtr ucp)
		: base_type(gr, ucp)
	{}

	void ProcessData(ResultType* result, ProcessDataInfo& pdi) const override
	{
		assert(result);
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
		dbg_assert(resData.size()  == pdi.arg2A->GetAbstrValuesUnit()->GetCount());
		auto resBegin = resData.begin();

		if constexpr(is_bitvalue_v<V>)
		{
			ModusPartByTable<V>(pdi.arg2A, std::move(pdi.values_fta), std::move(pdi.part_fta), resBegin, typename Unit<V>::range_t(0, 1 << nrbits_of_v<V>), pdi.resCount, m_AggrFunc);
		}
		else
		{
			// make tradeoff between 
			//      ModusPartTable: O(n+v*p)           processing with O(v*p) temp memory
			//	and ModusPartSet:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
			// when v is not countable(such as string, float), always choose the second method
			//
			// Note that ModusTotal is a special case of ModusPatial with p=1
			//
			// When memory condition doesnt favour Set: n >= v*p
			// then Table time O(n+v*p) <= O(2n) < O(n*log(min(n,v))
			// Thus, tradeof is made at v*p <= n.

			// Countable values; go for Table if sensible
			SizeT v = MAX_VALUE(SizeT);
			if constexpr (is_integral_v<scalar_of_t<V>>)
			{
				auto range = pdi.valuesRangeData->GetRange();
				if (!range.empty())
					v =  Cardinality(range);
			}

			assert(IsNotUndef(pdi.resCount)); //consequence of the checks on indexRange

			if (IsDefined(v)
				//		&& (!resCount || v / map_node_type_size<V> <= n / resCount / sizeof(SizeT))
				&& (!pdi.resCount || v <= pdi.n / pdi.resCount)
				) // memory condition v*p<=n, thus TableTime <= 2n.
				ModusPartByTable<V>(pdi.arg2A, std::move(pdi.values_fta), std::move(pdi.part_fta), resBegin, pdi.valuesRangeData->GetRange(), pdi.resCount, m_AggrFunc);
			else
				ModusPartBySet<V>(pdi.arg2A, std::move(pdi.part_fta), std::move(pdi.values_fta), resBegin, pdi.resCount, m_AggrFunc);
		}
	}

	AggrFunc m_AggrFunc;
};

template <typename V> 
struct WeightedModusPart : public AbstrOperAccPartBin
{
	typedef V                     ValueType;
	typedef DataArray<ValueType>  Arg1Type;   // value vector
	typedef AbstrDataItem         Arg2Type;   // weight vector
	typedef AbstrDataItem         Arg3Type;   // index vector
	typedef DataArray<ValueType>  ResultType; // will contain the first most occuring value per index value
			
	WeightedModusPart(AbstrOperGroup* gr) 
		:	AbstrOperAccPartBin(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), Arg3Type::GetStaticClass(), arg1_values_unit, COMPOSITION(ValueType))
	{}

	// Override Operator
	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A) const override
	{
		auto result = mutable_array_cast<ValueType>(res); assert(result);
		auto resData = result->GetLockedDataWrite();

		assert(resData.size() == res->GetTiledRangeData()->GetRangeSize()); // DataWriteLock was set by caller and p3 is domain of res

		WeightedModusPartDispatcher<V>(const_array_cast<V>(arg1A), OnlyDefinedCheckRequired(arg1A), arg2A, arg3A, resData.begin(), arg3A->GetAbstrValuesUnit()->GetCount());
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	CommonOperGroup cogEntropy("entropy", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogAvgEntropy("average_entropy", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogModusCount08("modus_count_uint8", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogModusCount16("modus_count_uint17", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogModusCount32("modus_count_uint32", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogModusCount64("modus_count_uint64", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogUniqueCount08("unique_count_uint8", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogUniqueCount16("unique_count_uint16", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogUniqueCount32("unique_count_uint32", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogUniqueCount64("unique_count_uint64", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogModus("modus", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogModusW("modus_weighted", oper_policy::better_not_in_meta_scripting);

	template <typename V, typename AggrFunc>
	struct AggrFuncInst
	{
		AggrFuncInst(AbstrOperGroup& aog, UnitCreatorPtr ucp)
			: mt(&aog, ucp)
			, mp(&aog, ucp)
		{}

	private:
		ModusTotal<V, AggrFunc> mt;
		ModusPart <V, AggrFunc> mp;
	};

	template <typename V>
	struct AggrFuncsInst
	{
		AggrFuncsInst()
			: m_ModusFunc(cogModus, arg1_values_unit)
			, m_ModusCountFunc08(cogModusCount08, default_unit_creator<UInt8>)
			, m_ModusCountFunc16(cogModusCount16, default_unit_creator<UInt16>)
			, m_ModusCountFunc32(cogModusCount32, default_unit_creator<UInt32>)
			, m_ModusCountFunc64(cogModusCount64, default_unit_creator<UInt64>)
			, m_UniqueCountFunc08(cogUniqueCount08, default_unit_creator<UInt8>)
			, m_UniqueCountFunc16(cogUniqueCount16, default_unit_creator<UInt16>)
			, m_UniqueCountFunc32(cogUniqueCount32, default_unit_creator<UInt32>)
			, m_UniqueCountFunc64(cogUniqueCount64, default_unit_creator<UInt64>)
			, m_EntropyFunc(cogEntropy, default_unit_creator<Float64>)
			, m_AvgEntropyFunc(cogAvgEntropy, default_unit_creator<Float64>)
		{}

	private:
		AggrFuncInst<V, modusFunc<SizeT, V> > m_ModusFunc;

		AggrFuncInst<V, modusCountFunc<UInt8 > > m_ModusCountFunc08;
		AggrFuncInst<V, modusCountFunc<UInt16> > m_ModusCountFunc16;
		AggrFuncInst<V, modusCountFunc<UInt32> > m_ModusCountFunc32;
		AggrFuncInst<V, modusCountFunc<UInt64> > m_ModusCountFunc64;

		AggrFuncInst<V, uniqueCountFunc<UInt8 > > m_UniqueCountFunc08;
		AggrFuncInst<V, uniqueCountFunc<UInt16> > m_UniqueCountFunc16;
		AggrFuncInst<V, uniqueCountFunc<UInt32> > m_UniqueCountFunc32;
		AggrFuncInst<V, uniqueCountFunc<UInt64> > m_UniqueCountFunc64;

		AggrFuncInst<V, entropyFunc<SizeT> > m_EntropyFunc;
		AggrFuncInst<V, average_entropyFunc<SizeT> > m_AvgEntropyFunc;
	};

	template <typename V>
	struct WeightedModusInst
	{
		WeightedModusInst()
			: wmt(&cogModusW)
			, wmp(&cogModusW)
		{}

	private:
		WeightedModusTotal<V> wmt;
		WeightedModusPart<V> wmp;
	};

	tl_oper::inst_tuple<typelists::aints, AggrFuncsInst<_> > aggrOpers;
	tl_oper::inst_tuple<typelists::aints, WeightedModusInst<_> > weigthedModusOpers;

//	ModusInst<SharedStr> mpString; //TODO, ook TODO: optimize dispatchers voor (U)Int4/2
}
