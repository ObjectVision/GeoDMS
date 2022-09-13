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

#if !defined(__RTC_GEO_RANGE_H)
#define __RTC_GEO_RANGE_H

#include "rtcBase.h"

#include "dbg/Check.h"


#include "geo/BaseBounds.h"
#include "geo/Point.h"
#include "geo/Conversions.h"

template <class T>
std::enable_if_t<is_integral_v<field_of_t<T> >>
	MakeHalfOpen(Range<T>& range)
{
	if (IsLowerBound(range.first, range.second))
		MakeStrictlyGreater(range.second);
}
 
template <class T>
typename std::enable_if_t<!is_integral_v<field_of_t<T>> >
	MakeHalfOpen(Range<T>&)
{}

template <class T>
struct Range : Couple<T>
{
	using typename Couple<T>::value_cref;
	using Couple<T>::first;
	using Couple<T>::second;

	Range()	: Couple<T>(MaxValue<T>(), MinValue<T>())
	{}

	Range(value_cref v1, value_cref v2): Couple<T>(LowerBound(v1, v2), UpperBound(v1, v2))  
	{} // init 'negative' ranges as positive

	Range(Undefined): Couple<T>(Undefined() ) {}

	template<typename CT_Iter>
	Range(CT_Iter first, CT_Iter last, bool mustCheckUndefinedValues,bool mustBeMadeHalfOpen)
		:	Couple<T>(MaxValue<T>(), MinValue<T>())       
	{
		if (mustCheckUndefinedValues)
		{
			for (;first!=last;++first)
				if (IsDefined(*first))
					(*this) |= *first;
		}
		else
			for (;first!=last;++first)
				(*this) |= *first;

		if (mustBeMadeHalfOpen)
			MakeHalfOpen(*this);
	}

	bool inverted() const { return ! IsLowerBound   (first, second); }
	bool empty   () const { return ! IsStrictlyLower(first, second); }
	bool is_max  () const { dms_assert(!inverted()); return first == MinValue<T>() && second == MaxValue<T>(); }
};


//----------------------------------------------------------------------
// Section      : Undefined
//----------------------------------------------------------------------

template <class T>
inline Range<T> UndefinedValue(const Range<T>*)
{
	return Range<T>(Undefined());
}

template <class T>
inline bool ContainsUndefined(const Range<T>& range)
{
	return IsIncluding(range, UNDEFINED_VALUE(T));
}

//----------------------------------------------------------------------
// Section      : Convert
//----------------------------------------------------------------------

template <typename Dst, typename Src, typename ExceptFunc, typename ConvertFunc> inline
Range<Dst> Convert4(const Range<Src>& src, const Range<Dst>*, const ExceptFunc* ef, const ConvertFunc* cf)
{
	if (src.inverted())
		return Range<Dst>();

	using RoundDn = typename ConvertFunc::template RoundDnFunc<Dst>::type;
	using RoundUp = typename ConvertFunc::template RoundUpFunc<Dst>::type;
	return
		Range<Dst>(
			Convert4(src.first,  TYPEID(Dst), ef, TYPEID(RoundDn))
		,	Convert4(src.second, TYPEID(Dst), ef, TYPEID(RoundUp))
		);
}

//----------------------------------------------------------------------
// Section      : range operators
//----------------------------------------------------------------------
// Full name    : Aggregation helper definitioin
// Purpose      : fast operation for aggregation operations
//----------------------------------------------------------------------

template <class T> inline
bool IsTouching(const Range<T>& a, const Range<T>& b)
{
	return	IsLowerBound(b.first, a.second)
		&&	IsLowerBound(a.first, b.second);
}

template <class T> inline
bool IsTouching(const Range<T>& a, T e)
{
	return	IsLowerBound(a.first, e)
		&&	IsLowerBound(e, a.second);
}

template <class T> inline
bool IsIntersecting(const Range<T>& a, const Range<T>& b)
{
	return IsStrictlyLower(b.first, a.second) && IsStrictlyLower(a.first, b.second);
}

template <class T> inline
bool IsIntersecting(const Range<T>& a, T e)
{
	return IsLowerBound(a.first, e) && IsStrictlyLower(e, a.second);
}

template <class T> inline
bool IsIntersecting(T e, const Range<T>& a)
{
	return IsLowerBound(a.first, e) && IsStrictlyLower(e, a.second);
}

template <class T> inline
void operator &= (Range<T>& a, const Range<T>& b)
{
	MakeUpperBound(a.first,  b.first);
	MakeLowerBound(a.second, b.second);
}

template <class T> inline
Range<T> operator &(Range<T> a, const Range<T>& b)
{
	a &= b;
	return a;
}

template <typename T, typename U> inline
void operator |= (Range<T>& a, const Range<U>& b)
{
	MakeLowerBound(a.first,  Convert<T>(b.first));
	MakeUpperBound(a.second, Convert<T>(b.second));
}

template <typename T, typename U> inline
Range<T> operator |(Range<T> a, const Range<U>& b)
{
	a |= b;
	return a;
}

template <typename T, typename U> inline
void operator |= (Range<T>& a, const U& b)
{
	MakeLowerBound(a.first,  Convert<T>(b));
	MakeUpperBound(a.second, Convert<T>(b));
}

