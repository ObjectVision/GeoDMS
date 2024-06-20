// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3

#pragma once

#if !defined(__SHV_CLASSBREAKS_H)
#define __SHV_CLASSBREAKS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#define MG_DEBUG_CLASSBREAKS false

#include "cpc/types.h"
#include "ClcBase.h"

#include <vector>

//----------------------------------------------------------------------
// typedefs
//----------------------------------------------------------------------

using ClassBreakValueType = Float64;
template<typename CB> using break_array_t = std::vector<CB>;
using break_array = break_array_t<ClassBreakValueType>;

using CountType = SizeT;
template<typename CB> using ValueCountPair = std::pair<CB, CountType>;
template<typename CB> using LimitsContainer = std::vector<CB>;
template<typename CB> using ValueCountPairContainerBase = std::vector < ValueCountPair<CB>, my_allocator < ValueCountPair<CB>> >;

template<typename CB>
struct ValueCountPairContainerT : ValueCountPairContainerBase<CB>
{
	ValueCountPairContainerT() : m_Total(0) {}

	ValueCountPairContainerT(ValueCountPairContainerT&& rhs) noexcept
		: ValueCountPairContainerBase<CB>(std::move(rhs))
		, m_Total(rhs.m_Total)
	{
		assert(rhs.empty());
		rhs.m_Total = 0;
	}
	template<typename Iter>
	ValueCountPairContainerT(Iter first, Iter last)
		: ValueCountPairContainerBase<CB>(first, last)
	{
		for (const auto& vcp : *this)
			m_Total += vcp.second;
	}

	void operator = (ValueCountPairContainerT&& rhs)  noexcept
	{
		ValueCountPairContainerBase<CB>::operator =((ValueCountPairContainerBase<CB>&&)rhs);
		std::swap(m_Total, rhs.m_Total);
	}


	SizeT m_Total = 0;
	MG_DEBUGCODE( void Check(); )

private:
	ValueCountPairContainerT(const ValueCountPairContainerT&) = delete;
	void operator = (const ValueCountPairContainerT&) = delete;
};

using ValueCountPairContainer = ValueCountPairContainerT<ClassBreakValueType>;

using AbstrValuesRangeDataPtrType = SharedPtr<const SharedObj>;
template<typename CB> using CountsResultTypeT = std::pair<ValueCountPairContainerT<CB>, AbstrValuesRangeDataPtrType>;
using CountsResultType = CountsResultTypeT<ClassBreakValueType>;
const UInt32 MAX_PAIR_COUNT = 4096;

template <typename CB> CLC_CALL CountsResultTypeT<CB> GetCounts(const AbstrDataItem* adi, SizeT maxPairCount);
CLC_CALL CountsResultType PrepareCounts(const AbstrDataItem* adi, SizeT maxPairCount);

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
