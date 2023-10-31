// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_SET_RANGEFUNCS_H)
#define __RTC_SET_RANGEFUNCS_H

#include "dbg/DebugCast.h"
#include "geo/ElemTraits.h"
#include "geo/IterTraits.h"
//REMOVE #include "geo/SequenceTraits.h"
#include "utl/swap.h"

#define MG_DEBUG_RANGEFUNCS

template <typename T>
bool IsAligned2(T* ptr)
{
	return !(reinterpret_cast<SizeT>(ptr) & 0x01); //DWORD ALINGMENT
}

template <typename T>
bool IsAligned4(T* ptr)
{
	return !(reinterpret_cast<SizeT>(ptr) & 0x03); //DWORD ALINGMENT
}

template <int N, typename Block>
bool IsAligned4(bit_iterator<N, Block> bitPtr)
{
	return bitPtr.nr_elem() == 0
		&& IsAligned4(bitPtr.data_begin());
}

//----------------------------------------------------------------------
// Section      : bit functions
//----------------------------------------------------------------------

template <typename Block>
void setbits(Block& lhs, Block mask, Block values)
{
	(lhs &= ~mask) |= (values & mask);
}

//----------------------------------------------------------------------
// Section      : type traits
//----------------------------------------------------------------------

template <typename T>
struct raw_constructed : std::is_trivially_constructible<T> {};

template <typename T, typename U> struct raw_constructed<Pair<T, U>> : std::bool_constant<raw_constructed<T>::value&& raw_constructed<U>::value> {};
template <bit_size_t N> struct raw_constructed<bit_value<N>> : std::true_type {};
template <typename T> struct raw_constructed<Couple<T> > : raw_constructed<T> {};
template <typename T> struct raw_constructed<Point<T> > : raw_constructed<T> {};
template <typename T> struct raw_constructed<Range<T> > : raw_constructed<T> {};

template <typename T>
constexpr bool raw_constructed_v = raw_constructed<T>::value;

template <typename T> struct raw_destructed : std::is_trivially_destructible<T> {};

template <bit_size_t N> struct raw_destructed<bit_value<N> > : std::true_type {};
template <typename T, typename U> struct raw_destructed<Pair<T, U>> : std::bool_constant<raw_destructed<T>::value&& raw_destructed<U>::value> {};
template <typename T> struct raw_destructed<Couple<T> > : raw_destructed<T> {};
template <typename T> struct raw_destructed<Point<T> > : raw_destructed<T> {};
template <typename T> struct raw_destructed<Range<T> > : raw_destructed<T> {};

template <typename T>
struct trivially_copyable : std::is_trivially_copyable<T> {};
template <bit_size_t N> struct trivially_copyable<bit_value<N> > : std::true_type {};
template <typename T, typename U> struct trivially_copyable<Pair<T, U>> : std::bool_constant<trivially_copyable<T>::value&& trivially_copyable<U>::value> {};
template <typename T> struct trivially_copyable<Couple<T> > : trivially_copyable<T> {};
template <typename T> struct trivially_copyable<Point<T> > : trivially_copyable<T> {};
template <typename T> struct trivially_copyable<Range<T> > : trivially_copyable<T> {};

template <typename T>
struct raw_copyable : std::bool_constant<trivially_copyable<T>::value&& raw_constructed<T>::value> {};

template <typename T>
struct trivially_move_assignable : std::is_trivially_move_assignable<T> {};
template <bit_size_t N> struct trivially_move_assignable<bit_value<N> > : std::true_type {};
template <typename T, typename U> struct trivially_move_assignable<Pair<T, U>> : std::bool_constant<trivially_move_assignable<T>::value&& trivially_move_assignable<U>::value> {};
template <typename T> struct trivially_move_assignable<Couple<T> > : trivially_move_assignable<T> {};
template <typename T> struct trivially_move_assignable<Point<T> > : trivially_move_assignable<T> {};
template <typename T> struct trivially_move_assignable<Range<T> > : trivially_move_assignable<T> {};

