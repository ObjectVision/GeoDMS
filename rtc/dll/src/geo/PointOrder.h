// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __RTC_GEO_POINTORDER_H
#define __RTC_GEO_POINTORDER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/ElemTraits.h"
#include "geo/Point.h"

template <typename P> struct Range;
template <typename P> struct sequence_traits;

//----------------------------------------------------------------------
// various defines
//----------------------------------------------------------------------

struct colrow_order_tag { static constexpr bool col_first = true; };
struct rowcol_order_tag { static constexpr bool col_first = false; };

template <typename Tag1, typename Tag2> struct must_swap
{
	static constexpr bool value = (Tag1::col_first != Tag2::col_first);
};

#if defined(DMS_POINT_ROWCOL)
	struct dms_order_tag : rowcol_order_tag {};
#else
	struct dms_order_tag : colrow_order_tag {};
#endif

	struct shp_order_tag : colrow_order_tag {};

template <bool MustSwap> struct reorder_functor;
template <> struct reorder_functor<false>
{
	template <typename  F>
	Point<F> operator ()(const Point<F>& point) const
	{
		return point;
	}
	template <typename  P>
	Range<P> operator ()(const Range<P>& rect) const
	{
		return rect;
	}
	template <typename T>
	void inplace(const T&) { /*NOP*/ }
};

template <> struct reorder_functor<true>
{
	template <typename  F>
	Point<F> operator ()(const Point<F>& point) const
	{
		return Point<F>(point.second, point.first);
	}
	template <typename  P>
	Range<P> operator ()(const Range<P>& rect) const
	{
		return Range<P>(
			operator ()(rect.first), 
			operator ()(rect.second)
		);
	}
	template <typename F>
	void inplace(Point<F>& p)
	{
		omni::swap(p.first, p.second);
	}
};

using shp_reorder_functor = reorder_functor< must_swap<shp_order_tag, dms_order_tag>::value >;


template <typename  F>
Point<F> shp2dms_order(F x, F y)
{
	return shp_reorder_functor()(Point<F>(x, y));
}

template <typename  F>
Point<F> shp2dms_order(const Point<F>& shpPoint)
{
	return shp_reorder_functor()(shpPoint);
}

template <typename P>
Range<P> shp2dms_order(const Range<P>& shpRect)
{
	return shp_reorder_functor()(shpRect);
}

RTC_CALL extern bool g_cfgColFirst;

template <typename  F>
Point<F> cfg2dms_order(const Point<F>& cfgPoint)
{
	if (g_cfgColFirst)
	{
		reorder_functor<
			must_swap<
				colrow_order_tag, 
				dms_order_tag
			>::value
		> rf;
		return rf(cfgPoint);
	}
	else
	{
		reorder_functor<
			must_swap<
				rowcol_order_tag, 
				dms_order_tag
			>::value
		> rf;
		return rf(cfgPoint);
	}
}

template <typename  F>
void cfg2dms_order_inplace(Point<F>& cfgPoint)
{
	if (g_cfgColFirst)
	{
		reorder_functor<
			must_swap<
				colrow_order_tag, 
				dms_order_tag
			>::value
		> rf;
		rf.inplace(cfgPoint);
	}
	else
	{
		reorder_functor<
			must_swap<
				rowcol_order_tag, 
				dms_order_tag
			>::value
		> rf;
		rf.inplace(cfgPoint);
	}
}

template <typename  F>
void  dmsPoint_SetFirstCfgValue(Point<F>& cfgPoint, F firstVal)
{
	if (g_cfgColFirst)
		cfgPoint.Col() = firstVal;
	else
		cfgPoint.Row() = firstVal;
}

template <typename  F>
void  dmsPoint_SetSecondCfgValue(Point<F>& cfgPoint, F secondVal)
{
	if (g_cfgColFirst)
		cfgPoint.Row() = secondVal;
	else
		cfgPoint.Col() = secondVal;
}

template <typename  F>
typename param_type<F>::type
dmsPoint_GetFirstCfgValue(const Point<F>& cfgPoint)
{
	if (g_cfgColFirst)
		return cfgPoint.Col();
	else
		return cfgPoint.Row();
}

template <typename  F>
typename param_type<F>::type
dmsPoint_GetSecondCfgValue(const Point<F>& cfgPoint)
{
	if (g_cfgColFirst)
		return cfgPoint.Row();
	else
		return cfgPoint.Col();
}


#endif // __RTC_GEO_POINTORDER_H


