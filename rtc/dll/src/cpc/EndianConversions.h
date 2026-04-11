// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_CPC_EMNDIANCONVERSIONS_H)
#define __RTC_CPC_EMNDIANCONVERSIONS_H

#include "cpc/CompChar.h"
#include "cpc/Types.h"
#include "dbg/Check.h"
#include "ptr/IterCast.h"

#include <algorithm>

//----------------------------------------------------------------------
// Flip byte order and write
//----------------------------------------------------------------------

template<unsigned N> inline void byte_reverse_n(void* t)
{
	std::reverse(
		((unsigned char *) t),
		((unsigned char *) t)+N
	);
};

template<class T> inline void byte_reverse(T& t)
{
	byte_reverse_n<sizeof(T)>(&t);
};

//----------------------------------------------------------------------
// CC_BYTEORDER BASED FUNCTIONS
//----------------------------------------------------------------------

#if defined(CC_BYTEORDER_INTEL)

#	if defined(CC_TRACE_COMPILER)
#		pragma message("Byte order as Intel")
#	endif

	template <class T> inline void ConvertLittleEndian(T& )  {}
	template <class T> inline void ConvertBigEndian   (T& x) { byte_reverse(x); }

	template <class Iter> inline void ConvertLittleEndian(Iter f, Iter l) {}
	template <class Iter> inline void ConvertBigEndian   (Iter f, Iter l)
	{ 
		typedef typename std::iterator_traits<Iter>::value_type T;
		std::for_each(f, l, &byte_reverse<T>); 
	}

#endif

#if defined(CC_BYTEORDER_MOTOROLA)

#	if defined(CC_TRACE_COMPILER)
#		pragma message("Byte order as Motorola")
#	endif

	template <class T> inline void ConvertBigEndian   (T& )  {}
	template <class T> inline void ConvertLittleEndian(T& x) { byte_reverse(x); }

	template <class Iter> inline void ConvertBigEndian   (Iter f, Iter l) {}
	template <class Iter> inline void ConvertLittleEndian(Iter f, Iter l)
	{ 
		typedef typename std::iterator_traits<Iter>::value_type T;
		std::for_each(f, l, &byte_reverse<T>); 
	}
#endif

template <class T> struct Point;
template <class T> struct Range;

template <class T> inline void ConvertBigEndian(Point<T>& p) { ConvertBigEndian(p.first); ConvertBigEndian(p.second); }
template <class T> inline void ConvertBigEndian(Range<T>& r) { ConvertBigEndian(r.first); ConvertBigEndian(r.second); }
template <class T> inline void ConvertLittleEndian(Point<T>& p) { ConvertLittleEndian(p.first); ConvertLittleEndian(p.second); }
template <class T> inline void ConvertLittleEndian(Range<T>& r) { ConvertLittleEndian(r.first); ConvertLittleEndian(r.second); }

//----------------------------------------------------------------------
// NibbleSwap, TwipsSwap, BitSwap
//----------------------------------------------------------------------

inline UInt32 NibbleSwap(UInt32 dWord)
{
	// SWAP NIBBLES (UInt4)
	return 
			((dWord & 0xF0F0F0F0) >> 4) 
		|	((dWord & 0x0F0F0F0F) << 4);
}

inline UInt32 TwipSwap(UInt32 dWord)
{
	dWord = NibbleSwap(dWord);

	// SWAP TWIPS (UInt2)
	return 
			((dWord & 0xCCCCCCCC) >> 2) 
		|	((dWord & 0x33333333) << 2);
}

inline UInt32 BitSwap(UInt32 dWord)
{
	dWord = TwipSwap(dWord);

	// SWAP BITS (UInt1)
	return 
			((dWord & 0xAAAAAAAA) >> 1) 
		|	((dWord & 0x55555555) << 1);
}

inline UInt32 BitReverse(UInt32 dWord)
{
	byte_reverse(dWord);
	return BitSwap(dWord);
}

template <typename Iter>
inline void NibbleSwap(Iter b, Iter e)
{
	static_assert(sizeof(typename std::iterator_traits<Iter>::value_type)==4);

	dms_assert(SizeT(&*b) % 4 == 0); // alignment requirement
	dms_assert(SizeT(&*e) % 4 == 0); // alignment requirement

	for (; b != e; ++b)
		*b = NibbleSwap(*b);
}

template <typename Iter>
inline void TwipSwap(Iter b, Iter e)
{
	static_assert(sizeof(typename std::iterator_traits<Iter>::value_type)==4);

	dms_assert(SizeT(&*b) % 4 == 0); // alignment requirement
	dms_assert(SizeT(&*e) % 4 == 0); // alignment requirement

	for (; b != e; ++b)
		*b = TwipSwap(*b);
}

template <typename Iter>
inline void BitSwap(Iter b, Iter e)
{
	static_assert(sizeof(typename std::iterator_traits<Iter>::value_type)==4);

	dms_assert(SizeT(&*b) % 4 == 0); // alignment requirement
	dms_assert(SizeT(&*e) % 4 == 0); // alignment requirement

	for (; b != e; ++b)
		*b = BitSwap(*b);
}

template <typename Iter>
inline void BitReverse(Iter b, Iter e)
{
	static_assert(sizeof(typename std::iterator_traits<Iter>::value_type)==4);

	dms_assert(b==e || SizeT(&b[ 0]) % 4 == 0); // alignment requirement
	dms_assert(b==e || SizeT(&e[-1]) % 4 == 0); // alignment requirement

	for (; b != e; ++b)
		*b = BitReverse(*b);
}

#endif // __RTC_CPC_EMNDIANCONVERSIONS_H
