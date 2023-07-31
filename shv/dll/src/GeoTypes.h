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
		x = Float64(x) * scaleFactor.first;
		y = Float64(y) * scaleFactor.second;
	}
	void operator /=(DPoint scaleFactor) {
		x = Float64(x) / scaleFactor.first;
		y = Float64(y) / scaleFactor.second;
	}

	bool operator ==(POINT rhs) const { return x == rhs.x && y == rhs.y; }
	bool operator !=(POINT rhs) const { return x != rhs.x || y != rhs.y; }
	bool IsSingular() const { return !x || !y; }
};

inline GType& get_x(GPoint& p) noexcept { return p.x; }
inline GType& get_y(GPoint& p) noexcept { return p.y; }
inline GType get_x(const GPoint& p) noexcept { return p.x; }
inline GType get_y(const GPoint& p) noexcept { return p.y; }

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
	TPoint(const Point<TType>& pnt) : Point<TType>(pnt.first, pnt.second) {}

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
	return Point<TType>(Max<TType>(lhs.first, rhs.first), Max<TType>(lhs.second, rhs.second));
}

inline TPoint LowerBound(TPoint lhs, TPoint rhs) 
{
	return Point<TType>(Min<TType>(lhs.first, rhs.first), Min<TType>(lhs.second, rhs.second));
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

//inline TPoint operator + (TPoint a, TPoint b) { a += b; return a; }
//inline TPoint operator * (TPoint a, TPoint b) { a *= b; return a; }
//inline TPoint operator - (TPoint a, TPoint b) { a -= b; return a; }
inline TPoint operator - (TPoint b) { return Point<TType>(-b.first, -b.second); }

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
	return shp2dms_order<TType>(Max<TType>(a.X(), b.X()), a.Y()+ b.Y());
}

inline TPoint ConcatHorizontal(TPoint a, TPoint b)
{
	return shp2dms_order<TType>(a.X() +b.X(), Max<TType>(a.Y(), b.Y()));
}


struct GRect : RECT
{
	GRect() { left = top = right = bottom = -1; }

