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

// Default lookup: writes valuesArray[partitioner.GetIndex(p)] for valid indices,
// `Undefined()` (which Assign deduces to the iterator's value_type's NULL) for
// missing ones. Correct as long as the iterator's value_type's NULL bit-pattern
// is the one downstream code checks via IsNull — i.e. as long as the caller has
// NOT applied a reinterpret_cast that changes that bit-pattern.
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

// Variant for callers that have applied a reinterpret_cast to merge V into a
// same-size unsigned proxy (see `same_size_type` in clc/dll/src/lookup.cpp).
// `undefVal` is the *original* V's UNDEFINED_VALUE bit-cast into the proxy
// type, so missing-index slots get V's correct NULL bit-pattern (which differs
// from the proxy type's own NULL, e.g. IPoint NULL = (INT32_MIN, INT32_MIN) =
// 0x8000_0000_8000_0000 vs UInt64 NULL = 0xFF…F).
template<typename Partitioner, typename OIV, typename CIP, typename CIV, typename UndefT>
void lookup_impl_with_undef(OIV outFirst, OIV outLast
	,	CIP indicesFirst, CIV valuesArray
	,	Partitioner partitioner
	,	const UndefT& undefVal
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
			Assign(*outFirst, undefVal);
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

// Variant of lookup_best for callers that have applied a reinterpret_cast to
// merge V into a same-size unsigned proxy. See `lookup_impl_with_undef` for the
// explanation of why the explicit `undefVal` is needed.
template <typename P, typename OIV, typename CIP, typename CIV, typename UndefT>
void lookup_best_with_undef(OIV outFirst, OIV outLast
	,	CIP indicesFirst, CIV valuesArray
	,	const Range<P>&  indexValueRange
	,	DataCheckMode dcm
	,	const UndefT& undefVal
)
{
	dms_assert(dcm != DCM_CheckBoth);

	bool hasOutOfRangeIndices = dcm & DCM_CheckRange;
	if (!(dcm & DCM_CheckDefined))
		if (!hasOutOfRangeIndices)
			lookup_impl_with_undef<typename partition_types<P>::naked_checker_t>(outFirst, outLast, indicesFirst, valuesArray, indexValueRange, undefVal);  // else of impl never used, so use tile version
		else
			lookup_impl_with_undef<typename partition_types<P>::range_checker_t>(outFirst, outLast, indicesFirst, valuesArray, indexValueRange, undefVal);
	else
	{
		dms_assert(!hasOutOfRangeIndices); // discard both for such operations: if <null> can be looked up, then just do it
		lookup_impl_with_undef<typename partition_types<P>::null_checker_t >(outFirst, outLast, indicesFirst, valuesArray, indexValueRange, undefVal);
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

// _with_undef variant for bit_iterator indices: dcm == DCM_None here, so the
// missing-index path is never hit and `undefVal` is unused. Just delegate.
template <typename P, typename OIV, bit_size_t N, typename B, typename CIV, typename UndefT>
void lookup_best_with_undef(OIV outFirst, OIV outLast
	,	bit_iterator<N, B> indicesFirst, CIV valuesArray
	,	const Range<P>&  indexValueRange
	,	DataCheckMode dcm
	,	const UndefT& /*undefVal*/
)
{
	lookup_best(outFirst, outLast, indicesFirst, valuesArray, indexValueRange, dcm);
}


#endif // !defined(__CLC_LOOKUP_H)
