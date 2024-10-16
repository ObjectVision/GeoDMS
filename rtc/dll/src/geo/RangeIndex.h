// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_GEO_RANGEINDEX_H)
#define __RTC_GEO_RANGEINDEX_H

#include "ser/RangeStream.h"
#include "ser/AsString.h"

#if defined(MG_DEBUG)
inline bool AssertIsOrdinalType(const ord_type_tag*) { return true; }
#endif

template <class V>
inline SizeT Range_GetIndex_naked(const Range<V>& range, V loc)
{
	static_assert(is_integral_v<V>);

	assert(IsDefined(range));           // caller must provide a definedrange
	assert(IsIncluding(range, loc));    // caller must shield out-of-range

	assert(loc >= range.first);
	unsigned_type_t<V> offset = loc - range.first;
	SizeT index = offset;   
	assert(index < Cardinality(range)); // Postcondition
	return index;
}

template <class V>
inline SizeT Range_GetIndex_naked_zbase(V upperBound, V loc)
{
	static_assert(is_integral_v<V>);

	assert(IsDefined(upperBound));           // caller must provide a definedrange
	assert(loc < upperBound);    // caller must shield out-of-range

	SizeT index = loc;
	return index;
}

template <typename V>
inline auto Range_GetValue_naked_zbase(V upperBound, SizeT index) -> V
{
	assert(index < upperBound);
	assert(index >= 0);
	return index;
}

template <class V>
inline V Range_GetValue_naked(const Range<V>& range, SizeT index)
{
	static_assert(is_integral_v<V>);

	assert(IsDefined(range));        // caller must provide a definedrange
	assert(index < Cardinality(range)); // caller must shield out of range

	return range.first + index;
}

//	(full) specialization for Void
template <>
inline SizeT Range_GetIndex_naked<Void>(const Range<Void>&, Void)
{
	return  0;
}

template <>
inline Void Range_GetValue_naked(const Range<Void>&, SizeT i)
{
	assert(i==0);
	return Void();
}

// overloading for Bool
template <bit_size_t N>
inline SizeT Range_GetIndex_naked(const Range<UInt32>& range, bit_value<N> loc)
{
	assert(range.first == 0);
	assert(range.second== bit_value<N>::nr_values);

	return  loc;
}

template <bit_size_t N, typename Block>
inline SizeT Range_GetIndex_naked(const Range<UInt32>& range, bit_reference<N, Block> loc)
{
	assert(range.first == 0);
	assert(range.second== bit_value<N>::nr_values);

	return bit_value<N>(loc);
}

template <typename T>
inline SizeT Range_GetIndex_naked(const Range<Point<T> >& range, Point<T>loc)
{
	assert(IsDefined(loc));

	assert(IsDefined(range));        // caller must shield undefined values
	assert(IsIncluding(range, loc)); // caller must provide loc in range

	SizeT rowSize  = Width(range);
	loc.Row() -= Top(range);  SizeT rowIndex = loc.Row();
	loc.Col() -= Left(range); SizeT colIndex = loc.Col();

	assert(colIndex < rowSize);
	SizeT index =  rowIndex * rowSize + colIndex;
	assert(index < Cardinality(range)); // Postcondition

	return index;
}

template <typename T>
inline SizeT Range_GetIndex_naked_zbase(Point<T> upperBound, Point<T> loc)
{
	assert(IsDefined(loc));

	assert(IsDefined(upperBound));        // caller must shield undefined values
	assert(IsStrictlyLower(loc, upperBound)); // caller must provide loc in range

	SizeT rowSize  = upperBound.Col();
	SizeT rowIndex = loc.Row();
	SizeT colIndex = loc.Col();

	SizeT index = rowIndex * rowSize + colIndex;
	return index;
}

template <typename T>
inline auto Range_GetValue_naked_zbase(Point<T> upperBound, SizeT index) -> Point<T>
{
	SizeT rowSize = upperBound.Col();
	auto loc = shp2dms_order(index % rowSize, index / rowSize);
	assert(IsDefined(loc));
	assert(loc.Col() >= 0);
	assert(loc.Col() < upperBound.Col());
	assert(loc.Row() >= 0);
	assert(loc.Row() < upperBound.Row());

	return loc;
}

