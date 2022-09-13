//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#if !defined(__CLC_OPERRELUNI_H)
#define __CLC_OPERRELUNI_H

#include <algorithm>
#include <map>
#include <set>

#include "ser/AsString.h"
#include "set/VectorFunc.h"
#include "geo/StringBounds.h"
#include "set/IndexCompare.h"
#include "set/VectorFunc.h"

#include "makeCululative.h"
#include "pcount.h"
#include "prototypes.h"

// *****************************************************************************
//                      INDEX operations
// *****************************************************************************

template <typename CI2>
struct IndexPCompareOper 
{
	IndexPCompareOper(const IndexGetter* unsortedPartitionData, CI2 data2Begin)
		:	m_IndexData(unsortedPartitionData) 
		,	m_Data2Begin(data2Begin) 
	{
		dms_assert(unsortedPartitionData);
	}
	bool operator ()(SizeT left, SizeT right)
	{ 
		SizeT 
			pl = m_IndexData->Get(left),
			pr = m_IndexData->Get(right);
		return (pl < pr)
			||	(	!	(pr < pl) 
					&&	m_DataComp(m_Data2Begin[left ], m_Data2Begin[right])
				);

	}
	typedef typename std::iterator_traits<CI2>::value_type value_type;
	const IndexGetter* m_IndexData;
	CI2                m_Data2Begin;
	DataCompare<value_type> m_DataComp;
};

template <typename InIt, typename vIt, typename V>
InIt indexed_lowerbound(InIt first, InIt last, vIt beginData, const V& value)
{
	SizeT n = last - first;
	DataCompare<V> comp;
	while (n)
	{
		SizeT n2 = n / 2;
		InIt m = first + n2;
		if (comp(beginData[*m], value))
			first = ++m, n -= (n2+1);
		else
			n = n2;
	}
	return first;
}

template <typename InIt, typename vIt, typename V>
InIt indexed_upperbound(InIt first, InIt last, vIt beginData, const V& value)
{
	SizeT n = last - first;
	DataCompare<V> comp;
	while (n)
	{
		SizeT n2 = n / 2;
		InIt m = first + n2;
		if (!comp(value, beginData[*m]))
			first = ++m, n -= (n2+1);
		else
			n = n2;
	}
	return first;
}

// *****************************************************************************
//                         UNARY RELATIONAL FUNCTIONS
// *****************************************************************************

template<typename IndexIter, typename ConstDataIter>
void make_index(
		IndexIter resDataBegin, 
		IndexIter resDataEnd, 
		ConstDataIter unsortedDataBegin
	)
{
	typedef std::iterator_traits<IndexIter>::value_type IndexValue;

	fill_id(resDataBegin, resDataEnd);
	std::stable_sort(
		resDataBegin, 
		resDataEnd, 
		IndexCompareOper<ConstDataIter, IndexValue>(
			unsortedDataBegin
		)
	);
}

template<typename IndexIter, bit_size_t N, typename CB>
void make_index(
		IndexIter resDataBegin, 
		IndexIter resDataEnd, 
		bit_iterator<N, CB> unsortedDataBegin
	)
{
	UInt32 nrElem = resDataEnd - resDataBegin;
	bit_iterator<N, CB> unsortedDataEnd = unsortedDataBegin + nrElem;

	UInt32 counts [ bit_value<N>::nr_values ];
	fast_zero(counts, counts + bit_value<N>::nr_values);

	pcount_bitvalues(unsortedDataBegin, unsortedDataEnd, counts);
	make_cumulative(counts, counts+bit_value<N>::nr_values);
	dms_assert(counts[bit_value<N>::nr_values - 1] == nrElem);

//  The following code gives a stable rank nr to each element
//
//	while (resDataEnd != resDataBegin)
//		*--resDataEnd = --counts[bit_value<N>(*--unsortedDataEnd)];
//
	while (nrElem)
		resDataBegin[--counts[bit_value<N>(*--unsortedDataEnd)]] = --nrElem;
	dms_assert(counts[0] == 0);
	dms_assert(unsortedDataEnd == unsortedDataBegin);
}

typedef const UInt32* ConstOrderIter;

template<typename IndexIter, typename ConstIndexIter, typename ConstDataIter>
void make_subindex(
		IndexIter resDataBegin, 
		IndexIter resDataEnd, 
		ConstIndexIter prevIndexBegin,
		ConstOrderIter prevOrderBegin,
		ConstDataIter unsortedDataBegin
	)
{
	typedef std::iterator_traits<IndexIter>::value_type IndexValue;

	UInt32 size = resDataEnd - resDataBegin;
	ConstIndexIter prevIndexEnd = prevIndexBegin + size;
	ConstOrderIter prevOrderEnd = prevOrderBegin + size;
	fast_copy(prevIndexBegin, prevIndexEnd, resDataBegin);

	ConstOrderIter prevOrderRangeBegin = prevOrderBegin;
	while (true)
	{
		prevOrderRangeBegin = std::adjacent_find(prevOrderRangeBegin, prevOrderEnd); // skip ordering elements of singletons
		if (prevOrderRangeBegin == prevOrderEnd)
			return;
		dms_assert(prevOrderRangeBegin+1 != prevOrderEnd); // 
		typename std::iterator_traits<ConstOrderIter>::value_type v = *prevOrderRangeBegin;
		dms_assert(v == prevOrderRangeBegin[1]); // we found an adjacent pair
		ConstOrderIter prevOrderRangeEnd = prevOrderRangeBegin+2;
		while (prevOrderRangeEnd != prevOrderEnd && *prevOrderRangeEnd == v)
			++prevOrderRangeEnd;
		std::stable_sort(
			resDataBegin + SizeT(prevOrderRangeBegin - prevOrderBegin), 
			resDataBegin + SizeT(prevOrderRangeEnd   - prevOrderBegin), 
			IndexCompareOper<ConstDataIter, IndexValue>(
				unsortedDataBegin
			)
		);
		prevOrderRangeBegin = prevOrderRangeEnd;
	}
}