//----------------------------------------------------------------------
// Section      : destroy_range
//----------------------------------------------------------------------

template <typename Iter> inline
typename std::enable_if< raw_destructed< typename std::iterator_traits<Iter>::value_type >::value >::type
destroy_range(Iter /*first*/, Iter /*last*/)
{
	// NOP
}

template <typename Iter> inline
typename std::enable_if< !raw_destructed< typename std::iterator_traits<Iter>::value_type >::value >::type
destroy_range(Iter first, Iter last)
{
	for (; first != last; ++first)
		std::destroy_at(first);
}


//----------------------------------------------------------------------
// Section      : swap range[_backward]
//----------------------------------------------------------------------

template <typename Iter1, typename Iter2>
Iter2 swap_range(Iter1 first, Iter1 last, Iter2 otherRange)
{
	for (; first != last; ++first, ++otherRange)
		omni::swap(*otherRange, *first);
	return otherRange;
}

template <typename Iter1, typename Iter2>
Iter2 swap_range_backward(Iter1 first, Iter1 last, Iter2 otherRange)
{
	while (first != last)
		omni::swap(*--last, *--otherRange);
	return otherRange;
}


//----------------------------------------------------------------------
// Section      : fast_<copy|move>[_backward]
//----------------------------------------------------------------------


#if defined(MG_DEBUG_RANGEFUNCS)

template <typename Iter>
struct is_random_iter : std::bool_constant<
	std::is_reference_v<ref_type_of_iterator<Iter> >
	&& std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>
>
{};

#endif

template <typename Iter, typename CIter> inline
Iter fast_copy(CIter first, CIter last, Iter target)
{
	return std::copy(first, last, target);
}

template <typename Iter>
inline auto fast_move(Iter first, Iter last, Iter target)
-> typename std::enable_if_t < !std::is_trivially_copy_assignable_v<typename std::iterator_traits<Iter>::value_type>, Iter>
{

#if defined(MG_DEBUG_RANGEFUNCS)
	using T = std::iterator_traits< Iter>::value_type;

	static_assert(!is_bitvalue_v<T>);
	static_assert(!std::is_trivially_copy_assignable_v< T >);
#endif

	//	dms_assert(!(first < target)|| (last <= target) ); // BEWARE OF OVERLAPPING RANGES

	for (; first != last; ++first, ++target)
		*target = std::move(*first);
	return target;
}

template <typename Iter, typename CIter> inline
Iter
fast_copy_backward(CIter first, CIter last, Iter target)
{

#if defined(MG_DEBUG_RANGEFUNCS)
	static_assert(!is_bitvalue_v<typename std::iterator_traits<CIter>::value_type>);
	static_assert(!is_bitvalue_v<typename std::iterator_traits< Iter>::value_type>);
#endif

	//	dms_assert(!(first < target)|| (last <= target) ); // BEWARE OF OVERLAPPING RANGES, BUT PROBLEM WITH INCOMPATIBLE ITERATORS

	return std::copy_backward(first, last, target);
}

template <typename Iter> inline
Iter
fast_move_backward(Iter first, Iter last, Iter targetEnd)
{

#if defined(MG_DEBUG_RANGEFUNCS)
	typedef typename std::iterator_traits< Iter>::value_type  T;

	static_assert(!is_bitvalue_v< T>);
	static_assert(!std::is_trivially_move_assignable_v< T, T>);
#endif

	dms_assert((targetEnd <= first) || (last <= targetEnd)); // BEWARE OF OVERLAPPING RANGES

	while (first != last)
		*--targetEnd = std::move(*--last);
	return targetEnd;
}

template <typename T> inline
typename std::enable_if<std::is_trivially_copy_assignable_v<T>, T*>::type
fast_copy(const T* first, const T* last, T* target)
{
	dms_assert(!(first < target) || (last <= target)); // BEWARE OF OVERLAPPING RANGES

	std::size_t n = last - first;
	memcpy(target, first, n * sizeof(T));
	return target + n;
}


