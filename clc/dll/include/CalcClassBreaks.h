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
using break_array = std::vector<ClassBreakValueType>;

using CountType = SizeT;
using ValueCountPair = std::pair<ClassBreakValueType, CountType>;
using LimitsContainer = std::vector<ClassBreakValueType>;
using ValueCountPairContainerBase = std::vector < ValueCountPair, my_allocator < ValueCountPair> >;

struct ValueCountPairContainer : ValueCountPairContainerBase
{
	ValueCountPairContainer() : m_Total(0) {}

	ValueCountPairContainer(ValueCountPairContainer&& rhs) noexcept
		: ValueCountPairContainerBase(std::move(rhs))
		, m_Total(rhs.m_Total)
	{
		dms_assert(rhs.empty());
		rhs.m_Total = 0;
	}
	template<typename Iter>
	ValueCountPairContainer(Iter first, Iter last)
		: ValueCountPairContainerBase(first, last)
	{
		for (const auto& vcp : *this)
			m_Total += vcp.second;
	}

	void operator = (ValueCountPairContainer&& rhs)  noexcept
	{
		ValueCountPairContainerBase::operator =((ValueCountPairContainerBase&&)rhs);
		std::swap(m_Total, rhs.m_Total);
	}


	SizeT m_Total = 0;
	MG_DEBUGCODE( void Check(); )

private:
	ValueCountPairContainer(const ValueCountPairContainer&) = delete;
	void operator = (const ValueCountPairContainer&) = delete;
};

using AbstrValuesRangeDataPtrType = SharedPtr<const SharedObj>;
using CountsResultType = std::pair<ValueCountPairContainer, AbstrValuesRangeDataPtrType>;

const UInt32 MAX_PAIR_COUNT = 4096;

CLC_CALL CountsResultType PrepareCounts(const AbstrDataItem* adi, SizeT maxPairCount);
CLC_CALL CountsResultType GetCounts    (const AbstrDataItem* adi, SizeT maxPairCount);

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
