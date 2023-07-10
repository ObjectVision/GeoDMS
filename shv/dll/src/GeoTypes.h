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
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//
#pragma once

#if !defined(__SHV_GEOTYPES_H)
#define __SHV_GEOTYPES_H


#include "ShvBase.h"

#include "geo/Round.h"

//----------------------------------------------------------------------
// section : TPoint & TRect
//----------------------------------------------------------------------

typedef LONG GType;

struct GPoint : POINT
{
	GPoint() { x = -1; y = -1; }
	GPoint(GType _x, GType _y) { x = _x, y = _y; }

	GPoint ScreenToClient(HWND hWnd) const;

	void operator -=(POINT rhs)       { x -= rhs.x; y-=rhs.y; }
	void operator +=(POINT rhs)       { x += rhs.x; y+=rhs.y; }
	void operator *=(DPoint scaleFactor) {
		x = x * scaleFactor.first;
		y = y * scaleFactor.second;
	}
	void operator /=(DPoint scaleFactor) {
		x = x / scaleFactor.first;
		y = y / scaleFactor.second;
	}

	bool operator ==(POINT rhs) const { return x == rhs.x && y == rhs.y; }
	bool operator !=(POINT rhs) const { return x != rhs.x || y != rhs.y; }
	bool IsSingular() const { return !x || !y; }
};

#if defined(DMS_TM_HAS_UINT64_AS_DOMAIN)
typedef Int64 TType;
#else
typedef Int32 TType;
#endif //defined(DMS_TM_HAS_UINT64_AS_DOMAIN)

inline GPoint operator + (GPoint a, POINT b) { a += b; return a; }
inline GPoint operator - (GPoint a, POINT b) { a -= b; return a; }
inline GPoint operator - (POINT b) { return GPoint(-b.x, -b.y); }


struct TPoint : Point<TType> //: POINT
{
	TPoint() : Point(-1, -1) {}
	TPoint(TType x, TType y) : Point(x, y) {}

	explicit TPoint(const GPoint& src): Point(src.x, src.y) {}

	TType x() const { return first;  }
	TType y() const { return second; }
	TType& x() { return first;  }
	TType& y() { return second; }

	void operator -=(TPoint rhs)       { first -= rhs.first; second -= rhs.second; }
	void operator +=(TPoint rhs)       { first += rhs.first; second += rhs.second; }
	void operator *=(TPoint rhs)       { first *= rhs.first; second *= rhs.second; }

	bool operator ==(TPoint rhs) const { return first == rhs.first && second == rhs.second; }
	bool operator !=(TPoint rhs) const { return first != rhs.first || second != rhs.second; }
	bool IsSingular() const { return !first || !second; }
};

inline GPoint UpperBound(POINT lhs, POINT rhs) 
{
	return GPoint(Max<GType>(lhs.x, rhs.x), Max<GType>(lhs.y, rhs.y));
}

inline GPoint LowerBound(POINT lhs, POINT rhs) 
{
	return GPoint(Min<GType>(lhs.x, rhs.x), Min<GType>(lhs.y, rhs.y));
}

inline TPoint UpperBound(TPoint lhs, TPoint rhs) 
{
	return TPoint(Max<TType>(lhs.first, rhs.first), Max<TType>(lhs.second, rhs.second));
}

inline TPoint LowerBound(TPoint lhs, TPoint rhs) 
{
	return TPoint(Min<TType>(lhs.first, rhs.first), Min<TType>(lhs.second, rhs.second));
}

inline void MakeUpperBound(POINT& lhs, POINT rhs) 
{
	MakeMax(lhs.x, rhs.x);
	MakeMax(lhs.y, rhs.y);
}

inline void MakeLowerBound(POINT& lhs, POINT rhs) 
{
	MakeMin(lhs.x, rhs.x);
	MakeMin(lhs.y, rhs.y);
}

inline TPoint operator + (TPoint a, TPoint b) { a += b; return a; }
inline TPoint operator * (TPoint a, TPoint b) { a *= b; return a; }
inline TPoint operator - (TPoint a, TPoint b) { a -= b; return a; }
inline TPoint operator - (TPoint b) { return TPoint(-b.first, -b.second); }