template <typename Iter>
inline auto fast_move(Iter first, Iter last, Iter target)
-> typename std::enable_if_t < std::is_trivially_copy_assignable_v<typename std::iterator_traits<Iter>::value_type>, Iter>
{
	return fast_copy(first, last, target);
}


template <typename T> inline T*
fast_copy_backward_trivial_impl(const T* first, const T* last, T* targetEnd)
{
	dms_assert((targetEnd <= first) || (last <= targetEnd)); // BEWARE OF OVERLAPPING RANGES

	std::size_t n = last - first;
	T* target = targetEnd - n;
	memmove(target, first, n * sizeof(T));
	return target;
}

template <typename T> inline
typename std::enable_if<std::is_trivially_assignable_v<T, T>, T*>::type
fast_copy_backward(const T* first, const T* last, T* targetEnd)
{
	return fast_copy_backward_trivial_impl(first, last, targetEnd);
}

template <typename T> inline
typename std::enable_if<std::is_trivially_assignable_v<T, T>, T*>::type
fast_move_backward(T* first, T* last, T* target)
{
	return fast_copy_backward_trivial_impl(const_cast<const T*>(first), const_cast<const T*>(last), target);
}

template <bit_size_t N, typename CB, typename B> inline
typename std::enable_if<std::is_same_v<B, typename std::remove_const<CB>::type >, bit_iterator<N, B> >::type
fast_copy(bit_iterator<N, CB> first, bit_iterator<N, CB> last, bit_iterator<N, B> target)
{
	dms_assert(!(first < target) || (last <= target)); // BEWARE OF OVERLAPPING RANGES

	for (; first.nr_elem(); ++target, ++first)
	{
		if (first == last)
			return target;
		*target = *first;
	}
	dms_assert(first.nr_elem() == 0);

	CB* currSourceBlock = first.data_begin();
	CB* lastSourceBlock = last.data_begin();
	B* currTargetBlock = target.data_begin();

	if (target.nr_elem() == 0)
	{
		// first and target are in phase; fast_copy the full block data
		currTargetBlock = fast_copy(currSourceBlock, lastSourceBlock, currTargetBlock);

		// copy the remaining bits of the incomplete last block; phase equality guarantees that this doesnt cross a block boundary in the target range
		if (last.nr_elem())
			setbits(*currTargetBlock, (B(1) << (last.nr_elem() * N)) - B(1), *lastSourceBlock);
		return bit_iterator<N, B>(currTargetBlock, last.nr_elem());
	}
	else
	{
		// example where target.nr_elem() == 3:
		// first:          01234567 89ABCDEF 01234567 89ABCDEF 
		// target(3):      DEF01234 56789ABC DEF01234 56789ABC
		// bitShiftMask:   11100000 00000000 00000000 00000000 bitShift   = 3
		// carryShiftMask: 00011111 11111111 11111111 11111111 carryShift = 13

		// first and target are out of phase; copy and shift block data
		UInt32 elemShift = target.nr_elem();
		UInt32 bitShift = elemShift * N;                              dms_assert((bitShift < bit_info<N, B>::nr_used_bits_per_block));
		UInt32 carryShift = bit_info<N, B>::nr_used_bits_per_block - bitShift; dms_assert((carryShift < bit_info<N, B>::nr_used_bits_per_block));
		B      bitShiftMask = (B(1) << bitShift) - 1;
		B      carryShiftMask = bit_info<N, B>::used_bits_mask - bitShiftMask;

		if (currSourceBlock != lastSourceBlock)
		{
			dms_assert((*currSourceBlock & ~bit_info<N, B>::used_bits_mask) == 0);

			setbits(*currTargetBlock, carryShiftMask, (*currSourceBlock) << bitShift);

			B carry = (*currSourceBlock) >> carryShift;
			dms_assert(!(carry & ~bitShiftMask));
			++currTargetBlock;
			++currSourceBlock;
			for (; currSourceBlock != lastSourceBlock; ++currTargetBlock, ++currSourceBlock)
			{
				B currSrcValue = (*currSourceBlock);
				dms_assert((currSrcValue & ~bit_info<N, B>::used_bits_mask) == 0);
				B shiftedValue = (currSrcValue << bitShift) & bit_info<N, B>::used_bits_mask;
				dms_assert(!(shiftedValue & ~carryShiftMask));
				dms_assert(!(shiftedValue & carry));
				*currTargetBlock = shiftedValue | carry;
				dms_assert((*currTargetBlock & ~bit_info<N, B>::used_bits_mask) == 0);

				carry = currSrcValue >> carryShift;
				dms_assert(!(carry & ~bitShiftMask));
			}
			setbits(*currTargetBlock, bitShiftMask, carry);
		}
		// copy the remaining bits of the incomplete last block; this could cross a block boundary in the target range
		return std::copy(
			bit_iterator<N, CB>(currSourceBlock, SizeT(0)),
			last,
			bit_iterator<N, B>(currTargetBlock, SizeT(elemShift))
		);
	}
}

