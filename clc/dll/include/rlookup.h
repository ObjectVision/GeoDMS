// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////


#include "ClcPCH.h"

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
namespace {

	template <typename T>
	using index_type_t = std::conditional_t<sizeof(cardinality_type_t<T>) <= 4, UInt32, UInt64>;

	template <typename II, typename T, typename TI>
	auto rlookup2index(II ib, II ie, const T& dataValue, TI classBoundsPtr) -> std::iterator_traits<II>::value_type
	{
		if (IsBitValueOrDefined(dataValue))
		{
			II i = indexed_lowerbound(ib, ie, classBoundsPtr, dataValue);
			if (i != ie && classBoundsPtr[*i] == dataValue)
				return *i;
		}
		return UNDEFINED_VALUE(std::iterator_traits<II>::value_type);
	}

	template <typename T, typename TI>
	SizeT binarySearch2index(const T& dataValue, TI classBoundsPtr, TI classBoundsEnd)
	{
		if (IsBitValueOrDefined(dataValue))
		{
			TI i = lowerbound(classBoundsPtr, classBoundsEnd, dataValue);
			if (i != classBoundsEnd && *i == dataValue)
				return i - classBoundsPtr;
		}
		return UNDEFINED_VALUE(SizeT);
	}

	template <typename II, typename T, typename TI>
	auto rlookup2index_with_null(II ib, II ie, const T& dataValue, TI classBoundsPtr) -> std::iterator_traits<II>::value_type
	{
		using V = typename std::iterator_traits<TI>::value_type;
		DataEqualityCompare<V> eqCmp;

		II i = indexed_lowerbound_with_null(ib, ie, classBoundsPtr, dataValue);
		if (i != ie && eqCmp(classBoundsPtr[*i], dataValue))
			return *i;
		return UNDEFINED_VALUE(std::iterator_traits<II>::value_type);
	}

	template <typename T, typename TI>
	SizeT binarySearch2index_with_null(const T& dataValue, TI classBoundsPtr, TI classBoundsEnd)
	{
		using V = typename std::iterator_traits<TI>::value_type;
		DataEqualityCompare<V> eqCmp;

		TI i = lowerbound_with_null(classBoundsPtr, classBoundsEnd, dataValue);
		if (i != classBoundsEnd && eqCmp(*i, dataValue))
			return i - classBoundsPtr;

		return UNDEFINED_VALUE(SizeT);
	}

