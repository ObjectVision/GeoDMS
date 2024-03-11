// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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

#include "OperAccUni.h"
#include "OperAccBin.h"
#include "OperRelUni.h"
#include "UnitCreators.h"
#include "IndexGetterCreator.h"



template<typename R> void SafeIncrementCounter(R& assignee)
{
	SafeIncrement(assignee);
}

void SafeIncrementCounter(SizeT& assignee)
{
	assignee++;
	assert(assignee); // SizeT cannot overflow when counting distict addressable elements
}

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

bool OnlyDefinedCheckRequired(const AbstrDataItem* adi)
{
	DataCheckMode dcm = adi->GetCheckMode();
	return !(dcm & DCM_CheckDefined);
}

/* 
template <typename V>
typename Unit<V>::range_t 
GetRange(const DataArray<V>* da)
{
	return da->GetValueRangeData()->GetRange();
}
*/

template <typename V>
typename Unit<V>::range_t
GetRange(const AbstrDataItem* adi)
{
	return debug_cast<const Unit<V>*>(adi->GetAbstrValuesUnit())->GetRange();
}

// *****************************************************************************
//											ModusTot
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename R, typename AggrFunc>
void ModusTotBySet(const AbstrDataItem* valuesItem, typename sequence_traits<R>::reference resData, AggrFunc aggrFunc)
{
	std::map<V, SizeT> counters;

	auto values_fta = (DataReadLock(valuesItem), GetFutureTileArray(const_array_cast<V>(valuesItem)));
	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		auto valuesIter = valuesLock.begin(),
		     valuesEnd  = valuesLock.end();

		for (; valuesIter != valuesEnd; ++valuesIter)
		{
			if (!IsDefined(*valuesIter))
				continue;
			SafeIncrementCounter(counters[*valuesIter]);
		}
	}

	resData = aggrFunc(counters.begin(), counters.end()
	,	[](auto i) { return i->second; }
	,	[](auto i) { return i->first; }
	);
}

/* REMOVE
// assume v >> n; time complexity: n*log(min(v, n))
template<typename V>
void ModusTotByIndex(const AbstrDataItem* valuesItem, typename sequence_traits<V>::reference resData)
{
	DataReadLock lock(valuesItem);
	auto valuesLock  = const_array_cast<V>(valuesItem)->GetLockedDataRead();
	auto valuesBegin = valuesLock.begin(),
	     valuesEnd   = valuesLock.end();

	SizeT n = valuesEnd - valuesBegin;
	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("ModusTotByIndex: index"));
	auto i = index.begin(), e = index.end(); assert(e - i == n);
	make_index_in_existing_span(i, e, valuesBegin);

	SizeT maxC = 0;
	resData = UNDEFINED_VALUE(V);
	while (i != e)
	{
		decltype(valuesBegin) vPtr = valuesBegin + *i; V v = *vPtr;
		if (IsDefined(v))
		{
			SizeT c = 1;
			while (++i != e && valuesBegin[*i] == v)
				++c;
			dms_assert(c > 0);
			if (c > maxC)
			{
				maxC = c;
				resData = v;
			}
		}
		else
			while (++i != e && valuesBegin[*i] == v)
				;
	};
}

template<typename V>
void ModusTotByIndexOrSet(
	const AbstrDataItem* valuesItem,
	typename sequence_traits<V>::reference resData)
{
	if (valuesItem->GetAbstrDomainUnit()->IsCurrTiled())
		ModusTotBySet  <V, R>(valuesItem, resData);
	else
		ModusTotByIndex<V>(valuesItem, resData);
}

REMOVE */

template<typename V, typename R, typename AggrFunc>
void ModusTotByTable(const AbstrDataItem* valuesItem, typename sequence_traits<R>::reference resData,  typename Unit<V>::range_t valuesRange, AggrFunc aggrFunc)
{
	SizeT vCount = Cardinality(valuesRange);
	std::vector<SizeT> buffer(vCount, 0);
	auto bufferB = buffer.begin();

	auto values_fta = (DataReadLock(valuesItem), GetFutureTileArray(const_array_cast<V>(valuesItem)));

	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();
		for (; valuesIter != valuesEnd; ++valuesIter)
		{
			if (IsDefined(*valuesIter))
			{
				auto i = Range_GetIndex_naked(valuesRange, *valuesIter);
				assert(i < vCount);
				SafeIncrementCounter(bufferB[i]);
			}
		}
	}
	resData = aggrFunc(buffer.begin(), buffer.end()
	, [ ](auto i) { return *i; }
	, [&](auto i) { return Range_GetValue_naked(valuesRange, i - buffer.begin()); }
	);
}