template <typename T>
inline Point<T> Range_GetValue_naked(const Range<Point<T> >& range, SizeT index)
{
	assert(IsDefined(range));        // callers must shield undefined values
	assert(index < Cardinality(range)); // caller must shield out of range

	UInt32 rowSize = Width(range);
	assert(rowSize != 0);

	return rowcol2dms_order<T>(Top (range) + index / rowSize, Left(range) + index % rowSize);
}

template <typename R, typename V>
inline typename std::enable_if_t<has_undefines_v<V>, SizeT >
Range_GetIndex_checked(const R& range, V loc)
{
	if (!IsIncluding(range, loc) || !IsDefined(loc)) 
		return UNDEFINED_VALUE(SizeT);
	return Range_GetIndex_naked(range, loc);
}

template <typename R, typename V>
inline typename std::enable_if_t<not has_undefines_v<V>, SizeT >
Range_GetIndex_checked(const R& range, V loc)
{
	return Range_GetIndex_naked(range, loc);
}

template <class V>
inline std::enable_if_t<has_undefines_v<V>, V>
Range_GetValue_checked(const Range<V>& range, SizeT index)
{
	if (index >= Cardinality(range)) 
		return UNDEFINED_VALUE(V);

	return Range_GetValue_naked(range, index);
}

template <class V>
inline typename std::enable_if_t<not has_undefines_v<V>, V>
Range_GetValue_checked(const Range<V>& range, SizeT index)
{
	return Range_GetValue_naked(range, index);
}

namespace range_impl 
{
	template <typename MutIter, typename T>
	void increment_Inplace(MutIter first, MutIter last, T inc)
	{
		for (; first != last; ++first) 
			*first += inc;
	}

	template <typename InIter, typename OutIter, typename T>
	void increment(InIter first, InIter last, OutIter outIter, T inc)
	{
		for (; first != last; ++outIter, ++first) 
			*outIter = *first + inc;
	}

	template <typename MutIter, typename T>
	void decrement_Inplace(MutIter first, MutIter last, T dec)
	{
		for (; first != last; ++first) 
			*first -= dec;
	}

	template <typename InIter, typename OutIter, typename T>
	void decrement(InIter first, InIter last, OutIter outIter, T inc)
	{
		for (; first != last; ++outIter, ++first) 
			*outIter = *first - inc;
	}

	// =============== Range_Index2Value_Inplace_naked

	template <typename MutIter, typename T>
	inline void Range_Index2Value_Inplace_naked(const Range<T>& range, MutIter first, MutIter last, const ord_type_tag*)
	{
		T inc = range.first;
		if (inc)
			increment_Inplace(first, last, inc);
	}

	//	specialization for Bool (function overloading)
	template <int N, typename B>
	inline void Range_Index2Value_Inplace_naked(const Range<UInt32>& range, bit_iterator<N, B> first, bit_iterator<N, B> last, const bool_type_tag*)
	{
		assert(range.first == 0);
		//	NOP
	}

	// =============== Range_Index2Value_Inplace_checked

	template <typename MutIter, typename T>
	inline void Range_Index2Value_Inplace_checked(Range<T> range, MutIter first, MutIter last, const ord_type_tag*)
	{
		assert(range.first <= range.second);
		if (range.first)
		{
			range.second -= range.first;
			assert(range.second >= 0);
			for (; first != last; ++first)
				if (SizeT(*first) < SizeT(range.second))
					*first += range.first;
				else
					MakeUndefined(*first);
		}
		else
		{
			assert(range.second >= 0);
			for (; first != last; ++first)
				if (SizeT(*first) >= SizeT(range.second))
					MakeUndefined(*first);
		}
	}

	//	specialization for Bool (function overloading)
	template <int N, typename B>
	inline void Range_Index2Value_Inplace_checked(const Range<UInt32>& range, bit_iterator<N, B> first, bit_iterator<N, B> last, const bool_type_tag*)
	{
		assert(range.first == 0);
		//	NOP
	}

