// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__CLC_PCOUNT_H)
#define __CLC_PCOUNT_H

#include "set/BitVector.h"
#include "DataCheckMode.h"
#include "Unit.h"

// *****************************************************************************
//                            PartCount
// *****************************************************************************

template<typename T, typename I>
void pcount_range(const T* sb, const T* se, I rb, SizeT rs, typename Unit<T>::range_t domainRange)
{
	rb -= domainRange.first;
	for (; sb != se; ++sb)
	{
		auto i = *sb;
		assert(IsIncluding(domainRange, i));

		assert(Range_GetIndex_naked(domainRange, i) < rs);
	
		++rb[i];
		if (!rb[i])
			throwErrorF("pcount", "numeric overflow of region %d", i);
	}
}

template<typename T, typename I>
void pcount_range(const Point<T>* sb, const Point<T>* se, I rb, SizeT rs, typename Unit<Point<T>>::range_t domainRange)
{
	for (; sb != se; ++sb)
	{
		assert(IsIncluding(domainRange, *sb));

		SizeT i = Range_GetIndex_naked(domainRange, *sb);
		assert(i < rs);
		++rb[i];
		if (!rb[i])
			throwErrorF("pcount", "numeric overflow of region %d", i);
	}
}

template<typename T, typename I>
void pcount_range_checked(const T* sb, const T* se, I rb, SizeT rs, typename Unit<T>::range_t domainRange)
{
	for (; sb != se; ++sb)
	{
		T v = *sb;
		SizeT i = Range_GetIndex_checked(domainRange, v);
		if (i < rs)
		{
			++rb[i];
			if (!rb[i])
				throwErrorF("pcount", "numeric overflow of region %d", i);
		}
	}
}

template<typename T>
void pcount_range(const T* sb, const T* se, UInt64* rb, SizeT rs, typename Unit<T>::range_t domainRange)
{
	rb -= domainRange.first;
	for (; sb != se; ++sb)
	{
		auto i = *sb;
		assert(IsIncluding(domainRange, i));

		assert(Range_GetIndex_naked(domainRange, i) < rs);
		++rb[i];
		if (!rb[i])
			throwErrorF("pcount", "numeric overflow of region %d", i);
	}
}

template<typename T>
void pcount_range(const Point<T>* sb, const Point<T>* se, UInt64* rb, SizeT rs, typename Unit<Point<T>>::range_t domainRange)
{
	for (; sb != se; ++sb)
	{
		assert(IsIncluding(domainRange, *sb));

		SizeT i = Range_GetIndex_naked(domainRange, *sb);
		assert(i < rs);
		++rb[i];
		if (!rb[i])
			throwErrorF("pcount", "numeric overflow of region %d", i);
	}
}

template<typename T>
void pcount_range_checked(const T* sb, const T* se, UInt64* rb, SizeT rs, typename Unit<T>::range_t domainRange)
{
	for (; sb != se; ++sb)
	{
		T v = *sb;
		SizeT i = Range_GetIndex_checked(domainRange, v);
		if (i < rs)
		{
			++rb[i];
			if (!rb[i])
				throwErrorF("pcount", "numeric overflow of region %d", i);
		}
	}
}

template<typename T, typename I>
void pcount_best(
	const T* sb, const T* se, 
	I rb, SizeT rs, 
	typename Unit<T>::range_t domainRange, 
	DataCheckMode dcm, bool mustInit)
{
	// TODO, variants for range-ok (naked and nonnull)
	//	for ord_point_type_tag (zbase and based) and crd_point_type_tag

	if (mustInit)
		fast_zero(rb, rb+rs);

	if (dcm & (DCM_CheckDefined|DCM_CheckRange))
	{
		dms_assert((dcm != DCM_CheckBoth) || IsIncluding(domainRange, UNDEFINED_VALUE(T) ) );
		pcount_range_checked(sb, se, rb, rs, domainRange);
	}
	else
		pcount_range        (sb, se, rb, rs, domainRange);
}

