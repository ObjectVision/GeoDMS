// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <numbers>
#include <cmath>

#include "vt/CheckedCalc.h"
#include "vt/Conversions.h"
#include "vt/IsNotUndef.h"
#include "mci/CompositeCast.h"
// #include "mem/HeapSequenceProvider.ipp"
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

// The modus intermediates (table buffers, counter maps) go through my_vec_t/my_map_t: they are the
// actual working set of this operator, and the default allocator would both bypass the lock-free
// allocation stocks and hide the cost from the per-operation allocation census (SS8.1.16).
#include "mem/MyContainers.h"

#include "dbg/SeverityType.h"
#include "parallel/portable_task_group.h" // task_canceled
#include "utl/Environment.h"
#include "utl/Registry.h" // IsPerformanceLogging



// *****************************************************************************
//											Modus Helper funcs
// *****************************************************************************

// the first of the largest positive counts; they are compared as countF gives them, so that sums of Float64 weights are
// not truncated to an integral counter
template <typename CIter>
CIter arg_max(CIter b, CIter e, auto countF)
{
	decltype(countF(b)) maxC = 0;
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

template <typename Value>
struct modusFunc {
	using result_type = Value;

	template <typename CIter>
	auto operator ()(CIter b, CIter e, auto countF, auto valueF) -> result_type
	{
		CIter p = arg_max(b, e, countF);
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
		CIter p = arg_max(b, e, countF);
		if (p == e)
			return 0;
		return ThrowingConvert<Counter>(countF(p)); // as uniqueCountFunc and count_uintN: an overflow is an error, not a wrapped count
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

struct frequencyTableFunc {
	using result_type = SharedStr;

	template <typename CIter>
	auto operator ()(CIter b, CIter e, auto countF, auto valueF) -> result_type
	{
		VectorOutStreamBuff buff;
		FormattedOutStream out(&buff);
		bool hasAlreadyWrittenSomething = false;
		for (auto i = b; i != e; ++i)
		{
			auto count = countF(i);
			if (!count)
				continue;

			if (hasAlreadyWrittenSomething)
				out << "; ";

			auto value = valueF(i);
			if (!IsDefined(value))
				out << "<null>: " << count;
			else
				out << value << ": " << count;
			hasAlreadyWrittenSomething = true;
		}
		return SharedStr(CharPtrRange(buff.GetData(), buff.GetDataEnd()));
	}
};

struct asUniqueListFunc {
	using result_type = SharedStr;

	template <typename CIter>
	auto operator ()(CIter b, CIter e, auto countF, auto valueF) -> result_type
	{
		VectorOutStreamBuff buff;
		FormattedOutStream out(&buff);
		bool hasAlreadyWrittenSomething = false;
		for (auto i = b; i != e; ++i)
		{
			auto count = countF(i);
			if (!count)
				continue;

			if (hasAlreadyWrittenSomething)
				out << "; ";

			auto value = valueF(i);
			if (!IsDefined(value))
				out << "<null>";
			else
				out << value;
			hasAlreadyWrittenSomething = true;
		}
		return SharedStr(CharPtrRange(buff.GetData(), buff.GetDataEnd()));
	}
};

// *****************************************************************************
//											Formal ranges
// *****************************************************************************

// Integral values are assumed to lie in the formal range of their values unit (see TableSize in ValuesTable.h). Whether a
// table or a set counts them, a counted value outside it throws, so that the result does not depend on which one counts.

template <typename V>
auto GetFormalRange(const DataArray<V>* valuesTF) -> Range<V>
{
	auto vrd = valuesTF->GetValueRangeData();
	if (!vrd)
		return Range<V>(Undefined());
	return vrd->GetRange();
}

// Checks values counted in ascending order, which puts their extremes at the ends; a null, counted when nulls are, comes
// first (see ValueCountKeyCompare).
template <typename V, typename CIter>
void CheckCountedValues(CIter b, CIter e, const Range<V>& range, auto valueF)
{
	if (!IsRestrictingRange(range))
		return;
	if (b != e && !IsDefined(valueF(b)))
		++b;
	if (b == e)
		return;
	if (!IsIncluding(range, valueF(b)))
		ThrowOutOfFormalRange<V>(valueF(b), range);
	--e;
	if (!IsIncluding(range, valueF(e)))
		ThrowOutOfFormalRange<V>(valueF(e), range);
}

// *****************************************************************************
//											ModusTot
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename R, typename AggrFunc>
void ModusTotBySet(const DataArray<V>* tileFunctor, typename sequence_traits<R>::container_type::reference resData, bool valueMustBeDefined, AggrFunc aggrFunc)
{
	auto values_fta = GetFutureTileArray(tileFunctor);
	auto counters = GetWeededWallCounts<V, SizeT>(values_fta, SizeT(-1), valueMustBeDefined);
	if constexpr (is_integral_v<V>)
		CheckCountedValues<V>(counters.begin(), counters.end(), GetFormalRange(tileFunctor), [](auto i) { return i->first; });

	resData = aggrFunc(counters.begin(), counters.end()
	,	[](auto i) { return i->second; }
	,	[](auto i) { return i->first; }
	);
}

template<typename V, typename R, typename AggrFunc>
void ModusTotByTable(const DataArray<V>* tileFunctor, typename sequence_traits<R>::container_type::reference resData,  typename Unit<V>::range_t valuesRange, AggrFunc aggrFunc)
{
	auto buffer = GetCountsAsArray<V, SizeT>(tileFunctor, valuesRange);

	resData = aggrFunc(buffer.begin(), buffer.end()
	, [ ](auto i) { return *i; }
	, [&](auto i) { return TableValue(valuesRange, i - buffer.begin()); }
	);
}

// The modus family counts integral values in a table over their formal range when the table is not larger than the values:
//      Table: O(n+v*p) processing with O(v*p) temp memory
//	and Set:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
// so where v*p <= n, TableTime O(n+v*p) <= O(2n) < O(n*log(min(n,v))). ModusTotal is the case p=1. Bit values always go
// to the table; values that are not countable, such as strings and floats, always to the set.

template <typename V, typename R, typename AggrFunc>
void ModusTotDispatcher(const DataArray<V>* valuesTF, typename sequence_traits<R>::container_type::reference resData, bool valueMustBeDefined, AggrFunc aggrFunc)
{
	if constexpr (is_bitvalue_v<scalar_of_t<V>>)
	{
		// bit values have no null, so the table represents both variants exactly
		ModusTotByTable<V, R>(valuesTF, resData, GetValuesRange<V>(valuesTF), aggrFunc);
	}
	else
	{
		if constexpr (is_integral_v<scalar_of_t<V>>)
		{
			// the table is indexed by values range and thus has no slot to count nulls in,
			// so it can only serve the variant that skips them; compare ModusPart::ProcessData
			if (valueMustBeDefined)
			{
				auto valuesRange = GetFormalRange(valuesTF);
				auto v = TableSize(valuesRange);
				if (IsDefined(v) && v <= valuesTF->GetTiledRangeData()->GetElemCount())
				{
					ModusTotByTable<V, R>(valuesTF, resData, valuesRange, aggrFunc);
					return;
				}
			}
		}
		ModusTotBySet<V, R>(valuesTF, resData, valueMustBeDefined, aggrFunc);
	}
}

// *****************************************************************************
//									ModusPart
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV, typename AggrFunc>
void ModusPartBySet(const AbstrDataItem* indicesItem, abstr_future_tile_array part_fta
	, future_tile_array<V> values_fta
	, OIV resBegin, SizeT pCount, bool valueMustBeDefined, AggrFunc aggrFunc  // countable dommain unit of result; P can be Void.
	, const Range<V>* formalRange) // of integral values
{
	assert(values_fta.size() == part_fta.size());

	using value_type = std::pair<SizeT, V>;
	auto tn = values_fta.size();

	auto counters = GetPartitionedWallCounts<V, SizeT>(values_fta
		, indicesItem, part_fta
		, 0, tn, pCount, valueMustBeDefined);

	auto i = counters.begin(), e = counters.end();
	SizeT ri = 0;
	auto getCount = [](auto counterPtr) { return counterPtr->second; };
	auto getValue = [](auto counterPtr) { return counterPtr->first.second; };
	while (i != e)
	{
		SizeT p = i->first.first;
		auto pb = i;
		while (++i != e)
			if (i->first.first != p)
				break;
		if constexpr (is_integral_v<V>)
			CheckCountedValues<V>(pb, i, *formalRange, getValue);
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
	SizeT vCount = TableSize(valuesRange);
	my_vec_t<SizeT> buffer(vCount*pCount, 0);
	auto bufferB = buffer.begin();

	for (tile_id t =0, tn = values_fta.size(); t != tn; ++t)
	{
		auto values = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		auto valuesIter = values.begin(),
		     valuesEnd  = values.end();

		auto indexGetter = std::unique_ptr<IndexGetter>( IndexGetterCreator::Create(indicesItem, part_afta[t])); part_afta[t] = nullptr;
		SizeT i=0;

		for (; valuesIter != valuesEnd; ++i, ++valuesIter)
		{
			if constexpr (has_undefines_v<V>)
				if (!IsDefined(*valuesIter))
					continue;
			auto pi = indexGetter->Get(i);
			if (pi >= pCount)
				continue;
			auto vi = Range_GetIndex_naked_unchecked(valuesRange, *valuesIter);
			if constexpr (has_undefines_v<V>)
				if (vi >= vCount)
					ThrowOutOfFormalRange<V>(*valuesIter, valuesRange);
			SafeIncrementCounter(bufferB[ pi * vCount + vi]);
		}
	}

	for (OIV resEnd = resBegin + pCount; resBegin != resEnd; ++resBegin)
	{
		*resBegin = aggrFunc(bufferB, bufferB + vCount
		, [ ](auto i) { return *i; }
		, [&](auto i) { return TableValue(valuesRange, i - bufferB); }
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
void WeightedModusTotBySet(const DataArray<V>* valuesTF, const AbstrDataItem* weightItem, typename sequence_traits<V>::container_type::reference resData)
{
	my_map_t<V, Float64> counters;

	for (tile_id t =0, tn = valuesTF->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = valuesTF->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(), valuesEnd   = valuesLock.end();
		auto weightsGetter = std::unique_ptr<AbstrValueGetter<Float64>>( WeightGetterCreator::Create(weightItem, t) );

		SizeT weightsIter = 0;
		for (; valuesIter != valuesEnd; ++weightsIter, ++valuesIter)
			if (IsDefined(*valuesIter))
			{
				Float64 weight = weightsGetter->Get(weightsIter);
				if (IsDefined(weight)) // as the partitioned twins: a null weight would make the value's total NaN, which then wins arg_max
					counters[*valuesIter] += weight;
			}
	}
	CheckCountedValues<V>(counters.begin(), counters.end(), GetFormalRange(valuesTF), [](auto i) { return i->first; });

	modusFunc<V> aggrFunc;

	resData = aggrFunc(counters.begin(), counters.end()
	, [](auto i) { return i->second; }
	, [](auto i) { return i->first; }
	);

}

template<typename V>
void WeightedModusTotByTable(const DataArray<V>* valuesTF, const AbstrDataItem* weightItem, typename sequence_traits<V>::container_type::reference resData, const typename Unit<V>::range_t& valuesRange)
{
	auto vCount = TableSize(valuesRange);
	my_vec_t<Float64> buffer(vCount, 0);

	for (tile_id t =0, tn = valuesTF->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = valuesTF->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();
		auto weightsGetter = std::unique_ptr<AbstrValueGetter<Float64>>( WeightGetterCreator(weightItem, t).Create() );

		SizeT weightIter  = 0;

		for (; valuesIter != valuesEnd; ++weightIter, ++valuesIter)
		{
			if constexpr (has_undefines_v<V>)
				if (!IsDefined(*valuesIter))
					continue;
			auto v = Range_GetIndex_naked_unchecked(valuesRange, *valuesIter);
			if constexpr (has_undefines_v<V>)
				if (v >= vCount)
					ThrowOutOfFormalRange<V>(*valuesIter, valuesRange);
			Float64 weight = weightsGetter->Get(weightIter);
			if (IsDefined(weight)) // see WeightedModusTotBySet
				buffer[v] += weight;
		}
	}

	modusFunc<V> aggrFunc;

	resData = aggrFunc(buffer.begin(), buffer.end()
	, [ ](auto i) { return *i; }
	, [&](auto i) { return TableValue(valuesRange, i - buffer.begin()); }
	);
}

template<typename V>
void WeightedModusTotDispatcher(const DataArray<V>* valuesTF, const AbstrDataItem* weightItem, typename sequence_traits<V>::container_type::reference resData)
{
	if constexpr (is_bitvalue_v<scalar_of_t<V>>)
	{
		WeightedModusTotByTable<V>(valuesTF, weightItem, resData, GetValuesRange<V>(valuesTF));
	}
	else
	{
		auto valuesRange = GetFormalRange(valuesTF);
		auto v = TableSize(valuesRange);
		if (IsDefined(v) && v <= valuesTF->GetTiledRangeData()->GetElemCount())
			WeightedModusTotByTable<V>(valuesTF, weightItem, resData, valuesRange);
		else
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
	my_map_t<value_type, Float64> wieghtAccumulators;

	for (tile_id t=0, tn= valuesTF->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = valuesTF->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
			 valuesEnd   = valuesLock.end();

		auto indexGetter = std::unique_ptr<IndexGetter>( IndexGetterCreator::Create(indicesItem, t) );
		auto weightsGetter = std::unique_ptr<AbstrValueGetter<Float64>>( WeightGetterCreator::Create(weightItem, t) );

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
	modusFunc<V> aggrFunc;
	auto getCount = [](auto counterPtr) { return counterPtr->second; };
	auto getValue = [](auto counterPtr) { return counterPtr->first.second; };
	auto formalRange = GetFormalRange(valuesTF);

	auto i = wieghtAccumulators.begin(), e = wieghtAccumulators.end();
	SizeT ri = 0;
	while (i != e)
	{
		SizeT p = i->first.first;
		auto pb = i;
		while (++i != e)
			if (i->first.first != p)
				break;
		CheckCountedValues<V>(pb, i, formalRange, getValue);
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
	SizeT vCount = TableSize(valuesRange);
	my_vec_t<Float64> buffer(vCount*pCount, 0);
	my_vec_t<Float64>::iterator bufferB = buffer.begin();

	for (tile_id t =0, tn = valuesTF->GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = valuesTF->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();

		auto indexGetter = std::unique_ptr<IndexGetter>( IndexGetterCreator::Create(indicesItem, t) );
		auto weightsGetter = std::unique_ptr<AbstrValueGetter<Float64>>( WeightGetterCreator::Create(weightItem, t) );

		SizeT weightIter = 0;
		Float64 weight;
		SizeT i=0;
		for (; valuesIter != valuesEnd; ++weightIter, ++i, ++valuesIter)
			if (IsDefined(*valuesIter) && IsDefined(weight = weightsGetter->Get(weightIter)))
			{
				auto pi = indexGetter->Get(i);
				if (pi >= pCount)
					continue;
				auto vi = Range_GetIndex_naked_unchecked(valuesRange, *valuesIter);
				if constexpr (has_undefines_v<V>)
					if (vi >= vCount)
						ThrowOutOfFormalRange<V>(*valuesIter, valuesRange);
				bufferB[pi * vCount + vi] += weight;
			}
	}

	modusFunc<V> aggrFunc;

	for (OIV resEnd = resBegin + pCount; resBegin != resEnd; ++resBegin)
	{
		*resBegin = aggrFunc(bufferB, bufferB + vCount
		, [ ](auto i) { return *i; }
		, [&](auto i) { return TableValue(valuesRange, i - bufferB); }
		);

		bufferB += vCount;
	}
	assert(bufferB == buffer.end());
}

template<typename V, typename OIV>
void WeightedModusPartDispatcher(const DataArray<V>* valuesTF, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem, OIV resBegin, SizeT nrP)
{
	if constexpr (is_bitvalue_v<scalar_of_t<V>>)
	{
		WeightedModusPartByTable<V>(valuesTF, weightItem, indicesItem, resBegin, GetValuesRange<V>(valuesTF), nrP);
	}
	else
	{
		assert(IsNotUndef(nrP)); //consequence of the checks on indexRange: values Unit of index has been used as domain of the result

		auto valuesRange = GetFormalRange(valuesTF);
		auto v = TableSize(valuesRange);
		auto n = valuesTF->GetTiledRangeData()->GetElemCount();
		if (IsDefined(v) && (!nrP || v <= n / nrP)) // memory condition v*p<=n, thus TableTime <= 2n.
			WeightedModusPartByTable<V>(valuesTF, weightItem, indicesItem, resBegin, valuesRange, nrP);
		else
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
	ModusTotal(AbstrOperGroup* gr, UnitCreatorPtr ucp, bool valueMustBeDefined)
		:	AbstrOperAccTotUni(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), ucp, COMPOSITION(ResultType), valueMustBeDefined)
	{}

	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, ArgRefs args, std::vector<ItemReadLock> readLocks) const override
	{
		ResultType* result = mutable_array_cast<ResultValueType>(res);
		assert(result);
		auto  resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		ModusTotDispatcher<V, ResultValueType>(const_array_cast<V>(arg1A), resData[0], m_ValueMustBeDefined, m_AggrFunc);
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
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);

		WeightedModusTotDispatcher<V>(const_array_cast<V>(arg1A), arg2A, resData[0]);
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

	ModusPart(AbstrOperGroup* gr, UnitCreatorPtr ucp, bool valueMustBeDefined)
		: base_type(gr, ucp, valueMustBeDefined)
	{}

	// The table path's O(v*p) counter buffer IS this operator's footprint at scale: t641's
	// Write_*_25m_LU_ModelType holds a single 77.2 G table against an 18.3 G prediction, twice --
	// the run's largest allocations by far, invisible to the admission gate (SS8.1.19). Mirror the
	// dispatcher's own tradeoff below (integral countable values and v*p <= n selects the table):
	// when the table will be chosen, charge it as working memory. The set path keeps the family
	// default. One buffer, not one per thread: ModusPartByTable walks the tiles serially.
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = base_type::EstimatePerformance(resultHolder, args);
		if constexpr (is_integral_v<scalar_of_t<V>>)
			try {
				if (!args.empty())
					if (auto valuesItem = GetItem(args[0]); valuesItem && IsDataItem(valuesItem))
					{
						auto valuesAdi = AsDataItem(valuesItem);
						auto vCount = valuesAdi->GetAbstrValuesUnit()->EstimateCount();
						SizeT v = vCount.expected;
						SizeT n = valuesAdi->GetAbstrDomainUnit()->EstimateCount().expected;
						SizeT p = result.resultingNrElements;
						// The probe run (SS8.1.21) showed the dispatcher's n as ASSUMED_SIZE here for
						// the t641 whale: the values-arg's cache domain was not count-ready even at
						// the run-time estimate, so `v <= n/p` compared against garbage and the term
						// never fired. A resolved n is at least p (one visited row per partition
						// cell); when it is smaller, treat n as unknown and fall back to "any
						// classification-sized v selects the table" -- if the dispatcher then picks
						// the set path after all, actual memory is LESS than charged, which is the
						// safe side for admission.
						const SizeT MAX_CLASSIFICATION_COUNT = 4096;
						bool credibleN = n >= p;
						bool fires = v && p && (credibleN ? v <= n / p : v <= MAX_CLASSIFICATION_COUNT);
						if (fires)
							MakeMax(result.workingMemorySize, v * p * sizeof(SizeT));
						// Residual probe: measured peak implies the dispatcher sized the table for
						// ~64 values where the unit's expected count says 19 -- a declared count vs
						// data-range gap. Log the bound alongside until one run settles it.
						if (IsPerformanceLogging())
							reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
								, "modus table-term probe: v={} vUB={} vConf={} n={} p={} fires={} ws={}"
								, v, vCount.upperBound, AsString(vCount.confidence), n, p, fires, result.workingMemorySize);
					}
			}
			catch (const task_canceled&) { throw; } // a teardown in progress is not an estimation failure
			catch (...) {} // an unresolved unit just keeps the family default
		return result;
	}

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
			// the table or the set, as described at ModusTotDispatcher
			assert(IsNotUndef(pdi.resCount)); //consequence of the checks on indexRange

			if constexpr (is_integral_v<scalar_of_t<V>>)
			{
				auto range = pdi.valuesRangeData ? pdi.valuesRangeData->GetRange() : Range<V>(Undefined());
				SizeT v = TableSize(range);

				// the table has no slot to count nulls in, so it can only serve the variant that skips them
				if (IsDefined(v) && this->m_ValueMustBeDefined
					&& (!pdi.resCount || v <= pdi.n / pdi.resCount)
					) // memory condition v*p<=n, thus TableTime <= 2n.
				{
					ModusPartByTable<V>(pdi.arg2A, std::move(pdi.values_fta), std::move(pdi.part_fta), resBegin, range, pdi.resCount, m_AggrFunc);
					return;
				}
				ModusPartBySet<V>(pdi.arg2A, std::move(pdi.part_fta), std::move(pdi.values_fta), resBegin, pdi.resCount, this->m_ValueMustBeDefined, m_AggrFunc, &range);
			}
			else
				ModusPartBySet<V>(pdi.arg2A, std::move(pdi.part_fta), std::move(pdi.values_fta), resBegin, pdi.resCount, this->m_ValueMustBeDefined, m_AggrFunc, nullptr);
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

	// Same table term as ModusPart: WeightedModusPartDispatcher picks the O(v*p) Float64 table under
	// the same v*p <= n condition, and that buffer dominates at scale (SS8.1.19).
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = AbstrOperAccPartBin::EstimatePerformance(resultHolder, args);
		if constexpr (is_integral_v<scalar_of_t<V>>)
			try {
				if (!args.empty())
					if (auto valuesItem = GetItem(args[0]); valuesItem && IsDataItem(valuesItem))
					{
						auto valuesAdi = AsDataItem(valuesItem);
						SizeT v = valuesAdi->GetAbstrValuesUnit()->EstimateCount().expected;
						SizeT n = valuesAdi->GetAbstrDomainUnit()->EstimateCount().expected;
						SizeT p = result.resultingNrElements;
						if (v && p && v <= n / p) // the dispatcher's memory condition: v*p <= n
							MakeMax(result.workingMemorySize, v * p * sizeof(Float64));
					}
			}
			catch (const task_canceled&) { throw; } // a teardown in progress is not an estimation failure
			catch (...) {} // an unresolved unit just keeps the family default
		return result;
	}

	// Override Operator
	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A) const override
	{
		auto result = mutable_array_cast<ValueType>(res); assert(result);
		// write_only_all, like ModusPart above: WeightedModusPartByTable/BySet assign every element of
		// [0, nrP) -- gaps and tail included -- so no zero-fill is needed. Asking for mustzero here was
		// unsatisfiable anyway: AbstrOperAccPartBin opens the lock write_only_all (deliberately -- the
		// partial-aggregation family initialises via TAcc1Func::Init, not via the allocator), and an
		// untiled result allocates in its ctor, so the request was silently dropped.
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		assert(resData.size() == res->GetTiledRangeData()->GetRangeSize()); // DataWriteLock was set by caller and p3 is domain of res

		WeightedModusPartDispatcher<V>(const_array_cast<V>(arg1A), arg2A, arg3A, resData.begin(), arg3A->GetAbstrValuesUnit()->GetCount());
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
	CommonOperGroup cogModusCount16("modus_count_uint16", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogModusCount32("modus_count_uint32", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogModusCount64("modus_count_uint64", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogUniqueCount08("unique_count_uint8", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogUniqueCount16("unique_count_uint16", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogUniqueCount32("unique_count_uint32", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogUniqueCount64("unique_count_uint64", oper_policy::better_not_in_meta_scripting);

	CommonOperGroup cogModus("modus", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogModusW("modus_weighted", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogFequencyTable("frequency_table", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogFequencyTableWithNull("frequency_table_with_null", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogAsUniqueList("as_unique_list", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogAsUniqueListWithNull("as_unique_list_with_null", oper_policy::better_not_in_meta_scripting);

	template <typename V, typename AggrFunc>
	struct AggrFuncInst
	{
		AggrFuncInst(AbstrOperGroup& aog, UnitCreatorPtr ucp, bool valueMustBeDefined)
			: mt(&aog, ucp, valueMustBeDefined)
			, mp(&aog, ucp, valueMustBeDefined)
		{}

	private:
		ModusTotal<V, AggrFunc> mt;
		ModusPart <V, AggrFunc> mp;
	};

	template <typename V>
	struct AggrFuncsInst
	{
		AggrFuncsInst()
			: m_ModusFunc(cogModus, arg1_values_unit, true)
			, m_ModusCountFunc08(cogModusCount08, default_unit_creator<UInt8>, true)
			, m_ModusCountFunc16(cogModusCount16, default_unit_creator<UInt16>, true)
			, m_ModusCountFunc32(cogModusCount32, default_unit_creator<UInt32>, true)
			, m_ModusCountFunc64(cogModusCount64, default_unit_creator<UInt64>, true)
			, m_UniqueCountFunc08(cogUniqueCount08, default_unit_creator<UInt8>, true)
			, m_UniqueCountFunc16(cogUniqueCount16, default_unit_creator<UInt16>, true)
			, m_UniqueCountFunc32(cogUniqueCount32, default_unit_creator<UInt32>, true)
			, m_UniqueCountFunc64(cogUniqueCount64, default_unit_creator<UInt64>, true)
			, m_EntropyFunc(cogEntropy, default_unit_creator<Float64>, true)
			, m_AvgEntropyFunc(cogAvgEntropy, default_unit_creator<Float64>, true)
			, m_FreqTable(cogFequencyTable, default_unit_creator<SharedStr>, true)
			, m_FreqTableWithNull(cogFequencyTableWithNull, default_unit_creator<SharedStr>, false)
			, m_AsUniqueList(cogAsUniqueList, default_unit_creator<SharedStr>, true)
			, m_AsUniqueListWithNull(cogAsUniqueListWithNull, default_unit_creator<SharedStr>, false)
		{}

	private:
		AggrFuncInst<V, modusFunc<V> > m_ModusFunc;

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
		AggrFuncInst<V, frequencyTableFunc > m_FreqTable;
		AggrFuncInst<V, frequencyTableFunc > m_FreqTableWithNull;
		AggrFuncInst<V, asUniqueListFunc > m_AsUniqueList;
		AggrFuncInst<V, asUniqueListFunc > m_AsUniqueListWithNull;
	};

	// TODO: generalise WeightedModusXXXX with a variable AggrFunc too, as Modus
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

	tl_oper::inst_tuple_templ<typelists::scalars, AggrFuncsInst> aggrOpers;
	tl_oper::inst_tuple_templ<typelists::aints, WeightedModusInst> weigthedModusOpers;

//	ModusInst<SharedStr> mpString; // TODO; also TODO: optimize the dispatchers for (U)Int4/2
}
