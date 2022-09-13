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

/***************************************************************************
 *
 * A Transformation is used in a draw_context to
 * transform from world to device coords.
 * Multiple Transformation can be combined.
/****************************************************************************/

#ifndef __RTC_GEO_TRANSFORM_H
#define __RTC_GEO_TRANSFORM_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/Range.h"
#include "geo/Point.h"

[[noreturn]] RTC_CALL void IllegalSingularity();

enum class OrientationType
{
	Default    = 0,
	LeftRight  = 0,
	RightLeft  = 1,
	TopBottom  = 0,
	BottomTop  = 2,
	NegateX    = 1,
	NegateY    = 2,
};

inline bool IsRightLeft(OrientationType orientation) { return int(orientation) & int(OrientationType::RightLeft); }
inline bool IsBottomTop(OrientationType orientation) { return int(orientation) & int(OrientationType::BottomTop); }
inline bool MustNegateX(OrientationType orientation) { return IsRightLeft(orientation); }
inline bool MustNegateY(OrientationType orientation) { return IsBottomTop(orientation); }

template <typename T>
Point<T> GetTopLeft(const Range<Point<T>>& extents, OrientationType orientation)
{
	return
		shp2dms_order(
			IsRightLeft(orientation) ? extents.second.X() : extents.first.X()
		,	IsBottomTop(orientation) ? extents.second.Y() : extents.first.Y()
		);
}

//----------------------------------------------------------------------
// class  : Transformation : an Affine Group over the field Point<T> which is T^2
//----------------------------------------------------------------------

template <class T>
class Transformation  // p' = fp + c
{
public:
	typedef T                              scalar_type;
	typedef Point<scalar_type>             point_type;
	typedef Range<point_type>              rect_type;
	typedef typename product_type<T>::type area_type;
// 3 ctors
  	Transformation() : m_Factor(1, 1), m_Offset(point_type(0, 0)) {}
  	Transformation(const point_type& pageBase, const point_type factor)
    	:	m_Factor(factor), m_Offset(pageBase)	
	{
		CheckSingularity();
	}
  	Transformation(const point_type& pageBase, const point_type factor, const point_type& worldBase)
    	:	m_Factor(factor), m_Offset(pageBase - worldBase*factor )
	{
		CheckSingularity();
	}

	Transformation(const rect_type& roi, const rect_type& viewPort, OrientationType orientation)
	{
		point_type sizeROI = Size(roi);
		point_type sizeVP  = Size(viewPort);
		if (sizeROI == point_type(0, 0))
			sizeROI = sizeVP;
		dms_assert( !(sizeROI == point_type(0, 0)) );
		T f = (sizeVP.first * sizeROI.second < sizeVP.second* sizeROI.first || (sizeROI.second == 0))
				?	(sizeVP.first / sizeROI.first)
				:	(sizeVP.second/ sizeROI.second);
		m_Factor = point_type(f, f);
		if (MustNegateY(orientation)) m_Factor.Row()= -m_Factor.Row();
		if (MustNegateX(orientation)) m_Factor.Col()= -m_Factor.Col();

		m_Offset  = Center(viewPort) - Center(roi)*m_Factor;
		CheckSingularity();
	}

	Transformation Inverse() const
	{
		return Transformation() / (*this);
	}

//	Scale operator to transform from world size to page size
	template <typename F> Point<F> PageScale (const Point<F>& p) const { return p * m_Factor; }
	template <typename F> Point<F> WorldScale(const Point<F>& p) const { dms_assert(!IsSingular()); return p / m_Factor; }
	template <typename P> Range<P> PageScale (const Range<P>& r) const { return Range<P>(PageScale (r.first), PageScale (r.second));}
	template <typename P> Range<P> WorldScale(const Range<P>& r) const { return Range<P>(WorldScale(r.first), WorldScale(r.second));}

//	Apply operator to transform from world space to page space
	template <typename F> Point<F> Apply  (const Point<F>& p) const { return DPoint(p) * m_Factor + m_Offset;   }
	template <typename F> Point<F> Reverse(const Point<F>& p) const { dms_assert(!IsSingular()); return (DPoint(p) - m_Offset) / m_Factor; }
	template <typename P> Range<P> Apply  (const Range<P>& r) const { return Range<P>(Apply  (r.first), Apply  (r.second));}
	template <typename P> Range<P> Reverse(const Range<P>& r) const { return Range<P>(Reverse(r.first), Reverse(r.second));}

//	Inplace Apply operators (that modify their arguments)
	template <typename F> void  InplApply  (Point<F>& p) const { p *= m_Factor; p += m_Offset;   }
	template <typename F> void  InplReverse(Point<F>& p) const { dms_assert(!IsSingular()); p -= m_Offset; p /= m_Factor; }
	template <typename P> void  InplApply  (Range<P>& r) const { InplApply  (r.first); InplApply  (r.second); MakeBounds(r.first, r.second); }
	template <typename P >void  InplReverse(Range<P>& r) const { InplReverse(r.first); InplReverse(r.second); MakeBounds(r.first, r.second); }