	GRect(GType left_, GType top_, GType right_, GType bottom_)
	{
		left   = left_;
		top    = top_;
		right  = right_;
		bottom = bottom_;
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
	GPoint LeftTop    () const { return GPoint(left,  top   ); }
	GPoint RightBottom() const { return GPoint(right, bottom); }
	GPoint RightTop   () const { return GPoint(right, top   ); }
	GPoint LeftBottom () const { return GPoint(left,  bottom); }
	GPoint Size  () const { return RightBottom() - LeftTop(); }
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
		left   = Float64(left  ) * scaleFactor.first;
		right  = Float64(right ) * scaleFactor.first;
		top    = Float64(top   ) * scaleFactor.second;
		bottom = Float64(bottom) * scaleFactor.second;
	}
	void operator /= (Point<Float64> scaleFactor)
	{
		left   = Float64(left  ) / scaleFactor.first;
		right  = Float64(right ) / scaleFactor.first;
		top    = Float64(top   ) / scaleFactor.second;
		bottom = Float64(bottom) / scaleFactor.second;
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

template <typename  F>
inline Point<F> g2dms_order(GPoint p)
{
	return shp2dms_order<F>(p.x, p.y);
}

template <typename  F>
inline auto g2dms_order(GRect p)
{
	return Range<Point<F>>(g2dms_order<F>(p.LeftTop()), g2dms_order<F>(p.RightBottom()));
}

inline GRect operator + (GRect a, POINT b) { a += b; return a; }
inline GRect operator - (GRect a, POINT b) { a -= b; return a; }
inline GRect operator + (GRect a, RECT  b) { a += b; return a; }
inline GRect operator - (GRect a, RECT  b) { a -= b; return a; }
inline GRect operator & (GRect a, RECT  b) { a &= b; return a; }

inline bool IsIncluding(GRect a, POINT p)
{
	return IsLowerBound(a.LeftTop(), p) && IsStrictlyLower(p, a.RightBottom());
}

inline bool IsIncluding(GRect a, GRect b)
{
	return IsLowerBound(a.LeftTop(), b.LeftTop()) && IsLowerBound(b.RightBottom(), a.RightBottom());
}

inline bool IsIntersecting(GRect a, GRect b)
{
	return IsStrictlyLower(b.LeftTop(), a.RightBottom())
		&& IsStrictlyLower(a.LeftTop(), b.RightBottom());
}

struct TRect : Range<TPoint>
{
	TRect() : Range(TPoint(), TPoint()) {}
	TRect(TType left, TType top, TType right, TType bottom) : Range(shp2dms_order<TType>(left, top), shp2dms_order<TType>(right, bottom)) {}
	TRect(const TPoint& topLeft, const TPoint& bottomRight) : Range(topLeft, bottomRight) {}
	TRect(const Range& src): Range(src.first, src.second) {}

	TType  Left  () const { return TopLeft().X(); }
	TType  Top   () const { return TopLeft().Y(); }
	TType  Right () const { return BottomRight().X(); }
	TType  Bottom() const { return BottomRight().Y(); }
	TType&  Left  () { return TopLeft().X(); }
	TType&  Top   () { return TopLeft().Y(); }
	TType&  Right () { return BottomRight().X(); }
	TType&  Bottom() { return BottomRight().Y(); }
	TPoint  TopLeft      () const { return first; }
	TPoint  BottomRight  () const { return second; }
	TPoint& TopLeft      () { return first; }
	TPoint& BottomRight  () { return second; }
	TPoint TopRight     () const { return shp2dms_order<TType>(Right(), Top()); }
	TPoint BottomLeft   () const { return shp2dms_order<TType>(Left(),  Bottom()); }
	TPoint Size  () const { return BottomRight() - TopLeft(); }
	TType  Width () const { return Right () - Left(); }
	TType  Height() const { return Bottom() - Top (); }

	void operator |=(const Range<TPoint>& rhs) 
	{
		MakeLowerBound(first,  rhs.first );
		MakeUpperBound(second, rhs.second);
	}

	void Shrink(const TPoint& delta) 
	{
		first  += delta;
		second -= delta;
	}

	template <typename Factor>
	void operator *=(Factor f)
	{
		first  *= f;
		second *= f;
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

inline GType  TType2GType  (TType   src) { return src < MinValue<GType>() ? MinValue<GType>() : (src > MaxValue<GType>() ? MaxValue<GType>() : src); }
inline GPoint TPoint2GPoint(const TPoint& src, CrdPoint sf) { return GPoint(TType2GType  (src.X() * sf.first), TType2GType(src.Y() * sf.second)); }
inline GRect  TRect2GRect  (const TRect & src, CrdPoint sf) { return GRect (TPoint2GPoint(src.first, sf), TPoint2GPoint(src.second, sf)); }

GType TType2GType1(TType src, CrdType scale, CrdType slack);
std::pair<GType, CrdType> TType2GType2(TType src, CrdType scale, CrdType slack);

inline GPoint TPoint2GPoint1(TPoint src, CrdPoint sf, CrdPoint slack)
{
	auto xs = TType2GType1(src.X(), sf.first, slack.first);
	auto ys = TType2GType1(src.Y(), sf.second, slack.second);
	return {xs, ys};
}

inline GRect TRect2GRect(TRect src, CrdPoint sf, CrdPoint slack)
{
	return GRect(TPoint2GPoint1(src.first, sf, slack), TPoint2GPoint1(src.second, sf, slack));
}

inline std::pair<GPoint, CrdPoint>  TPoint2GPoint2(TPoint src, CrdPoint sf, CrdPoint slack)
{
	auto xs = TType2GType2(src.X(), sf.first, slack.first);
	auto ys = TType2GType2(src.Y(), sf.second, slack.second);
	return { {xs.first, ys.first},  {xs.second, ys.second} };
}

template <typename T> inline GPoint Point2GPoint(Point<T> src, CrdPoint sf) { return GPoint(src.X() * sf.first, src.Y() * sf.second); }
template <typename T> inline TPoint Point2TPoint(Point<T> src) { return Point<TType>(src.first, src.second); }
template <typename P> inline GRect  Rect2GRect  (Range<P> src, CrdPoint sf) { return GRect(Point2GPoint(src.first, sf), Point2GPoint(src.second, sf)); }
template <typename P> inline TRect  Rect2TRect  (Range<P> src) { return TRect(Point2TPoint(src.first), Point2TPoint(src.second)); }

inline GPoint DPoint2GPoint(DPoint src, const CrdTransformation& self) { self.InplApply(src); IPoint tmp = Round<4>(src); return GPoint(tmp.X(), tmp.Y()); }
inline GRect  DRect2GRect(DRect src, const CrdTransformation& self)
{ 
	self.InplApply(src);
	Range<IPoint> tmp = RoundEnclosing<4>(src);
	return GRect(GPoint(tmp.first.X(), tmp.first.Y()), GPoint(tmp.second.X(), tmp.second.Y()));
}

inline DPoint GPoint2DPoint(GPoint src, const CrdTransformation& self) { auto res = shp2dms_order<Float64>(src.x, src.y); self.InplReverse(res); return res; }
inline DRect  GRect2DRect(GRect src, const CrdTransformation& self)
{
	return { GPoint2DPoint(src.LeftTop(), self), GPoint2DPoint(src.RightBottom(), self) };
}

inline TRect DRect2TRect(const DRect& src)
{
	Range<Point<Int64> > tmp = RoundEnclosing<8>(src);
	return Rect2TRect(tmp);
}

inline GPoint TPoint2GPoint(TPoint p, const CrdTransformation& self) { auto dp = DPoint(p) * self.Factor() + self.Offset(); return GPoint(dp.X(), dp.Y()); }
inline GRect TRect2GRect(const TRect& r, const CrdTransformation& self) { return GRect(TPoint2GPoint(r.first, self), TPoint2GPoint(r.second, self)); }

template <typename T, typename ExceptFunc, typename ConvertFunc>
TPoint Convert4(const Point<T>& pnt, const TPoint*, const ExceptFunc* ef, const ConvertFunc*)
{
	using scalar_result_type = typename ConvertFunc::template rebind<TType>::type;
	return Point<TType>(
		Convert4<TType>(pnt.first , TYPEID(T), ef, TYPEID(scalar_result_type))
	,	Convert4<TType>(pnt.second, TYPEID(T), ef, TYPEID(scalar_result_type))
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
inline TPoint UndefinedValue(const TPoint*)   { return Point<TType>(-1, -1); }
inline bool   IsDefined     (const GPoint& p) { return p.x != -1 || p.y != -1; }
inline bool   IsDefined     (const TPoint& p) { return p.first != -1 || p.second != -1; }

template <typename T, typename ExceptFunc, typename ConvertFunc>
Point<T> Convert4(TPoint pnt, const Point<T>*, const ExceptFunc* ef, const ConvertFunc*)
{
	using scalar_result_type = typename ConvertFunc::template rebind<T>::type;
	return Point<T>(
		Convert4(pnt.first , TYPEID(T), ef, TYPEID(scalar_result_type))
	,	Convert4(pnt.second, TYPEID(T), ef, TYPEID(scalar_result_type))
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
