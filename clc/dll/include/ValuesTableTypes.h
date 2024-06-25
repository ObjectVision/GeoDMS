// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_VALUESTABLETYPES_H)
#define __CLC_VALUESTABLETYPES_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ClcBase.h"

#include "geo/Pair.h"

#include "AggrFuncNum.h"

//#include "set/CompareFirst.h"
//#include "ASync.h"

//#include "AbstrDataItem.h"
//#include "DataArray.h"
//#include "UnitProcessor.h"

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
	SafeAccumulate(a, b);
//	a += b;
};

using CountType = SizeT;

template <typename V> using my_vector = std::vector<V, my_allocator<V>>;

template<ordered_value_type V, count_type C = CountType> using ValueCountPair = std::pair<V, C>;
template<ordered_value_type V, count_type C = CountType> using ValueCountPairContainerT = my_vector< ValueCountPair<V, C> >;
template<ordered_value_type V, count_type C = CountType> using PartionedValueCountPairContainerT = ValueCountPairContainerT<Pair<SizeT, V>, C>;

// template<count_type C> using CountArray = my_vector< C >;
// template<ordered_value_type V, count_type C = CountType> using FreqTable = std::variant<ValueCountPairContainerT<V, C>, CountArray<C>>;

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


#endif // !defined(__CLC_VALUESTABLETYPES_H)