template<typename ConstIter2>
void make_indexP(
		SizeT*       resDataBegin, 
		SizeT*       resDataEnd, 
		IndexGetter* unsortedPartitionData, 
		ConstIter2   unsortedData2Begin)
{
	fill_id(resDataBegin, resDataEnd);
	std::stable_sort(
		resDataBegin, 
		resDataEnd, 
		IndexPCompareOper<ConstIter2>(
			unsortedPartitionData,
			unsortedData2Begin
		)
	);
}

template <typename F>
typename unsigned_type<F>::type* unsigned_ptr(F* ptr)
{
	return reinterpret_cast<typename unsigned_type<F>::type*>(ptr);
}

template <bit_size_t N, typename Block>
bit_iterator<N, Block> unsigned_ptr(const bit_iterator<N, Block>& ptr)
{
	return ptr;
}

template <typename E, typename ResSequence, typename DataArray>
void make_index_container(
	ResSequence resData,
	const DataArray& arg1Data, 
	const Range<E>&  arg1Domain,
	const ord_type_tag*)
{
	auto 
		resBegin = resData.begin(), 
		resEnd   = resData.end();

	make_index(unsigned_ptr(resBegin), unsigned_ptr(resEnd), arg1Data.begin());
	Range_Index2Value_Inplace_naked(arg1Domain, resBegin, resEnd);
}

template <typename E, typename ResSequence, typename DataArray>
void make_index_container(
	ResSequence  resData,
	const DataArray& arg1Data, 
	const Range<E>&  arg1Domain,
	const crd_point_type_tag*)
{
	std::vector<row_id> temp(resData.size());
	make_index(temp.begin(), temp.end(), arg1Data.begin());
	Range_Index2Value_naked(arg1Domain, begin_ptr( temp ), end_ptr( temp ), resData.begin());
}

using OrderArray = sequence_traits<UInt32>::cseq_t;

template <typename E, typename ResRange, typename IndexArray, typename ValueArray>
void make_subindex_container(
	      ResRange resData, 
	const IndexArray&  prevIndexData, 
	const OrderArray& prevOrderData, 
	const ValueArray& argData, 
	const Range<E>&  domain,
	const ord_type_tag*)
{
	dms_assert(resData.size() == prevIndexData.size());
	dms_assert(resData.size() == prevOrderData.size());
	dms_assert(resData.size() == argData.size());

	auto
		resDataBegin = resData.begin(),
		resDataEnd   = resData.end();

	if (!resData.size())
		return;

	if (domain.first)
	{
		typename sequence_traits<typename IndexArray::value_type>::container_type prevIndexTemp(prevIndexData.size()); 
		Range_Value2Index_checked(domain, prevIndexData.begin(), prevIndexData.end(), prevIndexTemp.begin());
		make_subindex(
			unsigned_ptr(resDataBegin), 
			unsigned_ptr(resDataEnd), 
			unsigned_ptr(&*prevIndexTemp.begin()), 
			unsigned_ptr(prevOrderData.begin()), 
			argData.begin()
		);
	}
	else
		make_subindex(
			unsigned_ptr(resDataBegin), 
			unsigned_ptr(resDataEnd), 
			unsigned_ptr(prevIndexData.begin()), 
			unsigned_ptr(prevOrderData.begin()), 
			argData.begin()
		);
	Range_Index2Value_Inplace_checked(domain, resDataBegin, resDataEnd);
}

template <typename ResRange, typename IndexArray, typename ValueArray>
void make_subindex_container(
	      ResRange resData, 
	const IndexArray&    prevIndexData, 
	const OrderArray&    prevOrderData, 
	const ValueArray&    argData, 
	const Range<UInt32>& domain,
	const bool_type_tag*)
{
	dms_assert(resData.size() == prevIndexData.size());
	dms_assert(resData.size() == prevOrderData.size());
	dms_assert(resData.size() == argData.size());

	auto
		resDataBegin = resData.begin(),
		resDataEnd   = resData.end();

	dms_assert(!domain.first);
	make_subindex(
		resDataBegin
	,	resDataEnd
	,	prevIndexData.begin()
	,	prevOrderData.begin()
	,	argData.begin()
	);
}

template <typename E, typename ResRange, typename IndexArray, typename ValueArray>
void make_subindex_container(
	      ResRange resData, 
	const IndexArray& prevIndexData, 
	const OrderArray& prevOrderData, 
	const ValueArray& argData, 
	const Range<E>&   domain,
	const crd_point_type_tag*)
{
	dms_assert(resData.size() == prevIndexData.size());
	dms_assert(resData.size() == prevOrderData.size());
	dms_assert(resData.size() == argData.size());

	std::vector<UInt32> prevIndexTemp(prevIndexData.size()); 
	Range_Value2Index_checked(domain, begin_ptr(prevIndexData), end_ptr(prevIndexData), begin_ptr(prevIndexTemp));
	std::vector<UInt32> resTemp(resData.size());

	make_subindex(
		resTemp.begin(), 
		resTemp.end(), 
		prevIndexTemp.begin(), 
		prevOrderData.begin(), 
		argData.begin()
	);
	Range_Index2Value_checked(domain, begin_ptr(resTemp), end_ptr( resTemp ), resData.begin());
}

#endif // !defined(__CLC_OPERRELUNI_H)


