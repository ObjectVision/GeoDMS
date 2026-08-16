// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_GEO_ROUND_H
#define __RTC_GEO_ROUND_H

#include "geom/Point.h"
#include "geom/Range.h"
#include "geom/Geometry.h"

//----------------------------------------------------------------------
// section : type conversion
//----------------------------------------------------------------------

template <int N> struct int_type;
template <> struct int_type<1> { typedef  Int8 signed_type; typedef  UInt8 unsigned_type; };
template <> struct int_type<2> { typedef Int16 signed_type; typedef UInt16 unsigned_type; };
template <> struct int_type<4> { typedef Int32 signed_type; typedef UInt32 unsigned_type; };
template <> struct int_type<8> { typedef Int64 signed_type; typedef UInt64 unsigned_type; };

template <typename T, typename I> struct scalar_subst { typedef I type; };
template <typename T, typename I> struct scalar_subst<Point<T>, I> { typedef Point<typename scalar_subst<T, I>::type> type; };
template <typename T, typename I> struct scalar_subst<Range<T>, I> { typedef Range<typename scalar_subst<T, I>::type> type; };

template <typename T, int N> struct scalar_replace   : scalar_subst<T, typename std::conditional_t<is_signed<T>::value, typename int_type<N>::signed_type, typename int_type<N>::unsigned_type> > {};
template <typename T, int N> struct scalar_replace_u : scalar_subst<T, typename int_type<N>::unsigned_type> {}; // unsigned variant: a Positive round to a wpoint/upoint target must round in UNSIGNED space, else a grid coord > Int16/Int32 max gets clamped to the signed max (e.g. 32767 / 2^31), truncating large grids

//----------------------------------------------------------------------
// section : RoundToZero
//----------------------------------------------------------------------`

template <int N, typename V>
typename scalar_replace<V, N>::type
	RoundToZero(const V& v)
{
	typedef typename scalar_replace<V, N>::type R;
	// NaN-safe: without this guard, `return v` would convert NaN to 0x80000000 (UNDEFINED).
	if (!IsDefined(v))
		return UNDEFINED_VALUE(R);
	if (v < MIN_VALUE(R))
		return MIN_VALUE(R);
	if (v > MAX_VALUE(R))
		return MAX_VALUE(R);

	return v;
}

template <int N, typename T> 
Point<typename scalar_replace<T, N>::type>
	RoundToZero(const Point<T>& p)
{
	return Point<typename int_type<N>::signed_type>(RoundToZero<N>(p.first), RoundToZero<N>(p.second) ); 
}

template <int N, typename P> 
Range<typename scalar_replace<P, N>::type>
	RoundToZero(const Range<P>& r)
{
	return Range<typename scalar_replace<P, N>::type>(RoundToZero<N>(r.first), RoundToZero<N>(r.second) ); 
}

//----------------------------------------------------------------------
// section : RoundDown
//----------------------------------------------------------------------

template <int N, typename V>
typename scalar_replace<V, N>::type
	RoundDown(const V& v)
{
	using R = typename scalar_replace<V, N>::type;

	static_assert(std::is_floating_point_v<V>, "RoundDown is only defined for floating-point types");

	// NaN-safe: comparisons against NaN are all false, so without this guard
	// the (double->int) conversion below would produce 0x80000000 (UNDEFINED).
	if (!IsDefined(v))
		return UNDEFINED_VALUE(R);
	if (v < MIN_VALUE(R))
		return MIN_VALUE(R);
	if (v > MAX_VALUE(R))
		return MAX_VALUE(R);

	R lv = v;
	if (V(lv) > v)
		--lv;
	return lv;
}

template <int N, typename T> 
Point<typename scalar_replace<T, N>::type> RoundDown(const Point<T>& p)
{
	return Point<typename scalar_replace<T, N>::type>(RoundDown<N>(p.first), RoundDown<N>(p.second) ); 
}

template <int N, typename P> 
Range<typename scalar_replace<P, N>::type> RoundDown(const Range<P>& r)
{
	return Range<typename scalar_replace<P, N>::type>(RoundDown<N>(r.first), RoundDown<N>(r.second) ); 
}

//----------------------------------------------------------------------
// section : RoundDownPositiveAndFloorAtZero
//----------------------------------------------------------------------

template <int N, typename V>
typename scalar_replace_u<V, N>::type
RoundDownPositiveAndFloorAtZero(const V& v)
{
	using R = typename scalar_replace_u<V, N>::type;

	// NaN-safe: NaN < 0 and NaN > MAX are both false, so `R uv = v` would otherwise
	// produce 0x80000000 (UNDEFINED for signed; bit-pattern garbage for unsigned).
	if (!IsDefined(v))
		return UNDEFINED_VALUE(R);

	static_assert(std::is_floating_point_v<V>, "RoundDown is only defined for floating-point types");
	if constexpr (std::is_signed_v<V>)
	{
		if (v < V(0)) // 0?
			return 0;
	}

	if (v > MAX_VALUE(R))
		return MAX_VALUE(R);

	R uv = v;
	dms_assert(uv <= v);
	return uv;
}