template <int N, typename B> inline
bit_iterator<N, B>
fast_move(bit_iterator<N, B> first, bit_iterator<N, B> last, bit_iterator<N, B> target)
{
	return fast_copy(first, last, target);
}

template <int N, typename CB, typename B> inline
typename std::enable_if<std::is_same_v<B, typename std::remove_const<CB>::type >, bit_iterator<N, B> >::type
fast_copy_backward(bit_iterator<N, CB> first, bit_iterator<N, CB> last, bit_iterator<N, B> target)
{
	dms_assert(!(first < target) || (last <= target)); // BEWARE OF OVERLAPPING RANGES

	for (; last.nr_elem(); --target, --last)
	{
		if (first == last)
			return target;
		*target = *last;
	}
	dms_assert(last.nr_elem() == 0);

	CB* firstSourceBlock = first.data_begin(); if (first.nr_elem()) ++firstSourceBlock;
	CB* lastSourceBlock = last.data_begin();
	B* currTargetBlock = target.data_begin();

	if (target.nr_elem() == 0)
	{
		// first and target are in phase; fast_copy the full block data
		currTargetBlock = fast_copy_backward(firstSourceBlock, lastSourceBlock, currTargetBlock);

		// copy the remaining bits of the incomplete last block; phase equality guarantees that this doesnt cross a block boundary in the target range
		if (first.nr_elem())
			setbits(*--currTargetBlock, bit_info<N, B>::used_bits_mask - ((B(1) << (first.nr_elem() * N)) - B(1)), *first.data_begin());
		return bit_iterator<N, B>(currTargetBlock, first.nr_elem());
	}
	else
	{
		// example where target.nr_elem() == 3:
		// last:           01234567 89ABCDEF 01234567 89ABCDEF 
		// target(3):      DEF01234 56789ABC DEF01234 56789ABC
		// bitShiftMask:   11100000 00000000 00000000 00000000 bitShift   = 3
		// carryShiftMask: 00011111 11111111 11111111 11111111 carryShift = 13


		// first and target are out of phase; copy and shift block data
		UInt32 elemShift = target.nr_elem();
		UInt32 bitShift = elemShift * N;                              dms_assert((bitShift < bit_info<N, B>::nr_used_bits_per_block));
		UInt32 carryShift = bit_info<N, B>::nr_used_bits_per_block - bitShift; dms_assert((carryShift < bit_info<N, B>::nr_used_bits_per_block));
		B      bitShiftMask = (B(1) << bitShift) - B(1);
		B      carryShiftMask = bit_info<N, B>::used_bits_mask - bitShiftMask;

		while (firstSourceBlock != lastSourceBlock)
		{
			--lastSourceBlock;
			dms_assert((*lastSourceBlock & ~bit_info<N, B>::used_bits_mask) == 0);

			setbits(*currTargetBlock, bitShiftMask, (*lastSourceBlock) >> carryShift);

			// OPTIMIZE: for all but the last incomplete targetBlock, a simple assignment would suffice
			setbits(*--currTargetBlock, carryShiftMask, (*lastSourceBlock) << bitShift);
		}
		// copy the remaining bits of the incomplete last block; this could cross a block boundary in the target range
		return std::copy_backward(
			first,
			bit_iterator<N, CB>(lastSourceBlock, SizeT(0)),
			bit_iterator<N, B>(currTargetBlock, SizeT(elemShift))
		);
	}
}

