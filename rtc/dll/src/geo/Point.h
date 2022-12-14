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

#if !defined(__RTC_GEO_POINT_H)
#define __RTC_GEO_POINT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/CheckedCalc.h"
#include "geo/Couple.h"
#include "geo/ElemTraits.h"

#define DMS_POINT_ROWCOL

//----------------------------------------------------------------------
// class  : Point<T>
//----------------------------------------------------------------------

template <class T>
struct Point: Couple<T>
{
	typedef T        field_type;
	using Couple<T>::first;
	using Couple<T>::second;

//	Constructors (specified)
	Point() {}
	Point(T first, T second): Couple<T>(first, second) {}
	Point(Undefined): Couple<T>(Undefined()) {}
	template <typename U>
	Point(const Point<U>& rhs): Couple<T>(rhs.first, rhs.second) {}

	template <typename U>
	void operator =(const Point<U>& rhs) { first = rhs.first; second = rhs.second; }

#if defined(DMS_POINT_ROWCOL)
	const T& Row() const { return first;  }
	const T& Col() const { return second; }
	      T& Row()       { return first;  }
	      T& Col()       { return second; }

	const T& Y() const { return first;  }
	const T& X() const { return second; }
	      T& Y()       { return first;  }
	      T& X()       { return second; }
#else
	const T& Row() const { return second;  }
	const T& Col() const { return first; }
	      T& Row()       { return second;  }
	      T& Col()       { return first; }

	const T& X() const { return first;  }
	const T& Y() const { return second; }
	      T& X()       { return first;  }
	      T& Y()       { return second; }
#endif
};

//----------------------------------------------------------------------
// Main Section   PointBounds
//----------------------------------------------------------------------

#include "geo/BaseBounds.h"

//----------------------------------------------------------------------
// Section      : Undefined
//----------------------------------------------------------------------

template <class T>
inline Point<T> UndefinedValue(const Point<T>*)
{
  return Point<T>( Undefined() );
}

template <class T>
inline bool IsDefined(const Point<T>& x)
{
  return IsDefined(x.first) || IsDefined(x.second);
}


//----------------------------------------------------------------------
// Section      : MaxValue/MinValue
//----------------------------------------------------------------------
// Full name    : MaxValue/MinValue helper definitions
// Purpose      : encapsulation of <limits> or old-fashion limitations
//----------------------------------------------------------------------
// A Point serves the general concept of a cartesian product of 2 values
//----------------------------------------------------------------------

template<typename T> struct minmax_traits<Point<T> >
{
	static Point<T> MinValue() { return Point<T>(::MinValue<T>(), ::MinValue<T>()); }
	static Point<T> MaxValue() { return Point<T>(::MaxValue<T>(), ::MaxValue<T>()); } 
};

//----------------------------------------------------------------------
// Section      : LowerBound / UpperBound on Tuples operate element by element.
//----------------------------------------------------------------------
// Full name    : Aggregation helper definitioin
// Purpose      : fast operation for aggregation operations
//----------------------------------------------------------------------

template <class T>
inline bool
IsLowerBound(const Point<T>& a, const Point<T>& b) 
{ 
	return IsLowerBound(a.first, b.first) && IsLowerBound(a.second, b.second);
}

template <class T>
inline bool
IsUpperBound(const Point<T>& a, const Point<T>& b) 
{ 
	return IsUpperBound(a.first, b.first) && IsUpperBound(a.second, b.second);
}

template <class T>
inline Point<T> 
LowerBound(const Point<T>& a, const Point<T>& b) 
{ 
	return Point<T>(LowerBound(a.first, b.first), LowerBound(a.second, b.second));
}

template <class T>
inline Point<T> 
UpperBound(const Point<T>& a, const Point<T>& b) 
{ 
	return Point<T>(UpperBound(a.first, b.first), UpperBound(a.second, b.second));
}