	// =============== Range_Index2Value_naked

	template <typename InIter, typename OutIter, typename T>
	inline void Range_Index2Value_naked(const Range<T>& range, InIter first, InIter last, OutIter outIter, const ord_type_tag*)
	{
		if (range.first)
			increment(first, last, outIter, range.first);
		else
			fast_copy(first, last, outIter);
	}

	//	specialization for Bool (function overloading)
	template <typename InIter, int N, typename B>
	inline void Range_Index2Value_naked(const Range<UInt32>& range,InIter first, InIter last, bit_iterator<N, B> outIter, const bool_type_tag*)
	{
		assert(range.first == 0);
		fast_copy(first, last, outIter);
	}

	template <typename InIter, typename OutIter, typename T>
	inline void Range_Index2Value_naked(Range<T> range, InIter first, InIter last, OutIter outIter, const crd_point_type_tag*)
	{
		for (; first != last; ++outIter, ++first)
			*outIter = Range_GetValue_naked(range, *first);
	}

	// =============== Range_Index2Value_checked

	template <typename InIter, typename OutIter, typename T>
	inline void Range_Index2Value_checked(const Range<T>& range, InIter first, InIter last, OutIter outIter, const ord_type_tag*)
	{
		assert(range.first <= range.second);
		T                      inc = range.first;
		typename unsigned_type<T>::type sz = range.second - inc;

		if (range.first)
			for (; first != last; ++outIter, ++first)
				if (*first < sz)
					*outIter = *first + range.first;
				else
					MakeUndefined(*outIter);
		else
			for (; first != last; ++outIter, ++first)
				if (*first < sz)
					*outIter = *first;
				else
					MakeUndefined(*outIter);
	}

	//	specialization for Bool (function overloading)
	template <typename InIter, int N, typename B>
	inline void Range_Index2Value_checked(const Range<UInt32>& range, InIter first, InIter last, bit_iterator<N, B> outIter, const bool_type_tag* boolTypeTag)
	{
		return Range_Index2Value_naked(range, first, last, outIter, boolTypeTag);
	}

	template <typename InIter, typename OutIter, typename T>
	inline void Range_Index2Value_checked(Range<T> range, InIter first, InIter last, OutIter outIter, const crd_point_type_tag*)
	{
		for (; first != last; ++outIter, ++first)
			*outIter = Range_GetValue_checked(range, *first);
	}

	// =============== Range_Value2Index_Inplace_naked

	template <typename MutIter, typename T>
	inline void Range_Value2Index_Inplace_naked(const Range<T>& range, MutIter first, MutIter last, const ord_type_tag*)
	{
		if (range.first)
			decrement_Inplace(first, last, range.first);
	}

	template <int N, typename B>
	inline void Range_Value2Index_Inplace_naked(const Range<UInt32>& range, bit_iterator<N, B> first, bit_iterator<N, B> last, const bool_type_tag*)
	{
		assert(range.first == 0);
		// NOP
	}

	// =============== Range_Value2Index_naked

	template <typename InIter, typename OutIter, typename T>
	inline void Range_Value2Index_naked(const Range<T>& range, InIter first, InIter last, OutIter outIter, const ord_type_tag*)
	{
		if (range.first)
			decrement(first, last, outIter, range.first);
		else
			fast_copy(first, last, outIter);
	}

	template <int N, typename C, typename OutIter>
	inline void Range_Value2Index_naked(const Range<UInt32>& range, bit_iterator<N, C> first, bit_iterator<N, C> last, OutIter outIter, const bool_type_tag*)
	{
		assert(range.first == 0);
		fast_copy(first, last, outIter);
	}

	template <typename InIter, typename OutIter, typename T>
	inline void Range_Value2Index_naked(Range<T> range, InIter first, InIter last, OutIter outIter, const crd_point_type_tag*)
	{
		for (; first != last; ++outIter, ++first)
			*outIter = Range_GetIndex_naked(range, *first);
	}

	// =============== Range_Value2Index_checked