template <int N, typename B> inline
bit_iterator<N, B>
fast_move_backward(bit_iterator<N, B> first, bit_iterator<N, B> last, bit_iterator<N, B> target)
{
	return fast_copy_backward(first, last, target);
}

template <int N, typename CB, typename Iter> inline
Iter fast_copy(bit_iterator<N, CB> first, bit_iterator<N, CB> last, Iter target)
{
	for (; first != last; ++target, ++first)
		*target = typesafe_cast<bit_value<N>>(*first);
	return target;
}

template <int N, typename CB, typename Iter> inline
Iter fast_copy_backward(bit_iterator<N, CB> first, bit_iterator<N, CB> last, Iter targetEnd)
{
	while (first != last)
		*--last == typesafe_cast<bit_value<N>>(*--targetEnd);
	return targetEnd;
}

//----------------------------------------------------------------------
// Section      : raw_copy
//----------------------------------------------------------------------

template <typename Iter, typename CIter> inline
typename std::enable_if< raw_copyable< typename std::iterator_traits<Iter>::value_type >::value, Iter >::type
raw_copy(CIter first, CIter last, Iter target)
{
	return fast_copy(first, last, target);
}

template <typename Iter, typename CIter> inline
typename std::enable_if< !raw_copyable< typename std::iterator_traits<Iter>::value_type >::value, Iter >::type
raw_copy(CIter first, CIter last, Iter target)
{
	using T = std::iterator_traits<Iter>::value_type;

	for (; first != last; ++target, ++first)
		std::construct_at<T>(target, *first);

	return target;
}

//----------------------------------------------------------------------
// Section      : raw_move
//----------------------------------------------------------------------

template <typename T>
struct raw_movable : std::bool_constant< raw_copyable<T>::value&& raw_destructed<T>::value > {};

template <typename Iter, typename CIter> inline
Iter
raw_move_unchecked(CIter first, CIter last, Iter target)
{
	dms_assert(first <= last);
	Iter targetEnd = target + (last - first);
	dms_assert(target >= last || targetEnd <= first);
	Iter result = raw_copy(first, last, target);
	destroy_range(first, last);
	return result;
}

template <typename Iter, typename CIter> inline
typename boost::disable_if< raw_movable< typename std::iterator_traits<Iter>::value_type >, Iter >::type
raw_move(CIter first, CIter last, Iter target)
{
	dms_assert(first <= last);
	dms_assert(target >= last || target <= first);
	Iter targetEnd = target + (last - first);
	while (target < first && targetEnd >= first) // end of targetrange crosses first
	{
		CIter mid = first + (first - target);
		target = raw_move_unchecked(first, mid, target);
		first = mid;
		dms_assert(targetEnd == target + (last - first));
	}
	return raw_move_unchecked(first, last, target);
}

template <typename Iter, typename CIter> inline
typename std::enable_if< raw_movable< typename std::iterator_traits<Iter>::value_type >::value, Iter >::type
raw_move(CIter first, CIter last, Iter target)
{
	return raw_copy(first, last, target);
	//	UInt32 n = (last - first);
	//	memmove(target, first, n * sizeof(typename std::iterator_traits<Iter>::value_type));
	//	return target + n;
}

template <typename Iter, typename CIter> inline
Iter raw_move_backward_unchecked(CIter first, CIter last, Iter targetEnd)
{
	dms_assert(first <= last);
	Iter target = targetEnd - (last - first);
	raw_move_unchecked(first, last, target);
	return target;
}

template <typename Iter, typename CIter> inline
typename boost::disable_if< raw_movable< typename std::iterator_traits<Iter>::value_type >, Iter >::type
raw_move_backward(CIter first, CIter last, Iter targetEnd)
{
	dms_assert(first <= last);
	dms_assert(targetEnd >= last || targetEnd <= first);
	while (targetEnd > last && targetEnd - (last - first) < last) // end of targetrange crosses last
	{
		CIter mid = last - (targetEnd - last);
		targetEnd = raw_move_backward_unchecked(mid, last, targetEnd);
		last = mid;
	}
	return raw_move_backward_unchecked(first, last, targetEnd);
}