template <typename T, typename U> inline
void operator |= (Range<T>& a, const std::vector<U>& b)
{
	a |= RangeFromSequence(b.begin(), b.end());
}

template <class T, typename CIT> inline
void MakeIncluding(Range<T>& a, CIT first, CIT last)
{
	for( ; first != last; ++first)
		a |= *first;
}

template <class T> inline
bool IsIncluding(const Range<T>& a, const Range<T>& b)
{
	return IsLowerBound(a.first, b.first) && IsUpperBound(a.second, b.second);
}

template <class T> inline
bool IsIncluding(const Range<T>& a, T e)
{
	return IsLowerBound(a.first, e) && IsStrictlyLower(e, a.second);
}

template <bit_size_t N>
inline bool IsIncluding(const Range<UInt32>& a, bit_value<N> e)
{
	dms_assert(a.first  == 0);
	dms_assert(a.second == (1 << N));
	dms_assert(UInt32(bit_value<N>::base_type(e)) <  (1 << N));
	return true;
}

template <bit_size_t N, typename Block>
inline bool IsIncluding(const Range<UInt32>& a, bit_reference<N, Block> e)
{
	return IsIncluding(a, bit_value<N>(e));
}

template <typename T, typename CIT> inline
bool IsIncluding(const Range<T>& a, CIT first, CIT last)
{
	for (; first != last; ++first)
		if (!IsIncluding(a, *first))
			return false;
	return true;
}

template <typename T, typename CIT> inline
bool IsIncludingOrUndefined(const Range<T>& a, CIT first, CIT last)
{
	for(; first != last; ++first)
		if (IsDefined(*first) && !IsIncluding(a, *first))
			return false;
	return true;
}

template <class T> inline
void operator += (Range<T>& a, const Range<T>& b)
{
	a.first  += b.first;
	a.second += b.second;
}

template <class T> inline
void operator += (Range<T>& a, const T& b)
{
	a.first  += b;
	a.second += b;
}

template <class T> inline
void operator -= (Range<T>& a, const Range<T>& b)
{
	a.first  -= b.first;
	a.second -= b.second;
	MakeUpperBound(a.second, a.first);
}

template <class T> inline
void operator -= (Range<T>& a, const T& b)
{
	a.first  -= b;
	a.second -= b;
}

template <class T> inline
Range<T> operator + (const Range<T>& a, const Range<T>& b)
{
	return Range<T>(a.first + b.first, a.second + b.second);
}

template <class T> inline
Range<T> operator + (const Range<T>& a, const T& b)
{
	return Range<T>(a.first + b, a.second + b);
}

template <class T> inline
Range<T> operator - (const Range<T>& a, const Range<T>& b)
{
	return Range<T>(a.first - b.first, a.second - b.second);
}

template <class T> inline
Range<T> operator - (const Range<T>& a, const T& b)
{
	return Range<T>(a.first - b, a.second - b);
}

// unary negate range
template <class T> inline
Range<T> operator - (const Range<T>& a)
{
	return Range<T>(-a.second, -a.first);
}

template <class T>
inline Float64 AsFloat64(const Range<T>& x )
{
	throwIllegalAbstract(MG_POS, "Range::AsFloat64");
}

template <class T> inline
T Center(const Range<T>& r) { return (r.first+r.second) / scalar_of<T>::type(2); }

template <class T> inline
typename unsigned_type<T>::type
Size(const Range<T>& r) { return r.second - r.first; }

template <class T> inline
SizeT
Cardinality(const Range<T>& r)
{ 
	return IsDefined(r) 
		?	r.empty()
			?	0
			:	Cardinality(Size(r)) 
		:	UNDEFINED_VALUE(SizeT); 
}

template <> inline
SizeT
Cardinality(const Range<Void>& ) { return 1; }

template <class T> inline
typename product_type<T>::type
Area(const Range<Point<T> >& r)
{ 
	return IsDefined(r) 
		?	r.empty()
			?	0
			:	Area(Size(r) ) 
		:	UNDEFINED_VALUE(product_type<T>::type); 
}

template <class T> inline
Range<T> Inflate(const Range<T>& r, const T& p) { return Range<T>(r.first-p, r.second+p); }

template <class T> inline
Range<T> Deflate(const Range<T>& r, const T& p) { return Range<T>(r.first+p, r.second-p); }

template <class T> inline
Range<T> Inflate(const T& center, const T& radius) { return Range<T>(center-radius, center+radius); }

template <class T> inline T _Left  (const Range<Point<T> >& r) { return r.first .Col(); }
template <class T> inline T _Top   (const Range<Point<T> >& r) { return r.first .Row(); }
template <class T> inline T _Right (const Range<Point<T> >& r) { return r.second.Col(); }
template <class T> inline T _Bottom(const Range<Point<T> >& r) { return r.second.Row(); }
template <class T> inline T _Width (const Range<Point<T> >& r) { return _Right (r) - _Left(r); }
template <class T> inline T _Height(const Range<Point<T> >& r) { return _Bottom(r) - _Top (r); }

#endif // __RTC_GEO_RANGE_H
