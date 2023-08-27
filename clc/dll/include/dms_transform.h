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

#if !defined(__CLC_DMS_TRANSFORM_H)
#define __CLC_DMS_TRANSFORM_H

// REMOVE OR MOVE THE FOLLOWING TO rtc/dll/src/cpc/CompChar.h
#if defined(CC_STL_1300) && defined(MG_DEBUG)
#	define CC_FIX_TRANSFORM_CHECK
#endif

#include <execution>
#include "Prototypes.h"

// *****************************************************************************
//								additional transform algorithms
// *****************************************************************************

template<typename InpIter1, typename OutIter, typename UnaOper> inline
std::enable_if_t< !has_block_func_v<UnaOper>>
dms_transform(
	InpIter1 f1
,	InpIter1 l1
,	OutIter outIter
,	UnaOper&& oper
)
{	
#if defined(CC_FIX_TRANSFORM_CHECK)
	if (f1 == l1)
		return;
#endif
/*	NYI, filter out operations that have mutable working states, such as CentroidOrMid and Mid; and operations and value types that require sequential operation

	using res_value_type = std::iterator_traits<OutIter>::value_type;
	if constexpr (is_separable_v<res_value_type>)
		std::transform(std::execution::par_unseq, f1, l1, outIter, oper);
	else
*/
	//std::transform(f1, l1, outIter, oper);
	using result_value_type = typename std::iterator_traits<OutIter>::value_type;
	SizeT n = l1 - f1;
	parallel_for_if_separable<SizeT, result_value_type>(0, n, [oper = std::forward<UnaOper>(oper), f1, outIter](SizeT i) { outIter[i] = oper(f1[i]); });
}

template<bit_size_t N, typename CBlock, typename Block, typename UnaOper>
std::enable_if_t<has_block_func_v<UnaOper> >
dms_transform(
	bit_iterator<N, CBlock> f1,
	bit_iterator<N, CBlock> l1,
	bit_iterator<N, Block> outIter,
	UnaOper oper
)
{
	static_assert(std::is_same_v<typename UnaOper::arg1_type, bit_value<N> >);
	static_assert(std::is_same_v<typename UnaOper::res_type, bit_value<N> >);

	dms_assert(!f1.nr_elem());
	dms_assert(!outIter.nr_elem());

	dms_transform(f1.data_begin(), l1.data_end(), outIter.data_begin(), oper.m_BlockFunc);
}

template<typename InpIter1, typename InpIter2, typename OutIter, typename BinOper> inline
std::enable_if_t<!has_block_func_v<BinOper> >
dms_transform(
	InpIter1 f1 
,	InpIter1 l1 
,	InpIter2 f2 
,	OutIter outIter 
,	BinOper oper
)
{
	static_assert( std::is_same_v<typename BinOper::arg1_type, typename std::iterator_traits<InpIter1>::value_type> );
	static_assert( std::is_same_v<typename BinOper::arg2_type, typename std::iterator_traits<InpIter2>::value_type> );
	static_assert( std::is_same_v<typename BinOper::res_type, typename std::iterator_traits<OutIter >::value_type> );

#if defined(CC_FIX_TRANSFORM_CHECK)
	if (f1 == l1)
		return;
#endif

/*	NYI, filter out operations that have mutable working states, such as CentroidOrMid and Mid; and operations and value types that require sequential operation
* 
	using res_value_type = std::iterator_traits<OutIter>::value_type;
	if constexpr (is_separable_v<res_value_type>)
		std::transform(std::execution::par, f1, l1, f2, outIter, oper);
	else
*/
	std::transform(f1, l1, f2, outIter, oper);
}

template<bit_size_t N, typename CBlock, typename Block, typename BinOper>
std::enable_if_t<has_block_func_v<BinOper>>
dms_transform(
	bit_iterator<N, CBlock> f1,
	bit_iterator<N, CBlock> l1,
	bit_iterator<N, CBlock> f2,
	bit_iterator<N, Block> outIter,
	BinOper oper
)
{
	static_assert(std::is_same_v<typename BinOper::arg1_type, bit_value<N> >);
	static_assert(std::is_same_v<typename BinOper::arg2_type, bit_value<N> >);
	static_assert(std::is_same_v<typename BinOper::res_type,  bit_value<N> >);

	dms_assert(!f1.nr_elem());
	dms_assert(!f2.nr_elem());
	dms_assert(!outIter.nr_elem());

	dms_transform(f1.data_begin(), l1.data_end(), f2.data_begin(), outIter.data_begin(), oper.m_BlockFunc);
}