inline bool IsLowerBound(POINT a, POINT b)
{ 
	return IsLowerBound(DMS_LONG(a.x), DMS_LONG(b.x)) && IsLowerBound(DMS_LONG(a.y), DMS_LONG(b.y));
}

inline bool IsStrictlyLower(POINT a, POINT b)
{ 
	return IsStrictlyLower(DMS_LONG(a.x), DMS_LONG(b.x)) && IsStrictlyLower(DMS_LONG(a.y), DMS_LONG(b.y));
}

inline TPoint ConcatVertical(TPoint a, TPoint b)
{
	return TPoint(Max<TType>(a.first, b.first), a.second + b.second);
}

inline TPoint ConcatHorizontal(TPoint a, TPoint b)
{
	return TPoint(a.first+b.first, Max<TType>(a.second, b.second));
}


struct GRect : RECT
{
	GRect() { left = top = right = bottom = -1; }

	GRect(GType _left, GType _top, GType _right, GType _bottom)
	{
		left   = _left;
		top    = _top;
		right  = _right;
		bottom = _bottom;
	}
	GRect(GPoint topLeft, GPoint bottomRight)
	{
		left   = topLeft.x;
		top    = topLeft.y;
		right  = bottomRight.x;
		bottom = bottomRight.y;
	}

	GRect(const RECT& src) : RECT(src) {}

	bool empty() const { return left >= right || top >= bottom; }
	const GType&  Left  () const { return left; }
	const GType&  Top   () const { return top; }
	const GType&  Right () const { return right; }
	const GType&  Bottom() const { return bottom; }
	GType&  Left  () { return left; }
	GType&  Top   () { return top; }
	GType&  Right () { return right; }
	GType&  Bottom() { return bottom; }
	GPoint TopLeft    () const { return GPoint(left,  top   ); }
	GPoint BottomRight() const { return GPoint(right, bottom); }
	GPoint TopRight   () const { return GPoint(right, top   ); }
	GPoint BottomLeft () const { return GPoint(left,  bottom); }
	GPoint Size  () const { return BottomRight() - TopLeft(); }
	GType  Width () const { return right - left; }
	GType  Height() const { return bottom - top; }

	bool operator ==(const RECT& rhs) const
	{
		return left   == rhs.left 
			&& right  == rhs.right
			&& top    == rhs.top
			&& bottom == rhs.bottom;
	}
	bool operator !=(const RECT& rhs) const
	{
		return !this->operator ==(rhs);
	}
	void operator &=(const RECT& rhs) 
	{
		MakeMax(left,   rhs.left);
		MakeMax(top,    rhs.top);
		MakeMin(right,  rhs.right);
		MakeMin(bottom, rhs.bottom);
	}
	void operator |=(const RECT& rhs) 
	{
		MakeMin(left,   rhs.left);
		MakeMin(top,    rhs.top);
		MakeMax(right,  rhs.right);
		MakeMax(bottom, rhs.bottom);
	}
	void operator +=(const POINT& delta) 
	{
		left  += delta.x;
		right += delta.x;
		top   += delta.y;
		bottom+= delta.y;
	}

	void operator -=(const POINT& delta) 
	{
		left  -= delta.x;
		right -= delta.x;
		top   -= delta.y;
		bottom-= delta.y;
	}
	void operator +=(const RECT& rhs) 
	{
		left  += rhs.left;
		right += rhs.right;
		top   += rhs.top;
		bottom+= rhs.bottom;
	}

	void operator -=(const RECT& rhs) 
	{
		left  -= rhs.left;
		right -= rhs.right;
		top   -= rhs.top;
		bottom-= rhs.bottom;
	}
	void operator *= (Point<Float64> scaleFactor)
	{
		left  *= scaleFactor.first;
		right *= scaleFactor.first;
		top *= scaleFactor.second;
		bottom *= scaleFactor.second;
	}
	void operator /= (Point<Float64> scaleFactor)
	{
		left /= scaleFactor.first;
		right /= scaleFactor.first;
		top /= scaleFactor.second;
		bottom /= scaleFactor.second;
	}
	void Expand(POINT delta)
	{
		dms_assert(delta.x >= 0);
		dms_assert(delta.y >= 0);

		left  -= delta.x;
		top   -= delta.y;
		right += delta.x;
		bottom+= delta.y;
	}
	void Expand(GType delta) 
	{
		dms_assert(delta >= 0);

		left  -= delta;
		top   -= delta;
		right += delta;
		bottom+= delta;
	}
	void Shrink(POINT delta) 
	{
		dms_assert(delta.x >= 0);
		dms_assert(delta.y >= 0);

		left  += delta.x;
		top   += delta.y;
		right -= delta.x;
		bottom-= delta.y;
	}
	void Shrink(GType delta) 
	{
		dms_assert(delta >= 0);

		left  += delta;
		top   += delta;
		right -= delta;
		bottom-= delta;
	}
};

