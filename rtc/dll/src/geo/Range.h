// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_GEO_RANGE_H)
#define __RTC_GEO_RANGE_H

#include "RtcBase.h"

#include "dbg/Check.h"

#include "geo/BaseBounds.h"
#include "geo/Point.h"
#include "geo/ElemTraits.h"

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
Range<Dst> Convert4(Range<Src> src, const Range<Dst>*, const ExceptFunc* ef, const ConvertFunc* cf)
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
bool IsTouching(Range<T> a, Range<T> b)
{
	return	IsLowerBound(b.first, a.second)
		&&	IsLowerBound(a.first, b.second);
}

template <class T> inline
bool IsTouching(Range<T> a, T e)
{
	return	IsLowerBound(a.first, e)
		&&	IsLowerBound(e, a.second);
}

template <class T> inline
bool IsIntersecting(Range<T> a, Range<T> b)
{
	return IsStrictlyLower(b.first, a.second) && IsStrictlyLower(a.first, b.second);
}

template <class T> inline
bool IsIntersecting(Range<T> a, T e)
{
	return IsLowerBound(a.first, e) && IsStrictlyLower(e, a.second);
}

template <class T> inline
bool IsIntersecting(T e, Range<T> a)
{
	return IsLowerBound(a.first, e) && IsStrictlyLower(e, a.second);
}

template <class T> inline
void operator &= (Range<T>& a, Range<T> b)
{
	MakeUpperBound(a.first,  b.first);
	MakeLowerBound(a.second, b.second);
}

template <class T> inline
Range<T> operator &(Range<T> a, Range<T> b)
{
	a &= b;
	return a;
}

template <typename T, std::convertible_to<T> U> inline
void operator |= (Range<T>& a, const Range<U>& b)
{
	MakeLowerBound(a.first,  Convert<T>(b.first));
	MakeUpperBound(a.second, Convert<T>(b.second));
}

template <typename T, std::convertible_to<T> U> inline
Range<T> operator |(Range<T> a, const Range<U>& b)
{
	a |= b;
	return a;
}

template <typename T, std::convertible_to<T> U> inline
void operator |= (Range<T>& a, U b)
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
bool IsIncluding(Range<T> a, Range<T> b)
{
	return IsLowerBound(a.first, b.first) && IsUpperBound(a.second, b.second);
}

template <class T> inline
bool IsIncluding(Range<T> a, T e)
{
	return IsLowerBound(a.first, e) && IsStrictlyLower(e, a.second);
}

template <bit_size_t N>
inline bool IsIncluding(Range<UInt32> a, bit_value<N> e)
{
	dms_assert(a.first  == 0);
	dms_assert(a.second == (1 << N));
	dms_assert(UInt32(bit_value<N>::base_type(e)) <  (1 << N));
	return true;
}

template <bit_size_t N, typename Block>
inline bool IsIncluding(Range<UInt32> a, bit_reference<N, Block> e)
{
	return IsIncluding(a, bit_value<N>(e));
}

template <typename T, typename CIT> inline
bool IsIncluding(Range<T> a, CIT first, CIT last)
{
	for (; first != last; ++first)
		if (!IsIncluding(a, *first))
			return false;
	return true;
}

template <typename T, typename CIT> inline
bool IsIncludingOrUndefined(Range<T> a, CIT first, CIT last)
{
	for(; first != last; ++first)
		if (IsDefined(*first) && !IsIncluding(a, *first))
			return false;
	return true;
}

template <class T> inline
void operator += (Range<T>& a, Range<T> b)
{
	a.first  += b.first;
	a.second += b.second;
}

template <class T> inline
void operator += (Range<T>& a, T b)
{
	a.first  += b;
	a.second += b;
}

template <class T> inline
void operator -= (Range<T>& a, Range<T> b)
{
	a.first  -= b.first;
	a.second -= b.second;
	MakeUpperBound(a.second, a.first);
}

template <class T> inline
void operator -= (Range<T>& a, T b)
{
	a.first  -= b;
	a.second -= b;
}

template <class T> inline
Range<T> operator + (Range<T> a, Range<T> b)
{
	return Range<T>(a.first + b.first, a.second + b.second);
}

template <class T> inline
Range<T> operator + (Range<T> a, T b)
{
	return Range<T>(a.first + b, a.second + b);
}

template <class T> inline
Range<T> operator - (Range<T> a, Range<T> b)
{
	return Range<T>(a.first - b.first, a.second - b.second);
}

template <class T> inline
Range<T> operator - (Range<T> a, T b)
{
	return Range<T>(a.first - b, a.second - b);
}

// unary negate range
template <class T> inline
Range<T> operator - (Range<T> a)
{
	return Range<T>(-a.second, -a.first);
}

template <class T>
inline Float64 AsFloat64(Range<T> x )
{
	throwIllegalAbstract(MG_POS, "Range::AsFloat64");
}

template <class T> inline
T Center(Range<T> r) { return (r.first+r.second) / scalar_of_t<T>(2); }

template <class T> inline
typename unsigned_type<T>::type
Size(Range<T> r) { return r.second - r.first; }

template <class T> inline
Range<T> Inflate(Range<T> r, T p) { return Range<T>(r.first-p, r.second+p); }

template <class T> inline
Range<T> Deflate(Range<T> r, T p) { return Range<T>(r.first+p, r.second-p); }

template <class T> inline
Range<T> Inflate(T center, T radius) { return Range<T>(center-radius, center+radius); }

template <class T> inline T Left  (Range<Point<T> > r) { return r.first .Col(); }
template <class T> inline T Top   (Range<Point<T> > r) { return r.first .Row(); }
template <class T> inline T Right (Range<Point<T> > r) { return r.second.Col(); }
template <class T> inline T Bottom(Range<Point<T> > r) { return r.second.Row(); }
template <class T> inline T Width (Range<Point<T> > r) { return Right (r) - Left(r); }
template <class T> inline T Height(Range<Point<T> > r) { return Bottom(r) - Top (r); }

template <class T> inline Point<T> TopLeft(Range<Point<T> > r) { return r.first;  }
template <class T> inline Point<T> BottomRight(Range<Point<T> > r) { return r.second; }
template <class T> inline Point<T> TopRight(Range<Point<T> > r) { return shp2dms_order(r.second.X(), r.first.Y()); }
template <class T> inline Point<T> BottomLeft(Range<Point<T> > r) { return shp2dms_order(r.first.X(), r.second.Y()); }

#endif // __RTC_GEO_RANGE_H
