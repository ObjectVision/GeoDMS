// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_GEO_PAIR_H)
#define __RTC_GEO_PAIR_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <utility>
#include "geo/ElemTraits.h"

//----------------------------------------------------------------------
// struct  : Pair<T, U>
//
// A Pair serves the general concept of a cartesian product of 2 types
//----------------------------------------------------------------------

template <class T, class U>
struct Pair : std::pair<T, U>
{
	using base_type = std::pair<T, U>;
	using base_type::first;
	using base_type::second;

//	Constructors (specified)
	Pair (): std::pair<T, U>(T(), U()) {}
	Pair (const T& row, const U& col): base_type(row, col) {}
	Pair(Undefined): base_type(Undefined(), Undefined()) {}

//	Other Operations (specified)
	const Pair& operator = (const Pair& rhs)
	{
		first  = rhs.first;
		second = rhs.second;
		return *this;
	}

	/* REMOVE
    bool operator < (const Pair& rhs) const
    {
		return std::operator < ((const base_type&)(*this), (const base_type&)(rhs));
    }
*/

	const T& First () const { return first;  }
	const U& Second() const { return second; }
	      T& First ()       { return first;  }
	      U& Second()       { return second; }
};

//----------------------------------------------------------------------
// Section      : MaxValue/MinValue
//----------------------------------------------------------------------
// Full name    : MaxValue/MinValue helper definitions
// Purpose      : encapsulation of <limits> or old-fashion limitations
//----------------------------------------------------------------------
//
// Operators are not defined as members to serve types
// that don's support all operator
// For Example:
//    Pair<VAR_STRING> will be OK
//    as long as the operator -= is not used.
//
//----------------------------------------------------------------------
// Section      : MaxValue/MinValue
//----------------------------------------------------------------------

template<typename T, typename U> 
struct minmax_traits<Pair<T, U> >
{
	static Pair<T, U> MinValue() { return Pair<T, U>(::MinValue<T>(), ::MinValue<U>()); }
	static Pair<T, U> MaxValue() { return Pair<T, U>(::MaxValue<T>(), ::MaxValue<U>()); } 
};

template <class T, class U>
inline Pair<T, U> UndefinedValue(const Pair<T, U>*)
{
	return Pair<T, U>(Undefined(), Undefined());
}

template <class T, class U>
inline void MakeUndefined(Pair<T, U>& v)
{
	MakeUndefined(v.first);
	MakeUndefined(v.second);
}

template <class T, class U>
inline void MakeUndefinedOrZero(Pair<T, U>& v)
{
	MakeUndefinedOrZero(v.first);
	MakeUndefinedOrZero(v.second);
}

template <class T, class U>
inline bool IsDefined(Pair<T, U> x)
{
  return IsDefined(x.first) || IsDefined(x.second);
}

template <class T, class U>
inline  void Assign(Pair<T, U>& lhs, const Pair<T, U>& rhs) { lhs = rhs; }

//----------------------------------------------------------------------
// Section      : LowerBound / UpperBound on Tuples operate element by element.
//----------------------------------------------------------------------
// Full name    : Aggregation helper definitioin
// Purpose      : fast operation for aggregation operations
//----------------------------------------------------------------------

	template <class T, class U>
	inline bool
	IsLowerBound(Pair<T, U> a, Pair<T, U> b) 
	{ 
		return IsLowerBound(a.first, b.first) && IsLowerBound(a.second, b.second);
	}

	template <class T, class U>
	inline bool
	IsUpperBound(Pair<T, U> a, Pair<T, U> b) 
	{ 
		return IsUpperBound(a.first, b.first) && IsUpperBound(a.second, b.second);
	}

	template <class T, class U>
	inline Pair<T, U> 
	LowerBound(Pair<T, U> a, Pair<T, U> b) 
	{ 
		return Pair<T, U>(LowerBound(a.first, b.first), LowerBound(a.second, b.second));
	}

	template <class T, class U>
	inline Pair<T, U> 
	UpperBound(Pair<T, U> a, Pair<T, U> b) 
	{ 
		return Pair<T, U>(UpperBound(a.first, b.first), UpperBound(a.second, b.second));
	}

	template <class T, class U>
	inline void 
	MakeLowerBound(Pair<T, U>& a, Pair<T, U> b) 
	{ 
		MakeLowerBound(a.first,  b.first);
		MakeLowerBound(a.second, b.second);
	}

	template <class T, class U>
	inline void 
	MakeUpperBound(Pair<T, U>& a, Pair<T, U> b) 
	{ 
		MakeUpperBound(a.first,  b.first);
		MakeUpperBound(a.second, b.second);
	}

	template <class T, class U>
	inline void 
	MakeBounds(Pair<T, U>& a, Pair<T, U>& b) 
	{ 
		MakeBounds(a.first,  b.first);
		MakeBounds(a.second, b.second);
	}

	template <class T, class U>
	inline bool IsStrictlyLower(Pair<T, U> a, Pair<T, U> b)
	{ 
		return IsStrictlyLower(a.first, b.first) && IsStrictlyLower(a.second, b.second); 
	}

	template <class T, class U>
	inline bool IsStrictlyGreater(Pair<T, U> a, Pair<T, U> b)
	{ 
		return IsStrictlyGreater(a.first, b.first) && IsStrictlyGreater(a.second, b.second); 
	}

	template <class T, class U>
	void
	MakeStrictlyGreater(Pair<T, U>& ref)
	{
		MakeStrictlyGreater(ref.first);
		MakeStrictlyGreater(ref.second);
	}


//----------------------------------------------------------------------
// Section      : assignment operators
//----------------------------------------------------------------------
// Full name    : Aggregation helper definitioin
// Purpose      : fast operation for aggregation operations
//----------------------------------------------------------------------
// Similar operators are defined with macro's to promote similarity and clarity

#define DEFINE_ASSIGNMENT_OPERATOR(X)                                \
template <typename TX, typename TY, typename UX, typename UY> inline \
void operator X (Pair<TX, TY>& self, Pair<UX, UY> other)             \
{                                                                    \
	self.first  X other.first;                                       \
	self.second X other.second;                                      \
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
// Section      : unary operators on Tuples
//----------------------------------------------------------------------
// Similar operators are defined with macro's to promote similarity and clarity

template <class T, class U> inline                                    
Pair<T, U> operator -(Pair<T, U> arg)
{
	return Pair<T,U>(-arg.first, -arg.second);
}

//----------------------------------------------------------------------
// Section      : binary operators on Tuples
//----------------------------------------------------------------------


#define DEFINE_BINARY_OPERATOR(X)                              \
template <class T, class U1, class U, class U2> inline         \
Pair<T, U1> operator X(Pair<T, U1> a, Pair<U, U2> b)           \
{                                                              \
	return Pair<T,U1>(a.first X b.first, a.second X b.second); \
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

//----------------------------------------------------------------------
// Section      : Iterator operations for Tuples
//----------------------------------------------------------------------

template <class T, class U>
void operator ++(Pair<T, U>& v)
{
//	illegal_abstract("Pair", "operator ++()");
	U tmp = v.second;
	if (!(tmp < ++v.second))
		++v.first;

}

template <class T, class U>
Float64 AsFloat64(Pair<T, U> v)
{
	throwIllegalAbstract(MG_POS, "Pair::AsFloat64");
}

#endif // __RTC_GEO_PAIR_H