inline GRect operator + (GRect a, POINT b) { a += b; return a; }
inline GRect operator - (GRect a, POINT b) { a -= b; return a; }
inline GRect operator + (GRect a, RECT  b) { a += b; return a; }
inline GRect operator - (GRect a, RECT  b) { a -= b; return a; }
inline GRect operator & (GRect a, RECT  b) { a &= b; return a; }

inline bool IsIncluding(GRect a, POINT p)
{
	return IsLowerBound(a.TopLeft(), p) && IsStrictlyLower(p, a.BottomRight());
}

inline bool IsIncluding(GRect a, GRect b)
{
	return IsLowerBound(a.TopLeft(), b.TopLeft()) && IsLowerBound(b.BottomRight(), a.BottomRight());
}

inline bool IsIntersecting(GRect a, GRect b)
{
	return IsStrictlyLower(b.TopLeft(), a.BottomRight()) 
		&& IsStrictlyLower(a.TopLeft(), b.BottomRight());
}

struct TRect : Range<TPoint>
{
	TRect() : Range(TPoint(), TPoint()) {}
	TRect(TType _left, TType _top, TType _right, TType _bottom) : Range(TPoint(_left, _top), TPoint(_right, _bottom)) {}
	TRect(const TPoint& topLeft, const TPoint& bottomRight)     : Range(topLeft, bottomRight) {}
	TRect(const Range& src): Range(src.first, src.second) {}
	explicit TRect(const GRect& src): Range(TPoint(src.TopLeft()), TPoint(src.BottomRight())) {}

	TType  Left  () const { return TopLeft().x(); }
	TType  Top   () const { return TopLeft().y(); }
	TType  Right () const { return BottomRight().x(); }
	TType  Bottom() const { return BottomRight().y(); }
	TType&  Left  () { return TopLeft().x(); }
	TType&  Top   () { return TopLeft().y(); }
	TType&  Right () { return BottomRight().x(); }
	TType&  Bottom() { return BottomRight().y(); }
	TPoint  TopLeft      () const { return first; }
	TPoint  BottomRight  () const { return second; }
	TPoint& TopLeft      () { return first; }
	TPoint& BottomRight  () { return second; }
	TPoint TopRight     () const { return TPoint(Right(), Top()); }
	TPoint BottomLeft   () const { return TPoint(Left(),  Bottom()); }
	TPoint Size  () const { return BottomRight() - TopLeft(); }
	TType  Width () const { return Right () - Left(); }
	TType  Height() const { return Bottom() - Top (); }

	void operator |=(const TRect& rhs) 
	{
		MakeLowerBound(first,  rhs.first );
		MakeUpperBound(second, rhs.second);
	}
	void Shrink(const TPoint& delta) 
	{
		first  += delta;
		second -= delta;
	}
	TRect& Expand(Int32 delta) 
	{
		Left  () -= delta;
		Right () += delta;
		Top   () -= delta;
		Bottom() += delta;
		return *this;
	}
	TRect& Shrink(Int32 delta) 
	{
		Left  () += delta;
		Right () -= delta;
		Top   () += delta;
		Bottom() -= delta;
		return *this;
	}
	Range<TType> HorRange() const { return Range<TType>(Left(), Right () ); }
	Range<TType> VerRange() const { return Range<TType>(Top (), Bottom() ); }
};

inline void operator &=(GRect& clipRect, const TRect& rhs)
{
	if (clipRect.left   < rhs.Left  ()) clipRect.left   = rhs.Left  ();
	if (clipRect.right  > rhs.Right ()) clipRect.right  = rhs.Right ();
	if (clipRect.top    < rhs.Top   ()) clipRect.top    = rhs.Top   ();
	if (clipRect.bottom > rhs.Bottom()) clipRect.bottom = rhs.Bottom();
}