// ternary tranform function
template<typename TInputIter1, typename TInputIter2, typename TInputIter3, typename TOutputIter, typename TTernaryOperator> inline
TOutputIter dms_transform(TInputIter1 in1, TInputIter1 end1, TInputIter2 in2, TInputIter3 in3, TOutputIter out, TTernaryOperator oper)
{
	for (; in1 != end1; ++in1, ++in2, ++in3, ++out)
		*out = oper(*in1, *in2, *in3);
	return out; 
}

// nullary tranform_assign function
template<typename TOutputIter, typename TNullaryAssign> inline
void transform_assign(TOutputIter out, TOutputIter outEnd, TNullaryAssign oper)
{
	for (; out != outEnd; ++out)
		oper(*out);
}

// optimization specialization for assign_default
template <typename T> struct assign_default; // fwd decl
template<typename TOutputIter, typename T> inline
void transform_assign(TOutputIter out, TOutputIter outEnd, assign_default<T> oper)
{
	static_assert(std::is_same_v<typename std::iterator_traits<TOutputIter>::value_type, T>);
	fast_zero(out, outEnd);
}

#include <iterator>

//AMP Depreciated
// check out: https://en.wikipedia.org/wiki/SYCL

#define _SILENCE_AMP_DEPRECATION_WARNINGS
#include <amp.h>

// unary tranform_assign function
template<typename TInputIter1, typename TOutputIter, typename TUnaryAssign> inline
TOutputIter transform_assign_cpu(TOutputIter out, TInputIter1 in1, TInputIter1 end1, TUnaryAssign oper)
{
	for (; in1 != end1; ++out, ++in1)
		oper(*out, *in1);
	return out;
}

// unary tranform_assign function
template<typename TInputIter1, typename TOutputIter, typename TUnaryAssign> inline
TOutputIter transform_assign_amp(TOutputIter out, TInputIter1 in1, TInputIter1 end1, TUnaryAssign oper)
{
	using arg_value_type = typename std::iterator_traits<TInputIter1>::value_type;
	using res_value_type = typename std::iterator_traits<TOutputIter>::value_type;

	SizeT sz = std::distance(in1, end1);
	while (sz)
	{
		static_assert(sizeof(arg_value_type) < (1 << 7));
		static_assert(sizeof(res_value_type) < (1 << 7));

		SizeT blockSize = sz;
		MakeMin(blockSize, (1 << 24));
		sz -= blockSize;

		concurrency::array_view<const arg_value_type, 1> src(blockSize, in1);
		concurrency::array_view<res_value_type, 1> dst(blockSize, out);

		concurrency::parallel_for_each(dst.extent,
			[=](concurrency::index<1> idx) restrict(amp, cpu)
			{
				oper(dst[idx], src[idx]);
			}
		);
	}
	return out + sz;
}

//template <typename T> constexpr bool is_ampable_v = std::is_integral_v<T> && !(sizeof(T) % sizeof(int));
template <typename T> constexpr bool is_ampable_v = false; // std::is_integral_v<T> && (sizeof(T) == sizeof(int));
template <typename T> concept Ampable = is_ampable_v<T>;

// unary tranform_assign function
template<typename TInputIter1, typename TOutputIter, typename TUnaryAssign> inline
void transform_assign(TOutputIter out, TInputIter1 in1, TInputIter1 end1, TUnaryAssign oper)
{
	using arg_value_type = typename std::iterator_traits<TInputIter1>::value_type;
	using res_value_type = typename std::iterator_traits<TOutputIter>::value_type;

	if constexpr (is_ampable_v<arg_value_type> && is_ampable_v<res_value_type> && Ampable<TUnaryAssign>)
		transform_assign_amp(out, in1, end1, oper);
	else
		transform_assign_cpu(out, in1, end1, oper);
}


// binary tranform_assign function
template<typename TInputIter1, typename TInputIter2, typename TOutputIter, typename TBinaryAssign> inline
void transform_assign(TOutputIter out, TInputIter1 in1, TInputIter1 end1, TInputIter2 in2, TBinaryAssign oper)
{
	for (; in1 != end1; ++out, ++in2, ++in1)
		oper(*out, *in1, *in2);
}

// ternary tranform_assign function
template<typename TInputIter1, typename TInputIter2, typename TInputIter3, typename TOutputIter, typename TTernaryAssign> inline
void transform_assign(TOutputIter out, TInputIter1 in1, TInputIter1 end1, TInputIter2 in2, TInputIter3 in3, TTernaryAssign oper)
{
	for (; in1 != end1; ++out, ++in3, ++in2, ++in1)
		oper(*out, *in1, *in2, *in3);
}

#endif //!defined(__CLC_DMS_TRANSFORM_H)