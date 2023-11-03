// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if !defined(__CLC_INVERT_H)
#define __CLC_INVERT_H

#include "geo/CheckedCalc.h"

#include "DataCheckMode.h"
#include "PartitionTypes.h"

// ***************************************************************************** Impl

template<typename Partitioner, typename E, typename ResIter, typename DataIter>
void invert2values_impl(
		ResIter  resultPtr
	,	DataIter curr
	,	DataIter last
	,	Range<E> entityRange
	,	Partitioner partitioner
)
{
	resultPtr -= partitioner.GetIndexBase();
	UInt32 i = 0;
	for (;curr != last; ++curr, ++i)
	{
		auto v = *curr;
		if (partitioner.Check(v))
			resultPtr[partitioner.GetIndex(v)] = Range_GetValue_naked(entityRange, i);
	}
}

// *****************************************************************************
//									invert2values
// *****************************************************************************

template<typename E, typename V, typename ResIter, typename DataIter>
typename std::enable_if<is_bitvalue<typename std::iterator_traits<DataIter>::value_type>::value == 0>::type
invert2values_best(ResIter  resultPtr
	,	DataIter curr, DataIter last
	,	Range<V> domainRange
	,	typename Unit<E>::range_t entityRange
	,	DataCheckMode dcmIndices
)
{
	dms_assert((dcmIndices & DCM_CheckDefined) || !ContainsUndefined(domainRange));

	bool hasOutOfRangeIndices = dcmIndices & DCM_CheckRange;
	if (!(dcmIndices & DCM_CheckDefined))
		if (!hasOutOfRangeIndices)
			invert2values_impl<typename partition_types<V>::naked_checker_t>(resultPtr, curr, last, entityRange, domainRange);
		else
			invert2values_impl<typename partition_types<V>::range_checker_t>(resultPtr, curr, last, entityRange, domainRange);
	else
	{
		dms_assert(!hasOutOfRangeIndices); // discard both for such operations: if <null> can be inverted, then just do it
		invert2values_impl<typename partition_types<V>::null_checker_t>(resultPtr, curr, last, entityRange, domainRange);
	}
}

template<typename E, typename ResIter, typename bit_size_t N, typename B>
void invert2values_best(ResIter resultPtr
	,	bit_iterator<N, B> curr, bit_iterator<N, B> last
	,	Range<UInt32>             domainRange
	,	typename Unit<E>::range_t entityRange
	,	DataCheckMode dcmIndices
)
{
	dms_assert(dcmIndices == DCM_None);
	dms_assert(domainRange.first == 0 && domainRange.second == bit_value<N>::nr_values);

	for (SizeT i = 0; curr != last; ++curr, ++i)
		resultPtr[Range_GetIndex_checked(domainRange, *curr)] = Range_GetValue_checked(entityRange, i);
}

// ***************************************************************************** Impl

template<typename Partitioner, typename E, typename ResIter, typename DataIter>
void invertAll2values_impl(ResIter  resultPerV
	,	ResIter  resultPerE
	,	DataIter curr, DataIter last
	,	Range<E> entityRange
	,	Partitioner partitioner
)
{
	resultPerV -= partitioner.GetIndexBase();
	UInt32 i = 0;
	for (;curr != last; ++curr, ++i, ++resultPerE)
	{
		auto v = *curr;
		if (partitioner.Check(v))
		{
			ResIter resPtr = resultPerV + partitioner.GetIndex(v);
			*resultPerE = *resPtr;
			*resPtr = Range_GetValue_naked(entityRange, i);
		}
		else
			MakeUndefinedOrZero(*resultPerE);
	}
}

template<typename Partitioner, typename E, typename ResIter, typename DataIter>
void invertAll2values_tile(ResIter  resultPerV
	,	ResIter  resultPerE
	,	DataIter curr, DataIter last
	,	Range<E> entityRange
	,	Partitioner partitioner
)
{
	resultPerV -= partitioner.GetIndexBase();
	SizeT i = 0;
	for (;curr != last; ++curr, ++i, ++resultPerE)
	{
		typename Partitioner::Range::value_type v = *curr;
		if (partitioner.Check(v))
		{
			ResIter resPtr = resultPerV + partitioner.GetIndex(v);
			dms_assert(!IsDefined(*resultPerE)); // Each E-V mapping is only processed once and resultPerE has been made undefined before
			*resultPerE = *resPtr;
			*resPtr = Range_GetValue_naked(entityRange, i);
		}
	}
}