template <class T>
inline void 
MakeLowerBound(Point<T>& a, const Point<T>& b) 
{ 
	MakeLowerBound(a.first,  b.first);
	MakeLowerBound(a.second, b.second);
}

template <class T>
inline void 
MakeUpperBound(Point<T>& a, const Point<T>& b) 
{ 
	MakeUpperBound(a.first,  b.first);
	MakeUpperBound(a.second, b.second);
}

template <class T>
inline void 
MakeBounds(Point<T>& a, Point<T>& b) 
{ 
	MakeBounds(a.first,  b.first);
	MakeBounds(a.second, b.second);
}

template <class T>
inline bool IsStrictlyLower(const Point<T>& a, const Point<T>& b)
{ 
	return IsStrictlyLower(a.first, b.first) && IsStrictlyLower(a.second, b.second); 
}

template <class T>
inline bool IsStrictlyGreater(const Point<T>& a, const Point<T>& b)
{ 
	return IsStrictlyGreater(a.first, b.first) && IsStrictlyGreater(a.second, b.second); 
}

template <class T>
void MakeStrictlyGreater(Point<T>& ref)
{
	MakeStrictlyGreater(ref.first);
	MakeStrictlyGreater(ref.second);
}

template <class T>
inline Float64 AsFloat64(const Point<T>& x )
{
	throwIllegalAbstract(MG_POS, "Point::AsFloat64");
}

//----------------------------------------------------------------------
// Section      : assignment operators
//----------------------------------------------------------------------
// Full name    : Aggregation helper definitioin
// Purpose      : fast operation for aggregation operations
//----------------------------------------------------------------------
// Similar operators are defined with macro's to promote similarity and clarity

#define DEFINE_ASSIGNMENT_OPERATOR(X)                                \
template <typename T> inline                                         \
const Point<T>&  operator X (Point<T>& self, const Point<T>& other)  \
{                                                                    \
	self.first  X other.first;                                       \
	self.second X other.second;                                      \
	return self;                                                     \
}                                                                    \
template <typename T> inline                                         \
const Point<T>& operator X(Point<T>& self, T other)                  \
{                                                                    \
	self.first X other;                                              \
	self.second X other;                                             \
	return self;                                                     \
}

DEFINE_ASSIGNMENT_OPERATOR(+=)
DEFINE_ASSIGNMENT_OPERATOR(-=)
DEFINE_ASSIGNMENT_OPERATOR(*=)
DEFINE_ASSIGNMENT_OPERATOR(/=)
DEFINE_ASSIGNMENT_OPERATOR(%=)
DEFINE_ASSIGNMENT_OPERATOR(^=)
DEFINE_ASSIGNMENT_OPERATOR(&=)
DEFINE_ASSIGNMENT_OPERATOR(|=)
DEFINE_ASSIGNMENT_OPERATOR(<<=)
DEFINE_ASSIGNMENT_OPERATOR(>>=)

#undef DEFINE_ASSIGNMENT_OPERATOR

//----------------------------------------------------------------------
// Section      : unary operators on Points
//----------------------------------------------------------------------

template <class T> inline                                    
Point<T> operator -(const Point<T>& arg)
{
	return Point<T>(-arg.first, -arg.second);
}

#define DEFINE_UNARY_FUNC(X)                              \
template <class T> inline                                 \
Point<typename aggr_type<T>::type> X(const Point<T>& arg) \
{                                                         \
	return Point<T>(X(arg.first), X(arg.second));         \
}

DEFINE_UNARY_FUNC(sin)
DEFINE_UNARY_FUNC(con)
DEFINE_UNARY_FUNC(tan)
DEFINE_UNARY_FUNC(sqrt)

#undef DEFINE_UNARY_FUNC


//----------------------------------------------------------------------
// Section      : binary operators on Tuples
//----------------------------------------------------------------------
// Similar operators are defined with macro's to promote similarity and clarity