	//	Combine two transformations to a new Transformation: [f*g](x) := g(f(x)), where f = *this
	Transformation operator *(const Transformation& g) const { return Transformation(g.Apply(m_Offset), m_Factor*g.m_Factor); }

	// Combine this with post applyting then invertion of g: [f/g](x) := g^-1(f(x))
	Transformation operator /(const Transformation& g) const { return Transformation(g.Reverse(m_Offset), m_Factor / g.m_Factor); }

	Transformation operator +(const point_type& viewOffset) const { return Transformation(m_Offset+viewOffset, m_Factor); }
	Transformation operator -(const point_type& viewOffset) const { return Transformation(m_Offset-viewOffset, m_Factor); }

	// Change this transformation x->f(x) to x->g(f(x)), thus f := (g o f), thus include applying g after this
	void operator *=(const Transformation& g)
	{ 
		m_Offset *= g.m_Factor;
		m_Offset += g.m_Offset;
		m_Factor *= g.m_Factor; 
	} 
	// Change this transformation x->f(x) to x->f(sx)
	void operator *=(const point_type& scalePair)
	{ 
		m_Factor *= scalePair;
	} 
	void operator *=(scalar_type scaleFactor)
	{ 
		m_Factor *= scaleFactor;
	} 

	void operator /=(const Transformation& g)
	{ 
		m_Offset -= g.m_Offset;
		m_Offset /= g.m_Factor;
		m_Factor /= g.m_Factor; 
	} 
	// Change this transformation x->f(x) to x->f(x/s)
	void operator /=(const point_type& scalePair)
	{ 
		m_Factor /= scalePair;
	} 
	void operator /=(scalar_type scaleFactor)
	{ 
		m_Factor /= scaleFactor;
	} 

	// Change this transformation x->f(x) to x->f(x)+d
	void operator +=(const point_type& viewOffset)
	{ 
		m_Offset += viewOffset;
	} 
	void operator -=(const point_type& viewOffset)
	{ 
		m_Offset -= viewOffset;
	} 

	bool operator == (const Transformation& rhs) const { return m_Factor == rhs.Factor() && m_Offset == rhs.Offset(); }

	const point_type& Factor()      const { return m_Factor; }
	area_type         FactorProd()  const { return m_Factor.first * m_Factor.second; }
	bool              IsSingular()  const { return m_Factor.first == 0 || m_Factor.second == 0; }
	Float64           ZoomLevel()   const { return Abs(m_Factor.first); }
	point_type        V2WFactor()   const { dms_assert(!IsSingular()); return point_type(1.0 / m_Factor.first, 1.0 / m_Factor.second); }
	const point_type& Offset()      const { return m_Offset; }
	OrientationType   Orientation() const 
	{
		return OrientationType(
			int((m_Factor.Y()>=0) ? OrientationType::Default : OrientationType::NegateY)
		+	int((m_Factor.X()>=0) ? OrientationType::Default : OrientationType::NegateX)
		); 
	}
	bool IsNonScaling() const { return m_Factor.X() == 1 && m_Factor.Y() == 1; }
	bool IsLinear    () const { return m_Offset.X() == 0 && m_Offset.Y() == 0; }
	bool IsIdentity  () const { return IsNonScaling() && IsLinear(); }
	bool operator < (const Transformation& rhs) const { return m_Offset < rhs.m_Offset || (!(rhs.m_Offset < m_Offset) && m_Factor < rhs.m_Factor); }

private:
	void CheckSingularity()
	{
		if (m_Factor.first == 0 || m_Factor.second == 0)
			IllegalSingularity();
	}
	point_type m_Offset, m_Factor;
};

template <class T> inline
FormattedOutStream& operator << (FormattedOutStream& os, const Transformation<T>& t)
{
	os << "[ *" << t.Factor() << " + " << t.Offset() << "]";
	return os;
}



typedef Float64                 CrdType;
typedef Point<CrdType>          CrdPoint;
typedef Range<CrdPoint>         CrdRect;
typedef Transformation<CrdType> CrdTransformation;
inline CrdRect GetDefaultCrdRect() { return CrdRect(0.001*MinValue<CrdPoint>(), 0.001*MaxValue<CrdPoint>()); }

#endif // __RTC_GEO_TRANSFORM_H


