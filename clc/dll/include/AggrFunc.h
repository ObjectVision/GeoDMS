// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__CLC_AGGRFUNC_H)
#define __CLC_AGGRFUNC_H

#include "Prototypes.h"
#include "PartitionTypes.h"
#include "IndexGetterCreator.h"

// *****************************************************************************
//                      aggregate_total function
// *****************************************************************************

template <typename TAssignUniFunc, typename CIV>
void aggr1_total(typename TAssignUniFunc::assignee_ref output, CIV valuesFirst, CIV valuesLast, TAssignUniFunc assignFunc = TAssignUniFunc())
{ 
	for (; valuesFirst != valuesLast; ++valuesFirst)
		assignFunc(output, *valuesFirst);
}

template <typename TAssignBinFunc, typename CIV> 
void aggr2_total(typename TAssignBinFunc::assignee_ref output, CIV values1First, CIV values1Last, CIV values2First, const TAssignBinFunc& assignFunc = TAssignBinFunc())
{ 
	for (; values1First != values1Last; ++values2First, ++values1First)
		assignFunc(output, *values1First, *values2First);
}

// ***************************************************************************** Dispatching of different partition types

// Specialization for partition indices without possible <null> or out-of-range
template<typename TAssignFunc, typename CIV, typename OIA>
void aggr_fw_partial(OIA outFirst, CIV valuesFirst, CIV valuesLast, const IndexGetter* indices, TAssignFunc assignFunc = TAssignFunc() )
{
	SizeT i=0;
	for (; valuesFirst != valuesLast; ++i, ++valuesFirst)
	{
		SizeT index = indices->Get(i);
		if (IsDefined(index))
			assignFunc(outFirst[index], *valuesFirst);
	}
}

/* ============================ Binary accumulators ============== */

// *****************************************************************************
//                      aggregate_partial functions

// ***************************************************************************** Dispatching of different partition types

// Specialization for partition indices with possible <null> or out-of-range
template<typename TAssignFunc, typename CIV, typename OIA>
void aggr2_fw_partial(OIA outFirst
	,	CIV values1First, CIV values1Last,	CIV values2First
	,	const IndexGetter* indices
	,	const TAssignFunc& assignFunc = TAssignFunc()
	)
{
	SizeT i=0;
	for (; values1First != values1Last; ++i, ++values2First, ++values1First)
	{
		SizeT index = indices->Get(i);
		if (IsDefined(index))
			assignFunc(outFirst[index], *values1First, *values2First);
	}
}

#endif // !defined(__CLC_AGGRFUNC_H)
