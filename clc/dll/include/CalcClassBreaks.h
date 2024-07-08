// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_CLASSBREAKS_H)
#define __CLC_CLASSBREAKS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ClcBase.h"

#include "ValuesTableTypes.h"

#define MG_DEBUG_CLASSBREAKS false

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

#endif // !defined(__CLC_CLASSBREAKS_H)