inline GType  TType2GType  (TType   src) { return src < MinValue<GType>() ? MinValue<GType>() : (src > MaxValue<GType>() ? MaxValue<GType>() : src); }
inline GPoint TPoint2GPoint(const TPoint& src) { return GPoint(TType2GType  (src.first), TType2GType  (src.second)); }
inline GRect  TRect2GRect  (const TRect & src) { return GRect (TPoint2GPoint(src.first), TPoint2GPoint(src.second)); }

template <typename T> inline GPoint Point2GPoint(Point<T> src) { return GPoint(src.X(), src.Y()); }
template <typename T> inline TPoint Point2TPoint(Point<T> src) { return TPoint(src.X(), src.Y()); }
template <typename P> inline GRect  Rect2GRect  (Range<P> src) { return GRect(Point2GPoint(src.first), Point2GPoint(src.second)); }
template <typename P> inline TRect  Rect2TRect  (Range<P> src) { return TRect(Point2TPoint(src.first), Point2TPoint(src.second)); }


inline GPoint DPoint2GPoint(const DPoint& src) { IPoint tmp = Round<4>(src); return Point2GPoint(tmp); }
inline GRect  DRect2GRect(const DRect & src) 
{ 
	Range<IPoint> tmp = RoundEnclosing<4>(src);
	return Rect2GRect(tmp);
}

