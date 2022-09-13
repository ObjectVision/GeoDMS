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

#include "ClcPCH.h"
#pragma hdrstop

#if !defined(__CLC_RLOOKUP_H)
#define __CLC_RLOOKUP_H 

#include "UnitProcessor.h"

#include "OperRelUni.h"

#include "ParallelTiles.h"
#include "act/any.h"
#include "mci/CompositeCast.h"

// *****************************************************************************
//                         helper funcsRLookupOperator
// *****************************************************************************

template <typename II, typename T, typename TI>
UInt32 rlookup2index(II ib, II ie, const T& dataValue, TI classBoundsPtr)
{
	II i = indexed_lowerbound(ib, ie, classBoundsPtr, dataValue);
	if (i != ie && classBoundsPtr[*i] == dataValue)
		return *i;
	return UNDEFINED_VALUE(UInt32);
}

template <typename II, typename T, typename TI>
UInt32 rlookup2index_checked(II ib, II ie, const T& dataValue, TI classBoundsPtr)
{
	if (IsDefined(dataValue))
	{
		II i = indexed_lowerbound(ib, ie, classBoundsPtr, dataValue);
		if (i != ie && classBoundsPtr[*i] == dataValue)
			return *i;
	}
	return UNDEFINED_VALUE(UInt32);
}

template <typename E, typename II, typename T, typename TI>
E rlookup2value(II ib, II ie, const T& dataValue, TI classBoundsPtr, const typename Unit<E>::range_t& resRange)
{
	II i = indexed_lowerbound(ib, ie, classBoundsPtr, dataValue);
	if (i != ie && classBoundsPtr[*i] == dataValue)
		return Range_GetValue_naked(resRange, *i);
	return UNDEFINED_OR_ZERO(E);
}

template <typename E, typename II, typename T, typename TI>
E rlookup2value_checked(II ib, II ie, const T& dataValue, TI classBoundsPtr, const typename Unit<E>::range_t& resRange)
{
	if (IsDefined(dataValue))
	{
		II i = indexed_lowerbound(ib, ie, classBoundsPtr, dataValue);
		if (i != ie && classBoundsPtr[*i] == dataValue)
			return Range_GetValue_naked(resRange, *i);
	}
	return UNDEFINED_OR_ZERO(E);
}

template <typename RI, typename II, typename TI>
TI rlookup2index_range(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr)
{
	SizeT n = re-rb;
	using result_type = typename std::iterator_traits<RI>::value_type;
	parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
		{
			rb[i] = rlookup2index(ib, ie, dataPtr[i], classBoundsPtr);
		}
	);
	return dataPtr + n;
}

template <typename RI, typename II, typename TI>
TI rlookup2index_range_checked(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr)
{
	SizeT n = re-rb;
	using result_type = typename std::iterator_traits<RI>::value_type;
	parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
		{
			rb[i] = rlookup2index_checked(ib, ie, dataPtr[i], classBoundsPtr);
		}
	);
	return dataPtr + n;
}

template <typename RI, typename II, typename TI>
typename std::enable_if<has_undefines_v<typename std::iterator_traits<TI>::value_type>, TI >::type
rlookup2index_range_best(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, bool hasUndefinedValues)
{
	return (hasUndefinedValues)
		?	rlookup2index_range_checked<RI, II, TI>(rb, re, ib, ie, dataPtr, classBoundsPtr)
		:	rlookup2index_range        <RI, II, TI>(rb, re, ib, ie, dataPtr, classBoundsPtr);
}

template <typename RI, typename II, typename TI>
typename std::enable_if<!has_undefines_v<typename std::iterator_traits<TI>::value_type>, TI >::type
rlookup2index_range_best(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, bool hasUndefinedValues)
{
	dms_assert(!hasUndefinedValues);
	return rlookup2index_range<RI, II, TI >(rb, re, ib, ie, dataPtr, classBoundsPtr);
}

template <typename RI, typename II, typename TI, typename RangeType>
TI rlookup2value_range(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange)
{
	SizeT n = re-rb;
	using result_type = typename std::iterator_traits<RI>::value_type;
	parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
		{
			rb[i] = rlookup2value<std::iterator_traits<RI>::value_type>(ib, ie, dataPtr[i], classBoundsPtr, resRange);
		}
	);
	return dataPtr + n;
}

template <typename RI, typename II, typename TI, typename RangeType>
TI rlookup2value_range_checked(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange)
{
	SizeT n = re-rb;
	using result_type = typename std::iterator_traits<RI>::value_type;
	parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
		{
			rb[i] = rlookup2value_checked<std::iterator_traits<RI>::value_type>(ib, ie, dataPtr[i], classBoundsPtr, resRange);
		}
	);
	return dataPtr + n;
}