template <typename Iter, typename CIter> inline
typename std::enable_if< raw_movable< typename std::iterator_traits<Iter>::value_type >::value, Iter >::type
raw_move_backward(CIter first, CIter last, Iter targetEnd)
{
	return fast_move_backward(first, last, targetEnd);
	//	UInt32 n = (last - first);
	//	Iter target = targetEnd - n;
	//	memmove(target, first, n * sizeof(typename std::iterator_traits<Iter>::value_type));
	//	return target;
}


//----------------------------------------------------------------------
// Section      : fast_fill
//----------------------------------------------------------------------

template <typename Iter, typename U> inline
void
fast_fill(Iter first, Iter last, U value)
{

#if defined(MG_DEBUG_RANGEFUNCS)
	using T = typename std::iterator_traits< Iter>::value_type;
	static_assert(!is_bitvalue_v<U>);
	static_assert(!is_bitvalue_v<T>);

	static_assert(!(is_random_iter<Iter>::value && std::is_trivially_assignable_v<T, U> && (sizeof(T) < 4)));
#endif

	return std::fill(first, last, value);
}

template <typename T, typename U> inline
typename std::enable_if<std::is_trivially_assignable_v<T, U> && (sizeof(T) == 1)>::type
fast_fill(T* first, T* last, U value)
{
	static_assert(sizeof(T) == 1);
	memset(first, value, last - first);
}

template <typename T, typename U> inline
typename std::enable_if<std::is_trivially_assignable_v<T, U> && (sizeof(T) == 2)>::type
fast_fill(T* first, T* last, U value)
{
	static_assert(sizeof(T) == 2);
	dms_assert(IsAligned2(first));
	for (; !IsAligned4(first); ++first)
	{
		if (first == last)
			return;
		*first = value;
	}
	UInt32 n2 = (last - first) / 2;
	UInt32* alignedBlockPtr = reinterpret_cast<UInt32*>(first);
	UInt32* alignedBlockEnd = alignedBlockPtr + n2;

	dms_assert(IsAligned4(alignedBlockPtr));
	dms_assert(IsAligned4(alignedBlockEnd));

	UInt32 v = (UInt32(value) << 16) | (UInt32(value) & 0xFFFF);

	std::fill(alignedBlockPtr, alignedBlockEnd, v);
	first += 2 * n2;
	std::fill(first, last, value);
}

template <int N, typename B, typename U> inline
void fast_fill(bit_iterator<N, B> first, bit_iterator<N, B> last, U value)
{
	for (; first.nr_elem(); ++first)
	{
		if (first == last)
			return;
		*first = value;
	}
	dms_assert(first.nr_elem() == 0);

	bit_sequence<N, B>(
		first.data_begin(), last - first
		).set_range(value);
}

//----------------------------------------------------------------------
// Section      : raw_fill
//----------------------------------------------------------------------

template <typename Iter, typename U> inline
typename std::enable_if< raw_copyable< typename std::iterator_traits<Iter>::value_type >::value >::type
raw_fill(Iter first, Iter last, U value)
{
	fast_fill(first, last, value);
}

template <typename Iter, typename U> inline
typename std::enable_if< !raw_copyable< typename std::iterator_traits<Iter>::value_type >::value >::type
raw_fill(Iter first, Iter last, U value)
{
	using T = typename std::iterator_traits<Iter>::value_type;

	std::allocator<T> alloc;
	for (; first != last; ++first)
		alloc.construct(first, value);
}

//----------------------------------------------------------------------
// Section      : fast_zero
//	sets a range of values of type T to the default value T()
//	specialize for types that can be initialized with memset
//----------------------------------------------------------------------