inline TRect  DRect2TRect(const DRect & src) 
{ 
	Range<Point<Int64> > tmp = RoundEnclosing<8>(src);
	return Rect2TRect(tmp);
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
GPoint Convert4(const Point<T>& pnt, const GPoint*, const ExceptFunc* ef, const ConvertFunc*)
{
	using scalar_result_type = typename ConvertFunc::template rebind<TType>::type;
	return GPoint(
		Convert4<GType>(pnt.Col(), TYPEID(T), ef, TYPEID(scalar_result_type))
	,	Convert4<GType>(pnt.Row(), TYPEID(T), ef, TYPEID(scalar_result_type))
	);
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
TPoint Convert4(const Point<T>& pnt, const TPoint*, const ExceptFunc* ef, const ConvertFunc*)
{
	using scalar_result_type = typename ConvertFunc::template rebind<TType>::type;
	return TPoint(
		Convert4<TType>(pnt.Col(), TYPEID(T), ef, TYPEID(scalar_result_type))
	,	Convert4<TType>(pnt.Row(), TYPEID(T), ef, TYPEID(scalar_result_type))
	);
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
GRect Convert4(const Range<T>& rect, const GRect*, const ExceptFunc* ef, const ConvertFunc*)
{
	using round_dn_type = typename ConvertFunc::template RoundDnFunc<TPoint>::type;
	using round_up_type = typename ConvertFunc::template RoundUpFunc<TPoint>::type;
	return GRect(
		Convert4(rect.first , TYPEID(GPoint), ef, TYPEID(round_dn_type)),
		Convert4(rect.second, TYPEID(GPoint), ef, TYPEID(round_up_type))
	);
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
TRect Convert4(const Range<T>& rect, const TRect*, const ExceptFunc* ef, const ConvertFunc*)
{
	using round_dn_type = typename ConvertFunc::template RoundDnFunc<TPoint>::type;
	using round_up_type = typename ConvertFunc::template RoundUpFunc<TPoint>::type;
	return TRect(
		Convert4(rect.first , TYPEID(TPoint), ef, TYPEID(round_dn_type))
	,	Convert4(rect.second, TYPEID(TPoint), ef, TYPEID(round_up_type))
	);
}

inline GPoint UndefinedValue(const GPoint*)   { return GPoint(-1, -1); }
inline TPoint UndefinedValue(const TPoint*)   { return TPoint(-1, -1); }
inline bool   IsDefined     (const GPoint& p) { return p.x != -1 || p.y != -1; }
inline bool   IsDefined     (const TPoint& p) { return p.first != -1 || p.second != -1; }


template <typename T, typename ExceptFunc, typename ConvertFunc>
Point<T> Convert4(const GPoint& pnt, const Point<T>*, const ExceptFunc* ef, const ConvertFunc*)
{
	using scalar_result_type = typename ConvertFunc::template rebind<T>::type;
#if defined(DMS_POINT_ROWCOL)
	return Point<T>(
		Convert4(pnt.y, TYPEID(T), ef, TYPEID(scalar_result_type))
	,	Convert4(pnt.x, TYPEID(T), ef, TYPEID(scalar_result_type))
	);
#else
	return Point<T>(
		Convert4(pnt.x, TYPEID(T), ef, TYPEID(scalar_result_type))
	,	Convert4(pnt.y, TYPEID(T), ef, TYPEID(scalar_result_type))
	);
#endif
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
Point<T> Convert4(const TPoint& pnt, const Point<T>*, const ExceptFunc* ef, const ConvertFunc*)
{
	using scalar_result_type = typename ConvertFunc::template rebind<T>::type;
#if defined(DMS_POINT_ROWCOL)
	return Point<T>(
		Convert4(pnt.second, TYPEID(T), ef, TYPEID(scalar_result_type))
	,	Convert4(pnt.first , TYPEID(T), ef, TYPEID(scalar_result_type))
	);
#else
	return Point<T>(
		Convert4(pnt.first , TYPEID(T), ef, TYPEID(scalar_result_type))
	,	Convert4(pnt.second, TYPEID(T), ef, TYPEID(scalar_result_type))
	);
#endif
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
Range<T> Convert4(const GRect& rect, const Range<T>*, const ExceptFunc* ef, const ConvertFunc* cf)
{
	using point_result_type = typename ConvertFunc::template rebind<T>::type;
	return Range<T>(
		Convert4(rect.TopLeft()    , TYPEID(T), ef, TYPEID(point_result_type)),
		Convert4(rect.BottomRight(), TYPEID(T), ef, TYPEID(point_result_type))
	);
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
Range<T> Convert4(const TRect& rect, const Range<T>*, const ExceptFunc* ef, const ConvertFunc* cf)
{
	using point_result_type = typename ConvertFunc::template rebind<T>::type;
	return Range<T>(
		Convert4(rect.first , TYPEID(T), ef, TYPEID(point_result_type)),
		Convert4(rect.second, TYPEID(T), ef, TYPEID(point_result_type))
	);
}

inline LONG _Width (const RECT& r) { return r.right  - r.left; }
inline LONG _Height(const RECT& r) { return r.bottom - r.top;  }

//----------------------------------------------------------------------
// section : Streaming of TPoint, TRect
//----------------------------------------------------------------------

FormattedOutStream& operator <<(FormattedOutStream& os, const GRect&  rect );
FormattedOutStream& operator <<(FormattedOutStream& os, const GPoint& point);

//REMOVE FormattedOutStream& operator <<(FormattedOutStream& os, const TRect&  rect );
FormattedOutStream& operator <<(FormattedOutStream& os, const TPoint& point);

//----------------------------------------------------------------------
// section : RGBQUAD
//----------------------------------------------------------------------

#include "geo/Color.h"

template<typename ExceptFunc, typename ConvertFunc>
inline RGBQUAD Convert4(DmsColor dmsColor, const RGBQUAD*, const ExceptFunc*, const ConvertFunc*)
{
	RGBQUAD result = {
		GetBlue (dmsColor),
		GetGreen(dmsColor),
		GetRed  (dmsColor),
		GetTrans(dmsColor),
	};
	return result;
}

template<typename ExceptFunc, typename ConvertFunc>
inline DmsColor Convert4(RGBQUAD quad, const DmsColor*, const ExceptFunc*, const ConvertFunc*)
{
	return (quad.rgbReserved << 24)
		|  (quad.rgbBlue     << 16)
		|  (quad.rgbGreen    <<  8)
		|  (quad.rgbRed);
}

inline COLORREF DmsColor2COLORREF(DmsColor dmsColor) { return dmsColor; }
inline DmsColor COLORREF2DmsColor(COLORREF colorref) { return colorref; }
const COLORREF TRANSPARENT_COLORREF = 0xFFFFFFFF;


#endif // !defined(__SHV_GEOTYPES_H)