template <int N, typename T>
Point<typename scalar_replace_u<T, N>::type>
RoundDownPositiveAndFloorAtZero(const Point<T>& p)
{
	return Point<typename scalar_replace_u<T, N>::type>(RoundDownPositiveAndFloorAtZero<N>(p.first), RoundDownPositiveAndFloorAtZero<N>(p.second));
}

template <int N, typename P>
Range<typename scalar_replace_u<P, N>::type>
RoundDownPositiveAndFloorAtZero(const Range<P>& r)
{
	return Range<typename scalar_replace_u<P, N>::type>(RoundDownPositiveAndFloorAtZero<N>(r.first), RoundDownPositiveAndFloorAtZero<N>(r.second));
}

//----------------------------------------------------------------------
// section : RoundDownPositive
//----------------------------------------------------------------------

template <int N, typename V>
typename scalar_replace_u<V, N>::type
	RoundDownPositive(const V& v)
{
	static_assert(std::is_floating_point_v<V>, "RoundDown is only defined for floating-point types");

	using R = typename scalar_replace_u<V, N>::type;

	// NaN-safe: dms_assert is debug-only; in release builds NaN would silently
	// flow through to `R uv = v` and convert to 0x80000000.
	if (!IsDefined(v))
		return UNDEFINED_VALUE(R);

	if constexpr (std::is_signed_v<V>)
	{
		if (v < V(0))
			return 0;
	}

	if (v > MAX_VALUE(R))
		return MAX_VALUE(R);

	R uv = v;
	assert(uv <= v);
	return uv;
}

template <int N, typename T> 
Point<typename scalar_replace_u<T, N>::type>
	RoundDownPositive(const Point<T>& p)
{
	return Point<typename scalar_replace_u<T, N>::type>(RoundDownPositive<N>(p.first), RoundDownPositive<N>(p.second) ); 
}

template <int N, typename P> 
Range<typename scalar_replace_u<P, N>::type> 
	RoundDownPositive(const Range<P>& r)
{
	return Range<typename scalar_replace_u<P, N>::type>(RoundDownPositive<N>(r.first), RoundDownPositive<N>(r.second) ); 
}

//----------------------------------------------------------------------
// section : RoundUp
//----------------------------------------------------------------------

template <int N, typename V>
typename scalar_replace<V, N>::type
	RoundUp(const V& v)
{
	static_assert(std::is_floating_point_v<V>, "RoundDown is only defined for floating-point types");

	using R = typename scalar_replace<V, N>::type;

	// Symmetric with RoundDown: without these guards a double outside [MIN,MAX](R)
	// (or NaN) would convert to 0x80000000 (UNDEFINED) and the ++lv branch would
	// then silently shift it by 1, producing a partially-defined Range<IPoint>
	// that breaks invariants downstream (e.g. GRect ctor in shv/GeoTypes.h).
	if (!IsDefined(v))
		return UNDEFINED_VALUE(R);
	if (v < MIN_VALUE(R))
		return MIN_VALUE(R);
	if (v > MAX_VALUE(R))
		return MAX_VALUE(R);

	R lv = v;
	if (V(lv) < v)
		++lv;
	return lv;
}

template <int N, typename T> 
Point<typename scalar_replace<T, N>::type>
	RoundUp(const Point<T>& p)
{
	return Point<typename scalar_replace<T, N>::type>(RoundUp<N>(p.first), RoundUp<N>(p.second) ); 
}

template <int N, typename P> 
Range<typename scalar_replace<P, N>::type>
	RoundUp(const Range<P>& r)
{
	return Range<typename scalar_replace<P, N>::type>(RoundUp<N>(r.first), RoundUp<N>(r.second) );
}

//----------------------------------------------------------------------
// section : RoundUpPositive
//----------------------------------------------------------------------
// Unsigned-target ceil. RoundUp<N> derives its result scalar from scalar_replace<V,N>,
// which keys off the SOURCE signedness; converting a signed source (e.g. Int32 grid coord)
// to an unsigned 16/32-bit target therefore picks the SIGNED int_type (Int16/Int32) and
// (since 10685294) clamps at the signed max (32767 / 2^31), truncating large grids.
// RoundUpPositive uses scalar_replace_u (unsigned), so it clamps at the UNSIGNED max.

template <int N, typename V>
typename scalar_replace_u<V, N>::type
	RoundUpPositive(const V& v)
{
	using R = typename scalar_replace_u<V, N>::type;
	if (!IsDefined(v))
		return UNDEFINED_VALUE(R);
	if constexpr (std::is_signed_v<V>)
	{
		if (v < V(0))
			return 0;
	}
	if (v > MAX_VALUE(R))
		return MAX_VALUE(R);
	R uv = v;
	if (V(uv) < v) // round up if a fractional part was truncated
		++uv;
	return uv;
}

