// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __GEO_GEODIST_H
#define __GEO_GEODIST_H

#include "geo/Conversions.h"

using seq_elem_index_type = UInt32;

//----------------------------------------------------------------------
// Section      : Distance Bounds
//----------------------------------------------------------------------

template <typename T>
T MinDistToRange(T v, Range<T> r)
{
	assert(r.first <= r.second);
	if (v < r.first) return r.first - v;
	if (v > r.second) return v - r.second;
	return 0;
}

template <typename T>
T MaxDistToRange(T v, Range<T> r)
{
	return Max<T>(Dist(v, r.first), Dist(v, r.second));
}

template <typename T>
Point<T> MinDistToRange(Point<T> v, Range<Point<T>> r)
{
	return Point<T>(
		MinDistToRange(v.first, Range<T>(r.first.first, r.second.first)),
		MinDistToRange(v.second, Range<T>(r.first.second, r.second.second))
	);
}

//----------------------------------------------------------------------
// Section      : Distance Measures
//----------------------------------------------------------------------

template <typename R, typename T>
inline Point<T> Project2Segm(const Point<T>& p, const Point<T>& a, const Point<T>& b)
{
	Point<T> ab = b-a;
	Point<T> ap = p-a;

	R c = InProduct<R>(ab, ap);
	if (c<=0)
		return a;
	R abSqrDist = Norm<R>(ab);
	if (c > abSqrDist)
		return b;

	return a + Convert<Point<T> >(Convert<Point<R> >(ab) * (InProduct<R>(ap, ab) / abSqrDist));
}

template <typename R, typename T>
inline R SqrDist2Segm(const Point<T>& p, const Point<T>& a, const Point<T>& b)
{
	Point<T> ab = b-a;
	Point<T> ap = p-a;

	R c = InProduct<R>(ab, ap);
	if ( c<=0 )
		return MaxValue<R>();
	R abSqrDist = Norm<R>(ab);
	if (c > abSqrDist)
		return SqrDist<R>(p, b);
	return Sqr(OutProduct<R>(ap, ab)) / abSqrDist;
}

// *****************************************************************************
//									ArcProjectionHandle
// *****************************************************************************

template <typename R, typename T>
struct ArcProjectionHandle
{
	typedef Point<T>         PointType;
	typedef Range<PointType> RectType;
	typedef const PointType* ConstPointPtr;
	typedef R                sqrdist_type;

	PointType     m_Point;
	sqrdist_type  m_MinSqrDist = 0;

	// result vars
	bool         m_FoundAny = false, m_InArc = false, m_InSegm = false;
	seq_elem_index_type m_SegmIndex = UNDEFINED_VALUE(seq_elem_index_type);
	PointType    m_CutPoint;
	
	ArcProjectionHandle() {}

	ArcProjectionHandle(PointType p, sqrdist_type minSqrDist)
		:	m_Point(p)
		,	m_MinSqrDist(minSqrDist)
	{}

	bool CanSkip(const RectType& bb)
	{
		auto dist = MinDistToRange(m_Point, bb);
		return m_MinSqrDist < Norm<R>(dist);
	}

	bool MakeSafeMin(sqrdist_type newDist)
	{
		if (newDist < m_MinSqrDist)
		{
			m_MinSqrDist = newDist;
			return true;
		}
		return !m_FoundAny && newDist == m_MinSqrDist;
	}

	bool Project2Arc(ConstPointPtr arcBegin, ConstPointPtr arcEnd);
}; 

template <typename R, typename T>
bool ArcProjectionHandle<R, T>::Project2Arc(ConstPointPtr arcBegin, ConstPointPtr arcEnd)
{
	if (arcBegin == arcEnd)
		return false;

	ConstPointPtr 
		j = arcBegin, 
		i = j, 
		nearestSegm = nullptr;

	if	( MakeSafeMin(Norm<sqrdist_type>(m_Point - *arcBegin) ) )
		nearestSegm = arcBegin;

	for ( ; ++j != arcEnd ; i = j)
		if (MakeSafeMin( SqrDist2Segm<sqrdist_type>(m_Point, *i, *j)) )
			nearestSegm = i;

	if (nearestSegm == nullptr)
		return false;

	assert(i+1 == arcEnd);

	m_SegmIndex = nearestSegm - arcBegin;
	ConstPointPtr nearestSegmEnd = nearestSegm+1;
	if (nearestSegmEnd == arcEnd) // only possible when linestring is only one point
	{
		assert(nearestSegm == arcBegin);
		assert(arcBegin + 1 == arcEnd);
		assert(m_InArc == false);
		m_CutPoint = *nearestSegm;
	}
	else
	{
		m_CutPoint= Project2Segm<R>(m_Point, *nearestSegm, *nearestSegmEnd);
		m_InArc   = (nearestSegm != arcBegin || m_CutPoint != *arcBegin) 
				 && (nearestSegm != i-1      || m_CutPoint != *i);
	}
	m_InSegm  = m_InArc && (*nearestSegmEnd != m_CutPoint);
	m_FoundAny = true;

	assert(m_InArc
		?	arcBegin + m_SegmIndex +  (m_InSegm ? 1 : 2) < arcEnd
		:	(m_SegmIndex == 0 || arcBegin + m_SegmIndex + 2 == arcEnd)
	);
	return true;
}

// *****************************************************************************
//									ArcProjectionHandleWithDist
// *****************************************************************************

template <typename T> inline
T SafeBet(T val)
{
	assert(val > 0);
	T result = val * 1.001;
	assert(val < result);
	return result;
}

template <typename R, typename T>
struct ArcProjectionHandleWithDist : ArcProjectionHandle<R, T>
{
	R m_Dist = R();

	ArcProjectionHandleWithDist() {}

	ArcProjectionHandleWithDist(typename ArcProjectionHandleWithDist::PointType p, typename ArcProjectionHandleWithDist::sqrdist_type minSqrDist)
		:	ArcProjectionHandle<R, T>(p, minSqrDist)
		,	m_Dist(CalcDist())
	{}

	bool Project2Arc(typename ArcProjectionHandleWithDist::ConstPointPtr arcBegin, typename ArcProjectionHandleWithDist::ConstPointPtr arcEnd)
	{
		if (!ArcProjectionHandle<R, T>::Project2Arc(arcBegin, arcEnd))
			return false;
		m_Dist = sqrt(this->m_MinSqrDist);
		return true;
	}

private:
	R CalcDist() const {
		return SafeBet(sqrt(this->m_MinSqrDist));
	}

	bool MakeSafeMin(typename ArcProjectionHandleWithDist::sqrdist_type newDist)
	{
		if (ArcProjectionHandle::MakeSafeMin(newDist))
			m_Dist = CalcDist();
	}

};

#endif // __GEO_GEODIST_H