template <typename RI, typename II, typename TI, typename RangeType>
typename std::enable_if<has_undefines_v<typename std::iterator_traits<TI>::value_type>, TI >::type
rlookup2value_range_best(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange, bool hasUndefinedValues)
{
	return (hasUndefinedValues)
		?	rlookup2value_range_checked<RI, II, TI, RangeType>(rb, re, ib, ie, dataPtr, classBoundsPtr, resRange)
		:	rlookup2value_range        <RI, II, TI, RangeType>(rb, re, ib, ie, dataPtr, classBoundsPtr, resRange);
}
		
template <typename RI, typename II, typename TI, typename RangeType>
typename std::enable_if<!has_undefines_v<typename std::iterator_traits<TI>::value_type>, TI >::type
rlookup2value_range_best(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange, bool hasUndefinedValues)
{
	dms_assert(!hasUndefinedValues);
	return rlookup2value_range<RI, II, TI, RangeType >(rb, re, ib, ie, dataPtr, classBoundsPtr, resRange);
}
		

template <typename Vec, typename TArray>
void rlookup2index_array_unchecked(Vec& resData,
				 const TArray& arg1Data, // E1->V
				 const TArray& arg2Data  // E2->V
)
{
	dms_assert(resData.size()  == arg1Data.size());

	// First, make a index mapping E2->E2  for arg2.
	std::vector<UInt32> index(arg2Data.size()); 
	auto
		indexBegin = index.begin(),
		indexEnd   = index.end();

	auto a2Db = arg2Data.begin();

	// Sort index 
	make_index(indexBegin, indexEnd, a2Db);

	// Then, do a binary search of each element 
	auto a1De = rlookup2index_range(resData.begin(), resData.end(), indexBegin, indexEnd, arg1Data.begin(), a2Db);

	dms_assert(a1De == arg1Data.end());
}

template <typename I, typename V>
using indexed_tile_t = std::pair<std::vector<I>, typename DataArray<V>::locked_cseq_t>;

template <typename I, typename V>
auto make_index_array(typename DataArray<V>::locked_cseq_t arg2Data) -> indexed_tile_t<I, V>
{
	// make a mapping V->E2  from arg2. Assign in backward order to keep the first occurence
	std::vector<I> index(arg2Data.size());
	make_index(index.begin(), index.end(), arg2Data.begin());
	return { std::move(index), std::move(arg2Data) };
}

// *****************************************************************************
//                         helper funcs for ClassifyOperator
// *****************************************************************************

template <typename II, typename T, typename TI>
UInt32 classify2index(II ib, II ie, const T& dataValue, TI classBoundsPtr)
{
	II i = indexed_upperbound(ib, ie, classBoundsPtr, dataValue);
	if (i != ib)
	{
		// Check
		MGD_CHECKDATA( 
				(	i == ie ||		dataValue < classBoundsPtr[*i] )
			&&	(/*	i == ib ||*/ !(	dataValue < classBoundsPtr[ i[-1] ] ) ) 
		);

		// Action
		return i[-1];
	}
	return UNDEFINED_VALUE(UInt32);
}

template <typename II, typename T, typename TI>
UInt32 classify2index_checked(II ib, II ie, const T& dataValue, TI classBoundsPtr)
{
	return (IsDefined(dataValue))
		?	classify2index<II, T, TI>(ib, ie, dataValue, classBoundsPtr)
		:	UNDEFINED_VALUE(UInt32)
	;
}

template <typename II, typename T, typename TI, typename E>
E classify2value(II ib, II ie, const T& dataValue, TI classBoundsPtr, const Range<E>& resRange)
{
	II i = indexed_upperbound(ib, ie, classBoundsPtr, dataValue);
	if (i != ib)
	{
		// Check
		MGD_CHECKDATA( 
				(i == ie ||	DataCompare<T>()(dataValue, classBoundsPtr[*i]) )
			&&	!DataCompare<T>()(dataValue, classBoundsPtr[*(i-1) ]) 
		);

		// Action
		return Range_GetValue_naked(resRange, *(i-1) );
	}
	return UNDEFINED_VALUE(E);
}

template <typename II, typename T, typename TI, typename E>
E classify2value_checked(II ib, II ie, const T& dataValue, TI classBoundsPtr, const Range<E>& resRange)
{
	if (IsDefined(dataValue))
		return classify2value<II,T, TI, E>(ib, ie, dataValue, classBoundsPtr, resRange);
	return UNDEFINED_VALUE(E);
}

template <typename RI, typename II, typename TI>
TI classify2index_range(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr)
{
	SizeT n = re-rb;
	using result_type = typename std::iterator_traits<RI>::value_type;
	parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
		{
			rb[i] = classify2index(ib, ie, dataPtr[i], classBoundsPtr);
		}
	);
	return dataPtr + n;
}