template<typename V, typename R, typename AggrFunc>
void ModusTotDispatcher(const AbstrDataItem* valuesItem, typename sequence_traits<R>::reference resData, AggrFunc aggrFunc, const inf_type_tag*)
{
	// NonCountable values; go for Set implementation
	ModusTotBySet<V, R>(valuesItem, resData);
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

template<typename V, typename R, typename AggrFunc>
void ModusTotDispatcher(const AbstrDataItem* valuesItem, typename sequence_traits<R>::reference resData, AggrFunc aggrFunc, const bool_type_tag*)
{
	ModusTotByTable<V, R>(valuesItem, resData, GetRange<V>(valuesItem), aggrFunc);
}

template <typename V, typename R, typename AggrFunc>
void ModusTotDispatcher(const AbstrDataItem* valuesItem, typename sequence_traits<R>::reference resData, AggrFunc aggrFunc, const int_type_tag*)
{
	typename Unit<V>::range_t valuesRange = GetRange<V>( valuesItem );
	// Countable values; go for Table if sensible
	SizeT
		n = valuesItem->GetAbstrDomainUnit()->GetCount(),
		v = Cardinality(valuesRange);

	if	(	IsDefined(v)
		&&	(v / map_node_type_size<V> <= n / sizeof(V))
		&& OnlyDefinedCheckRequired(valuesItem) // memory condition v*p<=n, thus TableTime <= 2n.
		)
		ModusTotByTable<V, R>(valuesItem, resData, valuesRange, aggrFunc);
	else
		ModusTotBySet<V, R>(valuesItem, resData, aggrFunc);
}

// *****************************************************************************
//									ModusPart
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV, typename AggrFunc>
void ModusPartBySet(const AbstrDataItem* indicesItem, future_tile_array<V> values_fta, abstr_future_tile_array part_fta, OIV resBegin, SizeT pCount, AggrFunc aggrFunc)  // countable dommain unit of result; P can be Void.
{
	assert(values_fta.size() == part_fta.size());

	using value_type = std::pair<SizeT, V>;
	std::map<value_type, SizeT> counters;

	for (tile_id t=0, tn= values_fta.size(); t!=tn; ++t)
	{
		auto valuesLock = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, part_fta[t]); part_fta[t] = nullptr;
		auto valuesIter  = valuesLock.begin(),
			 valuesEnd   = valuesLock.end();
		SizeT i=0;
		for (; valuesIter != valuesEnd; ++i, ++valuesIter)
			if (IsDefined(*valuesIter))
			{
				SizeT pi = indexGetter->Get(i);
				if (IsDefined(pi))
				{
					assert(pi < pCount);
					++counters[value_type(pi, *valuesIter)];
				}
			}
	}
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

/* REMOVE
// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV>
void ModusPartByIndex(const AbstrDataItem* indicesItem, typename DataArray<V>::locked_cseq_t values, abstr_future_tile* part_ft, OIV resBegin, SizeT pCount)
{
	auto valuesBegin = values.begin();
	auto valuesEnd   = values.end();

	OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, part_ft);

	SizeT n = valuesEnd - valuesBegin;

	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("ModusPartByIndex: index"));
	auto i = index.begin(), e = index.end(); assert(e - i == n);
	make_indexP_in_existing_span(i, e, indexGetter, valuesBegin);

	while (i != e)
	{
		SizeT p = indexGetter->Get(*i);
		if (!IsDefined(p))
		{
			++i;
			continue;
		}
		else
		{
			dms_assert(p < pCount);
			SizeT maxC = 0;
			do 
			{
				decltype(valuesBegin) vPtr = valuesBegin + *i; V v = *vPtr;
				if (IsDefined(v))
				{
					SizeT c = 1;
					while (	++i != e &&	valuesBegin[*i] == v && indexGetter->Get(*i) == p )
						++c;
					dms_assert(c>0);
					if ( c > maxC)
					{
						maxC = c;
						resBegin[p] = v;
					}
				}
				else
				{
					while	(	++i != e 
							&&	valuesBegin[*i]   == v
							&&	indexGetter->Get(*i) == p 
							)
						;
				}
			}	while (i != e && indexGetter->Get(*i) == p);
		}
	}
}

template<typename V, typename OIV>
void ModusPartByIndexOrSet(const AbstrDataItem* indicesItem, future_tile_array<V> values_fta, abstr_future_tile_array part_fta, OIV resBegin, SizeT nrP)  // countable dommain unit of result; P can be Void.
{
	fast_fill(resBegin, resBegin+nrP, UNDEFINED_OR_ZERO(V));

	assert(values_fta.size() == part_fta.size());
	if (values_fta.size() != 1)
		ModusPartBySet  <V, OIV>(indicesItem, std::move(values_fta), std::move(part_fta), resBegin, nrP);
	else
		ModusPartByIndex<V, OIV>(indicesItem, values_fta[0]->GetTile(), part_fta[0], resBegin, nrP);
}
*/

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
void WeightedModusTotBySet(const AbstrDataItem* valuesItem, const AbstrDataItem* weightItem, typename sequence_traits<V>::reference resData)
{
	std::map<V, Float64> counters;

	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = const_array_cast<V>(valuesItem)->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();
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
/* REMOVE
// assume v >> n; time complexity: n*log(min(v, n))
template<typename V>
void WeightedModusTotByIndex(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	typename sequence_traits<V>::reference resData)
{
	auto valuesLock    = const_array_cast<V>(valuesItem)->GetLockedDataRead();
	auto valuesBegin   = valuesLock.begin(),
	     valuesEnd     = valuesLock.end();
	OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem);

	auto n = valuesEnd - valuesBegin;
	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("WeightModusTotByIndex: index"));
	auto i = index.begin(), e = index.end(); assert(e - i == n);
	make_index_in_existing_span(i, e, valuesBegin);

	Float64 maxC = MIN_VALUE(Float64);
	resData = UNDEFINED_OR_ZERO(V);

	while (i != e)
	{
		decltype(valuesBegin) vPtr = valuesBegin + *i; V v = *vPtr;
		if (IsDefined(v))
		{
			Float64 c = 0;
			do	{
				Float64 w = weightsGetter->Get(*i);
				if (IsDefined(w))
					c += w;
			}	while (++i != e && valuesBegin[*i] == v);
			if (c > maxC)
			{
				maxC = c;
				resData = v;
			}
		}
		else
			while (++i != e && valuesBegin[*i] == v)
				;
	};
}

template<typename V>
void WeightedModusTotByIndexOrSet(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	typename sequence_traits<V>::reference resData)
{
	if (valuesItem->GetAbstrDomainUnit()->IsCurrTiled())
		WeightedModusTotBySet  <V>(valuesItem, weightItem, resData);
	else
		WeightedModusTotByIndex<V>(valuesItem, weightItem, resData);
}
*/

template<typename V>
void WeightedModusTotByTable(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	typename sequence_traits<V>::reference resData, 
	const typename Unit<V>::range_t& valuesRange)
{
	UInt32 vCount = Cardinality(valuesRange);
	std::vector<Float64> buffer(vCount, 0);

	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = const_array_cast<V>(valuesItem)->GetLockedDataRead(t);
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

template<typename V>
void WeightedModusTotDispatcher(
	const AbstrDataItem* valuesItem
,	const AbstrDataItem* weightItem
,	typename sequence_traits<V>::reference resData
,	const inf_type_tag*
)
{
	// NonCountable values; go for Set implementation
	WeightedModusTotBySet<V>(
		valuesItem
	,	weightItem
	,	resData
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
void WeightedModusTotDispatcher(
	const AbstrDataItem* valuesItem
,	const AbstrDataItem* weightItem
,	typename sequence_traits<V>::reference  resData
,	const ord_type_tag*
)
{
	typename Unit<V>::range_t valuesRange = GetRange<V>( valuesItem );

	// Countable values; go for Table if sensible
	UInt32
		n = valuesItem->GetAbstrDomainUnit()->GetCount(),
		v = Cardinality(valuesRange);

	if	(	IsDefined(v)
		&&	v <= n
		&&	OnlyDefinedCheckRequired(valuesItem) // memory condition v*p<=n, thus TableTime <= 2n.
		)
		WeightedModusTotByTable<V>(
			valuesItem
		,	weightItem
		,	resData
		,	valuesRange
		);
	else
		WeightedModusTotBySet<V>(
			valuesItem
		,	weightItem
		,	resData
		);
}

template<typename V>
void WeightedModusTotDispatcher(
	const AbstrDataItem* valuesItem
,	const AbstrDataItem* weightItem
,	typename sequence_traits<V>::reference  resData
,	const bool_type_tag*
)
{
	WeightedModusTotByTable<V>(
		valuesItem
	,	weightItem
	,	resData
	,	GetRange<V>(valuesItem)
	);
}

// *****************************************************************************
//											WeightedModusPart
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV>
void WeightedModusPartBySet(
	const AbstrDataItem* valuesItem, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem,
	OIV resBegin, 
	SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	typedef std::pair<SizeT, V> value_type;
	std::map<value_type, Float64> counters;

	for (tile_id t=0, tn= valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = const_array_cast<V>(valuesItem )->GetLockedDataRead(t);
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
						counters[value_type(p, *valuesIter)] += weight;
					}
				}
			}
	}
	modusFunc<SizeT, V> aggrFunc;
	auto getCount = [](auto counterPtr) { return counterPtr->second; };
	auto getValue = [](auto counterPtr) { return counterPtr->first.second; };

	auto i = counters.begin(), e = counters.end();
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

/* REMOVE
// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV>
void WeightedModusPartByIndex(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	const AbstrDataItem* indicesItem,
	OIV resBegin, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	auto valuesLock  = const_array_cast<V>(valuesItem )->GetLockedDataRead();
	auto valuesBegin = valuesLock.begin(),
	     valuesEnd   = valuesLock.end();

	OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, no_tile);
	OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem);

	SizeT n = valuesEnd - valuesBegin;
	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("WeightedModusPartByIndex: index"));
	auto i = index.begin(), e = index.end(); assert(e - i == n);
	make_indexP_in_existing_span(i, e, indexGetter, valuesBegin);

	while (i != e)
	{
		SizeT p = indexGetter->Get(*i);
		if (!IsDefined(p))
		{
			++i;
			continue;
		}
		else
		{
			dms_assert(p < pCount);
			Float64 maxC = MIN_VALUE(Float64);
			do
			{
				auto vPtr = valuesBegin + *i; V v = *vPtr;
				if (IsDefined(*vPtr))
				{
					Float64 c = 0;
					do	{
						Float64 w = weightsGetter->Get(*i);
						if (IsDefined(w))
							c += w; 
					}	while (++i != e && valuesBegin[*i]   == v && indexGetter->Get(*i) == p);
					if ( c > maxC )
					{
						maxC = c;
						resBegin[p] = v;
					}
				}
				else
				{
					while (++i != e && valuesBegin[*i] == v && indexGetter->Get(*i) == p)
						;
				}
			}	while (i != e && indexGetter->Get(*i) == p);
		}
	}
}

template<typename V, typename OIV>
void WeightedModusPartByIndexOrSet(const AbstrDataItem* valuesItem, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem, OIV resBegin, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	fast_fill(resBegin, resBegin+pCount, UNDEFINED_OR_ZERO(V));

	if (valuesItem->GetAbstrDomainUnit()->IsCurrTiled())
		WeightedModusPartBySet  <V, OIV>(valuesItem, weightItem, indicesItem, resBegin, pCount);
	else
		WeightedModusPartByIndex<V, OIV>(valuesItem, weightItem, indicesItem, resBegin, pCount);
}
*/

template<typename V, typename OIV>
void WeightedModusPartByTable(
	const AbstrDataItem* valuesItem, 
	const AbstrDataItem* weightItem, 
	const AbstrDataItem* indicesItem, 
	OIV resBegin, 
	typename Unit<V>::range_t valuesRange, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	SizeT vCount = Cardinality(valuesRange);
	std::vector<Float64> buffer(vCount*pCount, 0);
	std::vector<Float64>::iterator bufferB = buffer.begin();

	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = const_array_cast<V>(valuesItem )->GetLockedDataRead(t);
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
				dms_assert(IsIncluding(valuesRange, *valuesIter)); // PRECONDITION
				UInt32 vi = Range_GetIndex_naked(valuesRange, *valuesIter);
				dms_assert(vi < vCount);
				SizeT pi = indexGetter->Get(i);
				if (IsNotUndef(pi))
				{
					dms_assert(pi < pCount);
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

template<typename V, typename OIV>
void WeightedModusPartDispatcher(const AbstrDataItem* valuesItem, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem, OIV resBegin, SizeT nrP
	, const inf_type_tag*)
{
	// NonCountable values; go for Set implementation
	WeightedModusPartBySet<V, OIV>(valuesItem, weightItem, indicesItem, resBegin, nrP);
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
void WeightedModusPartDispatcher(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	const AbstrDataItem* indicesItem,
	OIV resBegin,
	SizeT nrP
,	const int_type_tag*
)
{
	typename Unit<V>::range_t valuesRange = GetRange<V>(valuesItem);

	dms_assert(IsDefined(valuesRange)); //we already made result with p as domainUnit, thus count must be known and managable.
	dms_assert(!valuesRange.empty());   //we already made result with p as domainUnit, thus count must be known and managable.

	// Countable values; go for Table if sensible
	row_id
		n = valuesItem->GetAbstrDomainUnit()->GetCount(),
		v = valuesRange.empty() ? MAX_VALUE(row_id) : Cardinality(valuesRange);

	dms_assert(IsNotUndef(nrP)); //consequence of the checks on indexRange: values Unit of index has been used as domain of the result

	if	(	IsDefined(v)
		&& (!nrP || v <= n / nrP)
		&&	OnlyDefinedCheckRequired(valuesItem)
		) // memory condition v*p<=n, thus TableTime <= 2n.
		WeightedModusPartByTable<V>(
			valuesItem, weightItem,	indicesItem
		,	resBegin
		,	valuesRange
		,	nrP
		);
	else
		WeightedModusPartBySet<V>(
			valuesItem, weightItem,	indicesItem
		,	resBegin
		,	nrP
		);
}

template<typename V, typename OIV>
void WeightedModusPartDispatcher(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	const AbstrDataItem* indicesItem,
	OIV  resBegin, 
	SizeT nrP,
	const bool_type_tag*
)
{
	WeightedModusPartByTable<V>(
		valuesItem, weightItem,	indicesItem
	,	resBegin
	,	GetRange<V>(valuesItem)
	,	nrP
	);
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

		ModusTotDispatcher<V, ResultValueType>(arg1A, resData[0], m_AggrFunc, TYPEID(elem_traits<V>));
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

		WeightedModusTotDispatcher<V>(
			arg1A, arg2A,
			resData[0], 
			TYPEID(elem_traits<V>)
		);
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
			if constexpr (is_integral_v<field_of_t<V>>)
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
				ModusPartBySet<V>(pdi.arg2A, std::move(pdi.values_fta), std::move(pdi.part_fta), resBegin, pdi.resCount, m_AggrFunc);
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
		:	AbstrOperAccPartBin(gr
			,	ResultType::GetStaticClass()
			,	Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), Arg3Type::GetStaticClass()
			,	arg1_values_unit, COMPOSITION(ValueType)
			)
	{}

	// Override Operator
	void Calculate(DataWriteLock& res,
		const AbstrDataItem* arg1A, 
		const AbstrDataItem* arg2A,
		const AbstrDataItem* arg3A
	) const override
	{
		ResultType* result = mutable_array_cast<ValueType>(res);
		dms_assert(result);
		auto resData = result->GetLockedDataWrite();

		dbg_assert(resData.size() == res->GetTiledRangeData()->GetRangeSize()); // DataWriteLock was set by caller and p3 is domain of res

		WeightedModusPartDispatcher<V>(
			arg1A, arg2A, arg3A,
			resData.begin(), 
			arg3A->GetAbstrValuesUnit()->GetCount(),
			TYPEID(elem_traits<V>)
		);
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	CommonOperGroup cogEntropy("entropy");
	CommonOperGroup cogAvgEntropy("average_entropy");

	CommonOperGroup cogModusCount08("modus_count_uint8");
	CommonOperGroup cogModusCount16("modus_count_uint17");
	CommonOperGroup cogModusCount32("modus_count_uint32");
	CommonOperGroup cogModusCount64("modus_count_uint64");

	CommonOperGroup cogUniqueCount08("unique_count_uint8");
	CommonOperGroup cogUniqueCount16("unique_count_uint16");
	CommonOperGroup cogUniqueCount32("unique_count_uint32");
	CommonOperGroup cogUniqueCount64("unique_count_uint64");

	CommonOperGroup cogModus("modus");
	CommonOperGroup cogModusW("modus_weighted");

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
