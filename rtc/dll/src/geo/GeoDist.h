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

#ifndef __GEO_GEODIST_H
#define __GEO_GEODIST_H

#include "geo/Conversions.h"

typedef UInt32 seq_elem_index_type;

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
	typedef const PointType* ConstPointPtr;
	typedef R                sqrdist_type;

	ConstPointPtr m_Point      = nullptr;
	sqrdist_type  m_MinSqrDist = 0;

	// result vars
	bool         m_FoundAny = false, m_InArc = false, m_InSegm = false;
	seq_elem_index_type m_SegmIndex = UNDEFINED_VALUE(seq_elem_index_type);
	PointType    m_CutPoint;
	
	ArcProjectionHandle() {}

	ArcProjectionHandle(ConstPointPtr p, sqrdist_type minSqrDist)
		:	m_Point(p)
		,	m_FoundAny(false)
		,	m_MinSqrDist(minSqrDist)
	{}

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

	if	( MakeSafeMin(Norm<sqrdist_type>(*m_Point - *arcBegin) ) )
		nearestSegm = arcBegin;

	for ( ; ++j != arcEnd ; i = j)
		if (MakeSafeMin( SqrDist2Segm<sqrdist_type>(*m_Point, *i, *j)) )
			nearestSegm = i;

	if (nearestSegm == nullptr)
		return false;

	dms_assert(i+1 ==arcEnd);

	m_SegmIndex = nearestSegm - arcBegin;
	ConstPointPtr nearestSegmEnd = nearestSegm+1;
	if (nearestSegmEnd == arcEnd)
	{
		dms_assert(nearestSegm == arcBegin);
		dms_assert(arcBegin + 1 == arcEnd);
		m_CutPoint= *nearestSegm;
		m_InArc   = false;
	}
	else
	{
		m_CutPoint= Project2Segm<R>(*m_Point, *nearestSegm, *nearestSegmEnd);
		m_InArc   = (nearestSegm != arcBegin || m_CutPoint != *arcBegin) 
				 && (nearestSegm != i-1      || m_CutPoint != *i);
	}
	m_InSegm  = m_InArc && (*nearestSegmEnd != m_CutPoint);
	m_FoundAny = true;

	dms_assert(m_InArc
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
	dms_assert(val > 0);
	T result = val * 1.001;
	dms_assert(val < result);
	return result;
}

template <typename R, typename T>
struct ArcProjectionHandleWithDist : ArcProjectionHandle<R, T>
{
	R m_Dist = R();

	ArcProjectionHandleWithDist() {}

	ArcProjectionHandleWithDist(typename ArcProjectionHandleWithDist::ConstPointPtr p, typename ArcProjectionHandleWithDist::sqrdist_type minSqrDist)
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