template <typename Iter> inline
void
fast_zero(Iter first, Iter last)
{
	using T = typename std::iterator_traits< Iter>::value_type;

#if defined(MG_DEBUG_RANGEFUNCS)
	static_assert(!is_bitvalue_v<T>);
	static_assert(!(is_random_iter<Iter>::value && std::is_trivially_assignable_v<T, T>));
#endif

	return fast_fill(first, last, T());
}

template <typename T> inline
typename std::enable_if<std::is_trivially_assignable_v<T, T> >::type
fast_zero(T* first, T* last)
{
	memset(first, 0, (last - first) * sizeof(T));
}

template <int N, typename Block> inline void
fast_zero(bit_iterator<N, Block> first, bit_iterator<N, Block> last)
{
	UInt32 elemIndex = first.nr_elem();
	dms_assert(elemIndex < (bit_info<N, Block>::nr_elem_per_block));

	if (elemIndex)
	{
		(*first.data_begin()) &= ((Block(1) << (elemIndex * N)) - 1);
		first.skip_block();
	}

	dms_assert(first.nr_elem() == 0);

	bit_sequence<N, Block>(
		first.data_begin(),
		last - first
		).clear_range();
}

template <typename T> inline void
fast_zero_obj(T& obj)
{
	Byte* objPtr = reinterpret_cast<Byte*>(&obj);
	fast_zero(objPtr, objPtr + sizeof(T));
}

//----------------------------------------------------------------------
// Section      : fast_undefine
//----------------------------------------------------------------------

template <typename Iter> inline
void fast_undefine(Iter first, Iter last)
{
	using T = typename std::iterator_traits< Iter>::value_type;
	fast_fill(first, last, UNDEFINED_OR_ZERO(T));
}

template <int N, typename Block> inline void
fast_undefine(bit_iterator<N, Block> first, bit_iterator<N, Block> last)
{
	fast_zero(first, last); // sub-byte values don't have a separate 'nulll' indicator, so the faster 
}

template <typename P> inline
void fast_undefine(SA_Iterator<P> first, SA_Iterator<P> last)
{
	for (; first != last; ++first)
		MakeUndefined(*first);
}

//----------------------------------------------------------------------
// Section      : undefine_if_not
//----------------------------------------------------------------------

template <typename Iter, typename BT> inline
void
undefine_if_not(Iter first, Iter last, bit_iterator<1, BT> selIter)
{
	using T = typename std::iterator_traits< Iter>::value_type;

	while (first != last) // && selIter.nr_elem())
	{
		if (!*selIter)
			*first = UNDEFINED_VALUE(T);

		++selIter; ++first;
	}

}

//----------------------------------------------------------------------
// Section      : raw_init
//----------------------------------------------------------------------

template <typename Iter> inline
void raw_construct(Iter first, Iter last)
{
	using T = typename std::iterator_traits<Iter>::value_type;

	for (; first != last; ++first)
		new (first) T();
}


template <typename Iter> inline
typename std::enable_if< raw_constructed_v< typename std::iterator_traits<Iter>::value_type > >::type
raw_init(Iter first, Iter last)
{
	fast_zero(first, last);
}

template <typename Iter> inline
typename std::enable_if< !raw_constructed_v< typename std::iterator_traits<Iter>::value_type > >::type
raw_init(Iter first, Iter last)
{
	raw_construct(first, last);
}

template <typename Iter> inline
typename std::enable_if< raw_constructed_v< typename std::iterator_traits<Iter>::value_type > >::type
raw_awake(Iter first, Iter last)
{
	// NOP
}

template <typename Iter> inline
typename std::enable_if< ! raw_constructed_v< typename std::iterator_traits<Iter>::value_type > >::type
raw_awake(Iter first, Iter last)
{
	raw_construct(first, last);
}

template <typename Iter> inline
void raw_awake_or_init(Iter first, Iter last, bool mustZero)
{
	if (mustZero)
		raw_init(first, last);
	else
		raw_awake(first, last); // when all data is rewritten anyway, skip the fast_zero phase
}


#endif //!defined(__RTC_SET_RANGEFUNCS_H)