// *****************************************************************************
//									invertAll2values
// *****************************************************************************

template<typename E, typename V, typename ResIter, typename DataIter>
typename std::enable_if<has_undefines_v<typename std::iterator_traits<DataIter>::value_type> >::type
invertAll2values_best(ResIter  resultPerV, ResIter resultPerE
	,	DataIter curr, DataIter last
	,	Range<V> domainRange
	,	typename Unit<E>::range_t entityRange
	,	DataCheckMode dcmIndices, bool mustInitPerEntity
)
{
	dms_assert((dcmIndices & DCM_CheckDefined) || !ContainsUndefined(domainRange));

	bool hasOutOfRangeIndices = dcmIndices & DCM_CheckRange;
	SizeT i = 0;
	if (!(dcmIndices & DCM_CheckDefined))
		if (!hasOutOfRangeIndices)
		{
			dms_assert(mustInitPerEntity);
			invertAll2values_tile<typename partition_types<V>::naked_checker_t>(resultPerV, resultPerE, curr, last, entityRange, domainRange); // else of impl never used, so use tile version
		}
		else
			if (mustInitPerEntity)
				invertAll2values_impl<typename partition_types<V>::range_checker_t>(resultPerV, resultPerE, curr, last, entityRange, domainRange);
			else
				invertAll2values_tile<typename partition_types<V>::range_checker_t>(resultPerV, resultPerE, curr, last, entityRange, domainRange);
	else
	{
		dms_assert(!hasOutOfRangeIndices); // discard both for such operations: if <null> can be inverted, then just do it
		dms_assert(mustInitPerEntity); // tiled version would check value to tile ranges 
		invertAll2values_impl<typename partition_types<V>::null_checker_t>(resultPerV, resultPerE, curr, last, entityRange, domainRange);
	}
}

template<typename E, typename ResIter, typename bit_size_t N, typename B>
void invertAll2values_best(ResIter resultPerV, ResIter resultPerE
	,	bit_iterator<N, B> curr, bit_iterator<N, B> last
	,	Range<UInt32> domainRange
	,	typename Unit<E>::range_t entityRange
	,	DataCheckMode dcmIndices, bool mustInitPerEntity
)
{
	dms_assert(dcmIndices == DCM_None);
	dms_assert(domainRange.first == 0 && domainRange.second == bit_value<N>::nr_values);
	for (SizeT i = 0; curr != last; ++curr, ++i, ++resultPerE)
	{
		ResIter resPtr = resultPerV + Range_GetIndex_checked(domainRange, *curr);
		*resultPerE = *resPtr;
		*resPtr = Range_GetValue_checked(entityRange, i);
	}
}

template<typename V, typename E>
void invertAll2values_array(
	typename sequence_traits<E>::seq_t resultDataPerV,    // V->E (valuesRange->entityRange)
	typename sequence_traits<E>::seq_t* resultDataPerEPtr, // E->E (entityRange->entityRange)
	typename sequence_traits<V>::cseq_t argData,           // E->V
	typename Unit<V>::range_t valuesRange, 
	typename Unit<E>::range_t entityRange, 
	DataCheckMode  dcmIndices, bool mustInitPerEntity
)
{
	dms_assert(resultDataPerV.size() == Cardinality(valuesRange));
	dms_assert(!resultDataPerEPtr || resultDataPerEPtr->size() == Cardinality(entityRange));
	dms_assert(argData       .size() == Cardinality(entityRange));

	dms_assert((dcmIndices & DCM_CheckDefined) || !ContainsUndefined(valuesRange));

	if (resultDataPerEPtr)
		invertAll2values_best<E>(
			resultDataPerV.begin()
		,	resultDataPerEPtr->begin()
		,	argData.begin(), argData.end()
		,	valuesRange, entityRange
		,	dcmIndices, mustInitPerEntity
		);
	else
		invert2values_best<E>(
			resultDataPerV.begin()
		,	argData.begin(), argData.end()
		,	valuesRange, entityRange
		,	dcmIndices
		);
}

// line 240, 232

#endif //!defined(__CLC_INVERT_H)