template<bit_size_t N, typename Block, typename CountType>
void pcount_bitvalues(bit_iterator<N, Block> sb, bit_iterator<N, Block> se, CountType* counts)
{
/* 	REMOVE, TODO, THE FOLLOWING CODE IS PROBABLY FINE FOR N==1, 
	but what about ifs for other N? 
	Maybe I'd prepare an array of 2^N elems that I'd index with the last N bits to check for repeated data.
	This should be a generic facility at the bit_info<N> level.
*/
	for (; sb.m_NrElems && sb != se; ++sb)
		++counts[bit_value<N>(*sb)];
	while (sb.m_BlockData != se.m_BlockData)
	{
		if ((*sb.m_BlockData & bit_info<N, Block>::used_bits_mask) == 0)
		{
			counts[0] += bit_info<N, Block>::nr_elem_per_block;
			++sb.m_BlockData;
		}
		else if ((*sb.m_BlockData & bit_info<N, Block>::used_bits_mask) == bit_info<N, Block>::used_bits_mask)
		{
			counts[bit_info<N, Block>::value_mask] += bit_info<N, Block>::nr_elem_per_block;
			++sb.m_BlockData;
		}
		else
			do {
				++counts[bit_value<N>(*sb)];
				++sb;
			} while (sb.m_NrElems);
	}
	for (; sb != se; ++sb)
		++counts[bit_value<N>(*sb)];
}

template<bit_size_t N, typename Block, typename I>
void pcount_best(
	bit_iterator<N, const Block> sb, bit_iterator<N, const Block> se,
	I rb, SizeT rs, 
	typename Unit<bit_value<N>>::range_t domainRange, 
	DataCheckMode dcm, bool mustInit)
{
	dms_assert(dcm == DCM_None);
	dms_assert(rs == bit_value<N>::nr_values);
	dms_assert(domainRange.first == 0 && domainRange.second == bit_value<N>::nr_values);

	SizeT counts[bit_value<N>::nr_values]; 
	fast_zero(counts, counts + bit_value<N>::nr_values);

	pcount_bitvalues(sb, se, counts);

	using T = bit_value<N>;
	using U = typename value_type_of_iterator<I>::type;

	if (mustInit)
		fast_copy(counts, counts + bit_value<N>::nr_values, rb);
	else
		for (UInt32 i = 0; i != bit_value<N>::nr_values; ++i)
			rb[i] += ThrowingConvertNonNull<U>(counts[i]);
}

template<bit_size_t N, typename Block>
void pcount_best(
	bit_iterator<N, const Block> sb, bit_iterator<N, const Block> se,
	UInt64* rb, SizeT rs, 
	typename Unit<bit_value<N>>::range_t domainRange,
	DataCheckMode dcm, bool mustInit)
{
	dms_assert(dcm == DCM_None);
	dms_assert(rs == bit_value<N>::nr_values);
	dms_assert(domainRange.first == 0 && domainRange.second == bit_value<N>::nr_values);

	if (mustInit)
		fast_zero(rb, rb + bit_value<N>::nr_values);

	pcount_bitvalues(sb, se, rb);
}

#include <ranges>

template<typename E, typename T>
//requires( std::ranges::contiguous_range<ResSequence> && std::ranges::view<IndexDataSequence>)
//requires(std::ranges::contiguous_range<ResSequence>&& std::ranges::view<IndexDataSequence>)
void pcount_container(
	typename sequence_traits<T>::seq_t  resData
,	typename sequence_traits<E>::cseq_t argData
,	typename Unit<E>::range_t domainRange
,	DataCheckMode dcm, bool mustInit)
{
	auto sb = argData.begin(), se = argData.end();
	auto rb = resData.begin();
	SizeT rs = resData.size();

	assert(rs == Cardinality(domainRange));

	pcount_best(sb, se, rb, rs, domainRange, dcm, mustInit);
}


template<typename E>
//requires( std::ranges::contiguous_range<ResSequence> && std::ranges::view<IndexDataSequence>)
//requires(std::ranges::contiguous_range<ResSequence>&& std::ranges::view<IndexDataSequence>)
void has_any_container(
	typename sequence_traits<Bool>::seq_t  resData
	, typename sequence_traits<E>::cseq_t argData
	, typename Unit<E>::range_t domainRange
	, DataCheckMode dcm)
{
	auto sb = argData.begin(), se = argData.end();
	auto rb = resData.begin();
	SizeT rs = resData.size();

	dms_assert(rs == Cardinality(domainRange));
	for (; sb != se; ++sb)
	{
		E v = *sb;
		SizeT i = Range_GetIndex_checked(domainRange, v);
		if (i < rs)
			rb[i] = true;
	}
}


#endif //!defined(__CLC_PCOUNT_H)
