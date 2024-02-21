#pragma once

#if !defined(__RTC_GEO_POINT_H)
#define __RTC_GEO_POINT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/Couple.h"
#include "geo/ElemTraits.h"

#include "RtcGeneratedVersion.h"

#if DMS_VERSION_MAJOR < 15

#define DMS_POINT_ROWCOL

#endif

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
	constexpr Point() {} // default initialisastion results in valid possibly non-zero objects too
	constexpr Point(T first, T second): Couple<T>(first, second) {}
	constexpr Point(Undefined): Couple<T>(Undefined()) {}

	template <std::convertible_to<T> U>
	Point(const Point<U>& rhs): Couple<T>(rhs.first, rhs.second) {}

	template <std::convertible_to<T> U>
	void operator =(const Point<U>& rhs) { first = rhs.first; second = rhs.second; }

	template <std::convertible_to<T> U>
	void operator *=(Point<U> rhs) { first = first * rhs.first; second = second * rhs.second; }

	template <std::convertible_to<T> U>
	void operator *=(U rhs) { first = first * rhs; second = second * rhs; }

#if defined(DMS_POINT_ROWCOL)
	T Row() const { return first;  }
	T Col() const { return second; }
	T& Row()       { return first;  }
	T& Col()       { return second; }

	T Y() const { return first;  }
	T X() const { return second; }
	T& Y()       { return first;  }
	T& X()       { return second; }
#else
	T Row() const { return second;  }
	T Col() const { return first; }
	T& Row()       { return second;  }
	T& Col()       { return first; }

	T X() const { return first;  }
	T Y() const { return second; }
	T& X()       { return first;  }
	T& Y()       { return second; }
#endif
};

template<class T> inline auto get_x(Point<T>&& p) noexcept -> T { return p.X(); }
template<class T> inline auto get_y(Point<T>&& p) noexcept -> T { return p.Y(); }
template<class T> inline auto get_x(Point<T>& p) noexcept-> T& { return p.X(); }
template<class T> inline auto get_y(Point<T>& p) noexcept-> T& { return p.Y(); }

//----------------------------------------------------------------------
// Section      :	ordering
//----------------------------------------------------------------------

template <typename T>
bool operator < (const Point<T>& lhs, const Point<T>& rhs)
{
	return
		lhs.Row() < rhs.Row() ||
		(!(rhs.Row() < lhs.Row())
			&& lhs.Col() < rhs.Col()
			);
}

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
// Section      : Distance Measures
//----------------------------------------------------------------------

template <typename ReturnType, typename PU>
inline ReturnType InProduct(PU p1, PU p2)
{
	return
		ReturnType(get_x(p1)) * ReturnType(get_x(p2))
	+	ReturnType(get_y(p1)) * ReturnType(get_y(p2));
}

template <typename ReturnType, typename U>
inline ReturnType OutProduct(Point<U> p1, Point<U> p2)
{
	return
		ReturnType(p1.Col()) * ReturnType(p2.Row())
	-	ReturnType(p1.Row()) * ReturnType(p2.Col());
}

template <typename ReturnType, typename PU>
inline ReturnType Norm(PU point)
{
	return InProduct<ReturnType>(point, point);
}

template <typename ReturnType, typename PU>
inline ReturnType SqrDist(PU p1, PU p2)
{
	return Norm<ReturnType>(p1-p2);
}

template <typename T> struct is_fixed_size_element : is_numeric<T> {};
template <typename T> struct is_fixed_size_element<Point<T>> : is_fixed_size_element<T> {};
template <typename T> struct is_fixed_size_element<Range<T>> : is_fixed_size_element<T> {};

template <typename T> constexpr bool is_fixed_size_element_v = is_fixed_size_element<T>::value;
template <typename T> concept FixedSizeElement = is_fixed_size_element_v<T>;

template <typename T> struct is_point_type : std::false_type {};
template <typename T> struct is_point_type<Point<T>> :std::true_type {};

template <typename T> constexpr bool is_point_type_v = is_point_type<T>::value;
template <typename T> concept PointType = is_point_type_v<T>;

#endif // __RTC_GEO_POINT_H