#define DEFINE_BINARY_OPERATOR(X)                            \
template <typename T> inline                                 \
auto operator X(const Point<T>& a, const Point<T>& b)        \
{                                                            \
	return Point<T>(a.first X b.first, a.second X b.second); \
}                                                            \
template <class T> inline                                    \
Point<T> operator X(const Point<T>& a, const T& c)           \
{                                                            \
	return Point<T>(a.first X c, a.second X c);              \
}                                                            \
template <class T> inline                                    \
Point<T> operator X(const T& c, const Point<T>& b)           \
{                                                            \
	return Point<T>(c X b.first, c X b.second);              \
}

DEFINE_BINARY_OPERATOR(+)
DEFINE_BINARY_OPERATOR(-)
DEFINE_BINARY_OPERATOR(*)
DEFINE_BINARY_OPERATOR(/)
DEFINE_BINARY_OPERATOR(%)
DEFINE_BINARY_OPERATOR(^)
DEFINE_BINARY_OPERATOR(&)
DEFINE_BINARY_OPERATOR(|)
DEFINE_BINARY_OPERATOR(<<)
DEFINE_BINARY_OPERATOR(>>)
#undef DEFINE_BINARY_OPERATOR

template <typename T, typename U> inline         
std::enable_if_t<(sizeof(T)>=sizeof(U)) && (is_signed_v<T> != is_signed_v<U>), Point<signed_type_t<T>>>
operator +(const Point<T>& a, const Point<U>& b)
{
	return Point<T>(a.first + b.first, a.second + b.second);
}

template <typename T, typename U> inline
std::enable_if_t<(sizeof(T) > sizeof(U)) && (is_signed_v<T> == is_signed_v<U>), Point<T>>
operator +(const Point<T>& a, const Point<U>& b)
{
	return Point<T>(a.first + b.first, a.second + b.second);
}

template <typename T, typename U> inline
std::enable_if_t<(sizeof(T) >= sizeof(U)) && (is_signed_v<T> != is_signed_v<U>), Point<signed_type_t<T>>>
operator /(const Point<T>& a, const Point<U>& b)
{
	return Point<signed_type_t<T>>(a.first / b.first, a.second / b.second);
}

template <typename T, typename U> inline
std::enable_if_t<(sizeof(T) > sizeof(U)) && (is_signed_v<T> == is_signed_v<U>), Point<T>>
operator /(const Point<T>& a, const Point<U>& b)
{
	return Point<T>(a.first / b.first, a.second / b.second);
}


//----------------------------------------------------------------------
// Section      : Geometric operators
//----------------------------------------------------------------------


template <class T> inline
	SizeT Cardinality(const Point<T>& v) { return CheckedMul<SizeT>(Cardinality(v.first), Cardinality(v.second)); }

template <class T> inline
typename product_type<T>::type
Area(const Point<T>& v) 
{ 
	return v.first * v.second; 
}

//----------------------------------------------------------------------
// Section      : Distance Measures
//----------------------------------------------------------------------

template <typename ReturnType, typename T, typename U>
inline ReturnType InProduct(const Point<T>& p1, const Point<U>& p2)
{
	return
		ReturnType(p1.first ) * ReturnType(p2.first )
	+	ReturnType(p1.second) * ReturnType(p2.second);
}

template <typename ReturnType, typename U>
inline ReturnType OutProduct(const Point<U>& p1, const Point<U>& p2)
{
	return
		ReturnType(p1.Col()) * ReturnType(p2.Row())
	-	ReturnType(p1.Row()) * ReturnType(p2.Col());
}

template <typename ReturnType, typename U>
inline ReturnType Norm(const Point<U>& point)
{
	return InProduct<ReturnType>(point, point);
}

template <typename ReturnType, typename U>
inline ReturnType SqrDist(const Point<U>& p1, const Point<U>& p2)
{
	return Norm<ReturnType>(p1-p2);
}

#endif // __RTC_GEO_POINT_H