	template <typename InIter, typename OutIter, typename T>
	inline void Range_Value2Index_checked(const Range<T>& range, InIter first, InIter last, OutIter outIter, const ord_type_tag*)
	{
		bool checkUndefined = IsIncluding(range, UNDEFINED_VALUE(T));
		T inc = range.first;
		if (inc)
			if (checkUndefined)
				for (; first != last; ++outIter, ++first)
					if (IsDefined(*first) && IsIncluding(range, *first))
						*outIter = *first - inc;
					else
						MakeUndefined(*outIter);
			else
				for (; first != last; ++outIter, ++first)
					if (IsIncluding(range, *first))
						*outIter = *first - inc;
					else
						MakeUndefined(*outIter);
		else
			if (checkUndefined)
				for (; first != last; ++outIter, ++first)
					if (IsDefined(*first) && IsIncluding(range, *first))
						*outIter = *first;
					else
						MakeUndefined(*outIter);
			else
				for (; first != last; ++outIter, ++first)
					if (IsIncluding(range, *first))
						*outIter = *first;
					else
						MakeUndefined(*outIter);
	}

	template <int N, typename C, typename OutIter>
	inline void Range_Value2Index_checked(const Range<UInt32>& range, bit_iterator<N, C> first, bit_iterator<N, C> last, OutIter outIter, const bool_type_tag*)
	{
		Range_Value2Index_naked(range, first, last, outIter);
	}

	template <typename InIter, typename OutIter, typename T>
	inline void Range_Value2Index_checked(Range<T> range, InIter first, InIter last, OutIter outIter, const crd_point_type_tag*)
	{
		for (; first != last; ++outIter, ++first)
			*outIter = Range_GetIndex_checked(range, *first);
	}
};

template <typename MutIter, typename E>
inline void Range_Index2Value_Inplace_naked(const Range<E>& range, MutIter first, MutIter last)
{
	typedef typename std::iterator_traits<MutIter>::value_type value_type;
	range_impl::Range_Index2Value_Inplace_naked(range, first, last, TYPEID(elem_traits<value_type>));
}

template <typename InIter, typename OutIter, typename E>
inline void Range_Index2Value_naked(const Range<E>& range, InIter first, InIter last, OutIter outIter)
{
	typedef typename std::iterator_traits<OutIter>::value_type value_type;
	range_impl::Range_Index2Value_naked(range, first, last, outIter, TYPEID(elem_traits<value_type>));
}


template <typename MutIter, typename E>
inline void Range_Value2Index_Inplace_naked(const Range<E>& range, MutIter first, MutIter last)
{
	typedef typename std::iterator_traits<MutIter>::value_type value_type;
	range_impl::Range_Value2Index_Inplace_naked(range, first, last, TYPEID(elem_traits<value_type>));
}

template <typename InIter, typename OutIter, typename E>
inline void Range_Value2Index_naked(const Range<E>& range, InIter first, InIter last, OutIter outIter)
{
	typedef typename std::iterator_traits<InIter>::value_type value_type;
	range_impl::Range_Value2Index_naked(range, first, last, outIter, TYPEID(elem_traits<value_type>));
}

template <typename InIter, typename OutIter, typename E>
inline void Range_Value2Index_checked(const Range<E>& range, InIter first, InIter last, OutIter outIter)
{
	typedef typename std::iterator_traits<InIter>::value_type value_type;
	range_impl::Range_Value2Index_checked(range, first, last, outIter, TYPEID(elem_traits<value_type>));
}

template <typename InIter, typename OutIter, typename E>
inline void Range_Index2Value_checked(const Range<E>& range, InIter first, InIter last, OutIter outIter)
{
	typedef typename std::iterator_traits<OutIter>::value_type value_type;
	range_impl::Range_Index2Value_checked(range, first, last, outIter, TYPEID(elem_traits<value_type>));
}

template <typename MutIter, typename E>
inline void Range_Index2Value_Inplace_checked(const Range<E>& range, MutIter first, MutIter last)
{
	typedef typename std::iterator_traits<MutIter>::value_type value_type;
	range_impl::Range_Index2Value_Inplace_checked(range, first, last, TYPEID(elem_traits<value_type>));
}


#endif // __RTC_GEO_RANGEINDEX_H