template <int N, typename T>
Point<typename scalar_replace_u<T, N>::type>
	RoundUpPositive(const Point<T>& p)
{
	return Point<typename scalar_replace_u<T, N>::type>(RoundUpPositive<N>(p.first), RoundUpPositive<N>(p.second) );
}

template <int N, typename P>
Range<typename scalar_replace_u<P, N>::type>
	RoundUpPositive(const Range<P>& r)
{
	return Range<typename scalar_replace_u<P, N>::type>(RoundUpPositive<N>(r.first), RoundUpPositive<N>(r.second) );
}

//----------------------------------------------------------------------
// section : Round
//----------------------------------------------------------------------

template <int N, typename T> 
typename scalar_replace<T, N>::type
	Round(const T& v)
{
	return RoundDown<N>(v + 0.5);
}

template <int N, typename T> 
Point<typename scalar_replace<T, N>::type>
	Round(const Point<T>& p)
{
	return Point<typename scalar_replace<T, N>::type>(Round<N>(p.first), Round<N>(p.second) ); 
}

template <int N, typename P> 
Range<typename scalar_replace<P, N>::type> 
	Round(const Range<P>& r)
{
	return Range<typename scalar_replace<P, N>::type>(Round<N>(r.first), Round<N>(r.second) ); 
}

//----------------------------------------------------------------------
// section : RoundPositive
//----------------------------------------------------------------------

template <int N, typename V> 
typename scalar_replace_u<V, N>::type
	RoundPositive(const V& v)
{
	return RoundDownPositive<N>(v + 0.5);
}

template <int N, typename T> 
Point<typename scalar_replace_u<T, N>::type>
	RoundPositive(const Point<T>& p)
{
	return Point<typename scalar_replace_u<T, N>::type>(RoundPositive<N>(p.first), RoundPositive<N>(p.second) ); 
}

template <int N, typename P> 
Range<typename scalar_replace_u<P, N>::type>
	RoundPositive(const Range<P>& r)
{
	return Range<typename scalar_replace_u<P, N>::type>(RoundPositive<N>(r.first), RoundPositive<N>(r.second) ); 
}

//----------------------------------------------------------------------
// section : RoundPositiveHalfOpen
//----------------------------------------------------------------------

template <int N, typename V>
typename scalar_replace_u<V, N>::type
RoundPositiveHalfOpen(const V& v)
{
	auto result = RoundDownPositive<N>(v);
	return result + decltype(result)((v - result) > 0.5);
}

template <int N, typename T>
Point<typename scalar_replace_u<T, N>::type>
RoundPositiveHalfOpen(const Point<T>& p)
{
	return Point<typename scalar_replace_u<T, N>::type>(RoundPositiveHalfOpen<N>(p.first), RoundPositiveHalfOpen<N>(p.second));
}

template <int N, typename P>
Range<typename scalar_replace_u<P, N>::type>
RoundPositiveHalfOpen(const Range<P>& r)
{
	return Range<typename scalar_replace_u<P, N>::type>(RoundPositiveHalfOpen<N>(r.first), RoundPositiveHalfOpen<N>(r.second));
}

//----------------------------------------------------------------------
// section : RoundPositiveAndFloorAtZero
//----------------------------------------------------------------------

template <int N, typename V>
typename scalar_replace_u<V, N>::type
RoundPositiveAndFloorAtZero(const V& v)
{
	return RoundDownPositiveAndFloorAtZero<N>(v + 0.5);
}

template <int N, typename T>
Point<typename scalar_replace_u<T, N>::type>
RoundPositiveAndFloorAtZero(const Point<T>& p)
{
	return Point<typename scalar_replace_u<T, N>::type>(RoundPositiveAndFloorAtZero<N>(p.first), RoundPositiveAndFloorAtZero<N>(p.second));
}

template <int N, typename P>
Range<typename scalar_replace_u<P, N>::type>
RoundPositiveAndFloorAtZero(const Range<P>& r)
{
	return Range<typename scalar_replace_u<P, N>::type>(RoundPositiveAndFloorAtZero<N>(r.first), RoundPositiveAndFloorAtZero<N>(r.second));
}

//----------------------------------------------------------------------
// section : RoundEnclosing
//----------------------------------------------------------------------

template <int N, typename P> 
Range<typename scalar_replace<P, N>::type>
	RoundEnclosing(const Range<P>& r)
{
	return Range<typename scalar_replace<P, N>::type>(RoundDown<N>(r.first), RoundUp<N>(r.second) ); 
}

//----------------------------------------------------------------------
// section : RoundEnclosed
//----------------------------------------------------------------------

template <int N, typename P> 
Range<typename scalar_replace<P, N>::type>
	RoundEnclosed(const Range<P>& r)
{
	return Range<typename scalar_replace<P, N>::type>(RoundUp<N>(r.first), RoundDown<N>(r.second) ); 
}

#endif // __RTC_GEO_ROUND_H