	template <typename RI, typename II, typename TI>
	TI rlookup2index_range(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr)
	{
		SizeT n = re - rb;
		using result_type = typename std::iterator_traits<RI>::value_type;
		parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
			{
				rb[i] = Convert<result_type>(rlookup2index(ib, ie, dataPtr[i], classBoundsPtr));
			}
		);
		return dataPtr + n;
	}

	template <typename E, typename T, typename TI>
	E binarySearch2value(const T& dataValue, TI classBoundsPtr, TI classBoundsEnd, const typename Unit<E>::range_t& resRange)
	{
		auto index = binarySearch2index(dataValue, classBoundsPtr, classBoundsEnd);
		if (IsDefined(index))
			return Range_GetValue_naked(resRange, index);
		return UNDEFINED_OR_ZERO(E);
	}

	template <typename E, typename T, typename TI>
	E binarySearch2value_with_null(const T& dataValue, TI classBoundsPtr, TI classBoundsEnd, const typename Unit<E>::range_t& resRange)
	{
		auto index = binarySearch2index_with_null(dataValue, classBoundsPtr, classBoundsEnd);
		if (IsDefined(index))
			return Range_GetValue_naked(resRange, index);
		return UNDEFINED_OR_ZERO(E);
	}

	template <typename E, typename II, typename T, typename TI>
	E rlookup2value(II ib, II ie, const T& dataValue, TI classBoundsPtr, const typename Unit<E>::range_t& resRange)
	{
		auto index = rlookup2index(ib, ie, dataValue, classBoundsPtr);
		if (IsDefined(index))
			return Range_GetValue_naked(resRange, index);
		return UNDEFINED_OR_ZERO(E);
	}

	template <typename E, typename II, typename T, typename TI>
	E rlookup2value_with_null(II ib, II ie, const T& dataValue, TI classBoundsPtr, const typename Unit<E>::range_t& resRange)
	{
		auto index = rlookup2index_with_null(ib, ie, dataValue, classBoundsPtr);
		if (IsDefined(index))
			return Range_GetValue_naked(resRange, index);
		return UNDEFINED_OR_ZERO(E);
	}

	template <typename RI, typename TI, typename RangeType>
	TI binarySearch2value_range(RI rb, RI re, TI dataPtr, TI classBoundsPtr, TI classBoundsEnd, RangeType resRange)
	{
		SizeT n = re - rb;
		using result_type = typename std::iterator_traits<RI>::value_type;
		using key_type = typename std::iterator_traits<TI>::value_type;
		parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
			{
				rb[i] = binarySearch2value<result_type>(dataPtr[i], classBoundsPtr, classBoundsEnd, resRange);
			}
		);
		return dataPtr + n;
	}

	template <typename RI, typename TI, typename RangeType>
	TI binarySearch2value_range_with_null(RI rb, RI re, TI dataPtr, TI classBoundsPtr, TI classBoundsEnd, RangeType resRange)
	{
		SizeT n = re - rb;
		using result_type = typename std::iterator_traits<RI>::value_type;
		using key_type = typename std::iterator_traits<TI>::value_type;
		parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
			{
				rb[i] = binarySearch2value_with_null<result_type>(dataPtr[i], classBoundsPtr, classBoundsEnd, resRange);
			}
		);
		return dataPtr + n;
	}

	template <typename RI, typename II, typename TI, typename RangeType>
	TI rlookup2value_range(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange)
	{
		SizeT n = re - rb;
		using result_type = typename std::iterator_traits<RI>::value_type;
		using key_type = typename std::iterator_traits<TI>::value_type;
		parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
			{
				rb[i] = rlookup2value<result_type>(ib, ie, dataPtr[i], classBoundsPtr, resRange);
			}
		);
		return dataPtr + n;
	}

	template <typename RI, typename II, typename TI, typename RangeType>
	TI rlookup2value_range_with_null(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange)
	{
		SizeT n = re - rb;
		using result_type = typename std::iterator_traits<RI>::value_type;
		using key_type = typename std::iterator_traits<TI>::value_type;
		parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
			{
				rb[i] = rlookup2value_with_null<result_type>(ib, ie, dataPtr[i], classBoundsPtr, resRange);
			}
		);
		return dataPtr + n;
	}


	template <typename Vec, typename KeyArrayView>
	void rlookup2index_array_non_null_values(Vec& resData, const KeyArrayView& arg1Data, /* E1->V */ const KeyArrayView& arg2Data  /* E2->V */ )
	{
		assert(resData.size() == arg1Data.size());

		auto a2Db = arg2Data.begin();

		// First, make a index mapping E2->E2  for arg2.
		std::vector<UInt32> index;

		// Sort index 
		make_index_non_null_values(index, arg2Data.size(), a2Db);
		auto indexBegin = index.begin(), indexEnd = index.end();

		// Then, do a binary search of each element 
		auto a1De = rlookup2index_range(resData.begin(), resData.end(), indexBegin, indexEnd, arg1Data.begin(), a2Db);

		dms_assert(a1De == arg1Data.end());
	}

	template <typename I, typename V>
	using indexed_tile_t = std::pair<std::vector<I>, typename DataArray<V>::locked_cseq_t>;

	template <typename I, typename V>
	auto make_index_array_skip_null(typename DataArray<V>::locked_cseq_t arg2Data) -> indexed_tile_t<I, V>
	{
		// make a mapping V->E2  from arg2. Assign in backward order to keep the first occurence
		std::vector<I> index;
		make_index_skip_null(index, arg2Data.size(), arg2Data.begin());
		return { std::move(index), std::move(arg2Data) };
	}

	template <typename I, typename V>
	auto make_index_array_all_values(typename DataArray<V>::locked_cseq_t arg2Data) -> indexed_tile_t<I, V>
	{
		// make a mapping V->E2  from arg2. Assign in backward order to keep the first occurence
		std::vector<I> index;
		make_index_all_values(index, arg2Data.size(), arg2Data.begin());
		return { std::move(index), std::move(arg2Data) };
	}

	// *****************************************************************************
	//                         helper funcs for ClassifyOperator
	// *****************************************************************************

	template <typename II, typename T, typename TI>
	auto classify2index(II ib, II ie, const T& dataValue, TI classBoundsPtr) -> std::iterator_traits<II>::value_type
	{
		if (IsBitValueOrDefined(dataValue))
		{
			II i = indexed_upperbound(ib, ie, classBoundsPtr, dataValue);
			if (i != ib)
			{
				// Check
				MGD_CHECKDATA(
					(i == ie || dataValue < classBoundsPtr[*i])
					&& (!(dataValue < classBoundsPtr[i[-1]]) || !IsBitValueOrDefined(classBoundsPtr[i[-1]]))
				);

				// Action
				return i[-1];
			}
		}
		return UNDEFINED_VALUE(std::iterator_traits<II>::value_type);
	}

	template <typename T, typename TI>
	SizeT upperBound2index(const T& dataValue, TI classBoundsPtr, TI classBoundsEnd)
	{
		if (IsBitValueOrDefined(dataValue))
		{
			TI i = upperbound(classBoundsPtr, classBoundsEnd, dataValue);
			if (i != classBoundsPtr)
			{
				// Check
				MGD_CHECKDATA(
					(i == classBoundsEnd || dataValue < *i)
					&& (!(dataValue < i[-1]) || !IsBitValueOrDefined(i[-1]))
				);

				// Action
				return i - classBoundsPtr - 1;
			}
		}
		return UNDEFINED_VALUE(SizeT);
	}

	template <typename II, typename T, typename TI, typename E>
	E classify2value(II ib, II ie, const T& dataValue, TI classBoundsPtr, const Range<E>& resRange)
	{
		auto index = classify2index(ib, ie, dataValue, classBoundsPtr);
		if (IsDefined(index))
			return Range_GetValue_naked(resRange, index);
		return UNDEFINED_VALUE(E);
	}

	template <typename T, typename TI, typename E>
	E upperBound2value(const T& dataValue, TI classBoundsPtr, TI classBoundsEnd, const Range<E>& resRange)
	{
		auto index = upperBound2index(dataValue, classBoundsPtr, classBoundsEnd);
		if (IsDefined(index))
			return Range_GetValue_naked(resRange, index);
		return UNDEFINED_VALUE(E);
	}


	template <typename RI, typename II, typename TI>
	TI classify2index_range(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr)
	{
		SizeT n = re - rb;
		using result_type = typename std::iterator_traits<RI>::value_type;
		parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
			{
				rb[i] = classify2index(ib, ie, dataPtr[i], classBoundsPtr);
			}
		);
		return dataPtr + n;
	}

	template <typename RI, typename TI, typename RangeType>
	TI upperBound2value_range(RI rb, RI re, TI dataPtr, TI classBoundsPtr, TI classBoundsEnd, RangeType resRange)
	{
		SizeT n = re - rb;
		using result_type = typename std::iterator_traits<RI>::value_type;
		parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
			{
				rb[i] = upperBound2value(dataPtr[i], classBoundsPtr, classBoundsEnd, resRange);
			}
		);
		return dataPtr + n;
	}

	template <typename RI, typename II, typename TI, typename RangeType>
	TI classify2value_range(RI rb, RI re, II ib, II ie, TI dataPtr, TI classBoundsPtr, RangeType resRange)
	{
		SizeT n = re - rb;
		using result_type = typename std::iterator_traits<RI>::value_type;
		parallel_for_if_separable<SizeT, result_type>(0, n, [=](SizeT i)
			{
				rb[i] = classify2value(ib, ie, dataPtr[i], classBoundsPtr, resRange);
			}
		);
		return dataPtr + n;
	}

	// **********************************************  Dispatchers
	// disp(resData, m_Arg1Data, indexedTilePtr->second, inviter->GetRange(), indexedTilePtr->first, m_MustCheck);

	struct rlookup_dispatcher {
		template <typename ResultIndexView, typename ValueArrayView>
		void applyIndexedSearch(ResultIndexView resData, ValueArrayView arg1Data, ValueArrayView arg2Data
		,	typename Unit<typename ResultIndexView::value_type>::range_t arg2DomainRange
		,	const std::vector<index_type_t<typename ResultIndexView::value_type>>& index
		)
		{
			assert(resData.size() == arg1Data.size());

			// do a binary search of each element 

			auto a2De = rlookup2value_range(resData.begin(), resData.end(), begin_ptr(index), end_ptr(index), arg1Data.begin(), arg2Data.begin(), arg2DomainRange);
			assert(a2De == arg1Data.end());
		}

		template <typename ResultIndexView, typename ValueArrayView>
		void applyBinarySearch(ResultIndexView resData, ValueArrayView arg1Data, ValueArrayView arg2Data
			, typename Unit<typename ResultIndexView::value_type>::range_t arg2DomainRange
		)
		{
			assert(resData.size() == arg1Data.size());

			// do a binary search of each element 

			auto a2De = binarySearch2value_range(resData.begin(), resData.end(), arg1Data.begin(), arg2Data.begin(), arg2Data.end(), arg2DomainRange);
			assert(a2De == arg1Data.end());
		}
	};

	struct rlookup_with_null_dispatcher {
		template <typename ResultIndexView, typename ValueArrayView>
		void applyIndexedSearch(ResultIndexView resData, ValueArrayView arg1Data, ValueArrayView arg2Data
			, typename Unit<typename ResultIndexView::value_type>::range_t arg2DomainRange
			, const std::vector<index_type_t<typename ResultIndexView::value_type>>& index
		)
		{
			assert(resData.size() == arg1Data.size());

			// do a binary search of each element 

			auto a2De = rlookup2value_range_with_null(resData.begin(), resData.end(), begin_ptr(index), end_ptr(index), arg1Data.begin(), arg2Data.begin(), arg2DomainRange);
			assert(a2De == arg1Data.end());
		}
		template <typename ResultIndexView, typename ValueArrayView>
		void applyBinarySearch(ResultIndexView resData, ValueArrayView arg1Data, ValueArrayView arg2Data
			, typename Unit<typename ResultIndexView::value_type>::range_t arg2DomainRange
		)
		{
			assert(resData.size() == arg1Data.size());

			// do a binary search of each element 

			auto a2De = binarySearch2value_range_with_null(resData.begin(), resData.end(), arg1Data.begin(), arg2Data.begin(), arg2Data.end(), arg2DomainRange);
			assert(a2De == arg1Data.end());
		}
	};

	struct classify_dispatcher {
		template <typename ResultIndexView, typename ValueArrayView>
		void applyIndexedSearch(ResultIndexView resData, ValueArrayView arg1Data, ValueArrayView arg2Data
			, typename Unit<typename ResultIndexView::value_type>::range_t arg2DomainRange
			, const std::vector<index_type_t<typename ResultIndexView::value_type>>& index
		)
		{
			assert(resData.size() == arg1Data.size());

			// do a binary search of each element 
			auto a2De = classify2value_range(resData.begin(), resData.end(), begin_ptr(index), end_ptr(index), arg1Data.begin(), arg2Data.begin(), arg2DomainRange);
			assert(a2De == arg1Data.end());
		}
		template <typename ResultIndexView, typename ValueArrayView>
		void applyBinarySearch(ResultIndexView resData, ValueArrayView arg1Data, ValueArrayView arg2Data
			, typename Unit<typename ResultIndexView::value_type>::range_t arg2DomainRange
		)
			// requires std::is_same_v< Unit<Vec::value_type>::range_t, Range<E> >
			// requires std::is_same_v< TArray1::value_type, TArray2::value_type >
		{
			assert(resData.size() == arg1Data.size());

			// do a binary search of each element 

			auto a2De = upperBound2value_range(resData.begin(), resData.end(), arg1Data.begin(), arg2Data.begin(), arg2Data.end(), arg2DomainRange);
			assert(a2De == arg1Data.end());
		}
	};
} // end anonymous namespace

#endif // !defined(__CLC_RLOOKUP_H)

