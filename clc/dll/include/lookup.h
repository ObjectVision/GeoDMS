// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_LOOKUP_H)
#define __CLC_LOOKUP_H

#include "geo/BaseBounds.h"
#include "geo/Range.h"
#include "DataCheckMode.h"

#include "PartitionTypes.h"

// *****************************************************************************
extern CLC_CALL CommonOperGroup cog_lookup;

// *****************************************************************************
//                      Lookup operations
//
// use: 
//		lookup        (resBeginPtr, resEndPtr, constIndexPtr, constValuePtr, IndexRange);
//		lookup_ranged (resBeginPtr, resEndPtr, constIndexPtr, constValuePtr, IndexRange); // only assigns when index in range
//		lookup_checked(resBeginPtr, resEndPtr, constIndexPtr, constValuePtr, IndexRange); // as ranged + Undefined check
//
// specific variants differentiate between 1 and 2 dimensional (row, col) indices
// and choose more efficient inner loops when found appropiate
// *****************************************************************************

//                      varians for 1 dimensional countables


// ***************************************************************************** Impl

template<typename Partitioner, typename OIV, typename CIP, typename CIV>
void lookup_impl(OIV outFirst, OIV outLast
	,	CIP indicesFirst, CIV valuesArray
	,	Partitioner partitioner
	)
{
	if (partitioner.GetIndexBase() && !partitioner.m_Domain.empty())
		valuesArray -= partitioner.GetIndexBase();
	for (; outFirst != outLast; ++outFirst, ++indicesFirst)
	{
		typename Partitioner::Range::value_type p = *indicesFirst;
		if (partitioner.Check(p))
			Assign(
				*outFirst, 
				valuesArray[ partitioner.GetIndex(p) ]
			);
		else
			Assign(*outFirst, Undefined() );
	}
}

// ***************************************************************************** Dispatching of different partition types

// Specialization for partition indices with possible <null> or out-of-range
template <typename P, typename OIV, typename CIP, typename CIV>
void lookup_best(OIV outFirst, OIV outLast
	,	CIP indicesFirst, CIV valuesArray
	,	const Range<P>&  indexValueRange
	,	DataCheckMode dcm)
{
	dms_assert(dcm != DCM_CheckBoth);

	bool hasOutOfRangeIndices = dcm & DCM_CheckRange;
	if (!(dcm & DCM_CheckDefined))
		if (!hasOutOfRangeIndices)
			lookup_impl<typename partition_types<P>::naked_checker_t>(outFirst, outLast, indicesFirst, valuesArray, indexValueRange);  // else of impl never used, so use tile version
		else
			lookup_impl<typename partition_types<P>::range_checker_t>(outFirst, outLast, indicesFirst, valuesArray, indexValueRange);
	else
	{
		dms_assert(!hasOutOfRangeIndices); // discard both for such operations: if <null> can be looked up, then just do it
		lookup_impl<typename partition_types<P>::null_checker_t >(outFirst, outLast, indicesFirst, valuesArray, indexValueRange);
	}
}

// Specialization for partition indices without possible <null> or out-of-range
template <typename P, typename OIV, bit_size_t N, typename B, typename CIV>
void lookup_best(OIV outFirst, OIV outLast
	,	bit_iterator<N, B> indicesFirst, CIV valuesArray 
	,	const Range<P>&  indexValueRange
	,	DataCheckMode dcm
)
{
	dms_assert(dcm == DCM_None);

	for (; outFirst != outLast; ++outFirst, ++indicesFirst)
		Assign(
			*outFirst, 
			valuesArray[ bit_value<N>(*indicesFirst) ]
		);
}


#endif // !defined(__CLC_LOOKUP_H)
