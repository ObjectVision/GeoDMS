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

#ifndef __RTC_GEO_ROUND_H
#define __RTC_GEO_ROUND_H

#include "geo/Point.h"
#include "geo/Range.h"
#include "geo/Geometry.h"

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
template <typename T, int N> struct scalar_replace_u : scalar_subst<T, typename int_type<N>::signed_type> {};

//----------------------------------------------------------------------
// section : RoundToZero
//----------------------------------------------------------------------`

template <int N, typename V> 
typename scalar_replace<V, N>::type
	RoundToZero(const V& v)
{
	typedef typename scalar_replace<V, N>::type R;
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
	typedef typename scalar_replace_u<V, N>::type R;

	if (v < R(0)) // 0?
		return 0;

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
	typedef typename scalar_replace_u<V, N>::type R;

	dms_assert(v>=0);

	if (v > MAX_VALUE(R))
		return MAX_VALUE(R);

	R uv = v;
	dms_assert(uv <= v);
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
	typename scalar_replace<V, N>::type lv = v;
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