template <typename RI, typename II, typename TI>
TI classify2index_range_checked(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr)
{
	SizeT n = re-rb;
	using result_type = typename std::iterator_traits<RI>::value_type;
	parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
		{
			rb[i] = classify2index_checked(ib, ie, dataPtr[i], classBoundsPtr);
		}
	);
	return dataPtr + n;
}

template <typename RI, typename II, typename TI>
typename std::enable_if<has_undefines_v<typename std::iterator_traits<TI>::value_type>, TI >::type
classify2index_range_best(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, bool hasUndefinedValues)
{
	return (hasUndefinedValues)
		?	classify2index_range_checked<RI, II, TI>(rb, re, ib, ie, dataPtr, classBoundsPtr)
		:	classify2index_range        <RI, II, TI>(rb, re, ib, ie, dataPtr, classBoundsPtr);
}

template <typename RI, typename II, typename TI>
typename std::enable_if<!has_undefines_v<typename std::iterator_traits<TI>::value_type>, TI >::type
classify2index_range_best(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, bool hasUndefinedValues)
{
	dms_assert(!hasUndefinedValues);
	return classify2index_range<RI, II, TI>(rb, re, ib, ie, dataPtr, classBoundsPtr);
}

template <typename RI, typename II, typename TI, typename RangeType>
TI classify2value_range(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange)
{
	SizeT n = re-rb;
	using result_type = typename std::iterator_traits<RI>::value_type;
	parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
		{
			rb[i] = classify2value(ib, ie, dataPtr[i], classBoundsPtr, resRange);
		}
	);
	return dataPtr + n;
}

template <typename RI, typename II, typename TI, typename RangeType>
TI classify2value_range_checked(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange)
{
	SizeT n = re-rb;
	using result_type = typename std::iterator_traits<RI>::value_type;
	parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
		{
			rb[i] = classify2value_checked(ib, ie, dataPtr[i], classBoundsPtr, resRange);
		}
	);

	return dataPtr + n;
}

template <typename RI, typename II, typename TI, typename RangeType>
typename std::enable_if<has_undefines_v<typename std::iterator_traits<TI>::value_type>, TI >::type
classify2value_range_best(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange, bool hasUndefinedValues)
{
	return (hasUndefinedValues)
		?	classify2value_range_checked<RI, II, TI, RangeType>(rb, re, ib, ie, dataPtr, classBoundsPtr, resRange)
		:	classify2value_range        <RI, II, TI, RangeType>(rb, re, ib, ie, dataPtr, classBoundsPtr, resRange);
}

template <typename RI, typename II, typename TI, typename RangeType>
typename std::enable_if<!has_undefines_v<typename std::iterator_traits<TI>::value_type>, TI >::type
classify2value_range_best(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange, bool hasUndefinedValues)
{
	dms_assert(!hasUndefinedValues);
	return classify2value_range<RI, II, TI, RangeType>(rb, re, ib, ie, dataPtr, classBoundsPtr, resRange);
}

// **********************************************  Dispatchers
// disp(resData, m_Arg1Data, indexedTilePtr->second, inviter->GetRange(), indexedTilePtr->first, m_MustCheck);

struct rlookup_dispatcher {
	template <typename SequenceView, typename TArray1, typename TArray2, typename E>
	void operator ()(SequenceView resData, const TArray1& arg1Data, const TArray2& arg2Data,
		const Range<E>& arg2DomainRange,
		const std::vector<typename cardinality_type<E>::type>& index,
		bool hasUndefinedValues)
	// requires std::is_same_v< Unit<Vec::value_type>::range_t, Range<E> >
	// requires std::is_same_v< TArray1::value_type, TArray2::value_type >
	{
		dms_assert(resData.size() == arg1Data.size());

		// do a binary search of each element 
		auto a2De = rlookup2value_range_best(resData.begin(), resData.end(), begin_ptr(index), end_ptr(index), arg1Data.begin(), arg2Data.begin(), arg2DomainRange, hasUndefinedValues);
		dms_assert(a2De == arg1Data.end());
	}
};

struct classify_dispatcher {
	template <typename SequenceView, typename TArray1, typename TArray2, typename E>
	void operator ()(SequenceView resData, const TArray1& arg1Data, const TArray2& arg2Data,
		const Range<E>& arg2DomainRange,
		const std::vector<typename cardinality_type<E>::type>& index,
		bool hasUndefinedValues)
	{
		dms_assert(resData.size() == arg1Data.size());

		// do a binary search of each element 
		auto a2De = classify2value_range_best(resData.begin(), resData.end(), begin_ptr(index), end_ptr(index), arg1Data.begin(), arg2Data.begin(), arg2DomainRange, hasUndefinedValues);
		dms_assert(a2De == arg1Data.end());
	}
};

#endif // !defined(__CLC_RLOOKUP_H)

