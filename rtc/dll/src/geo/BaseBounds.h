// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_GEO__BASEBOUNDS_H)
#define __RTC_GEO__BASEBOUNDS_H

#include "geo/ElemTraits.h"
#include "geo/MinMax.h"
#include "geo/BitValue.h"
#include "utl/Instantiate.h"

#include <limits>

#if defined(min)
	#pragma reminder("include your namespace polluting libs after BaseBounds.h; we have to access std::numeric_limits<T>::min")
	#undef min
#endif //defined(min)

#if defined(max)
	#pragma reminder("include your namespace polluting libs after BaseBounds.h; we have to access std::numeric_limits<T>::max")
	#undef max
#endif //defined(max)

namespace minmax_impl {

	template <typename T, bool MinIsLowestValue, bool HasMinAsNull>
	struct LowestValueProvider;

	template <typename T>
	struct LowestValueProvider<T, true, false>
	{
		static T value() { return std::numeric_limits<T>::min(); }
	};
	template <typename T>
	struct LowestValueProvider<T, true, true>
	{
		static T value() { return std::numeric_limits<T>::min()+1; } // min() is UNDEFINED
	};

	template <typename T>
	struct LowestValueProvider<T, false, false>
	{
		static T value() { return -std::numeric_limits<T>::max(); }
	};

	template <typename T, bool HasMaxAsNull>
	struct HighestValueProvider;

	template <typename T>
	struct HighestValueProvider<T, true>
	{
		static T value() { return std::numeric_limits<T>::max()-1; }
	};

	template <typename T>
	struct HighestValueProvider<T, false>
	{
		static T value() { return std::numeric_limits<T>::max(); }
	};
}	// end of namespace impl

template<typename T> struct minmax_traits
{
	static T MinValue() { return minmax_impl::LowestValueProvider <T, std::numeric_limits<T>::is_integer, has_min_as_null<T>::value>::value(); }
	static T MaxValue() { return minmax_impl::HighestValueProvider<T, has_max_as_null<T>::value>::value(); }
};

template<typename T> inline T MinValue() { return minmax_traits<T>::MinValue(); }
template<typename T> inline T MaxValue() { return minmax_traits<T>::MaxValue(); }

#define MIN_VALUE(T) MinValue< T >()
#define MAX_VALUE(T) MaxValue< T >()

//----------------------------------------------------------------------
// Section      : elementwize ordering: MakeLower, MakeHigher, etc.
//----------------------------------------------------------------------
// Full name    : Aggregation helper definitioin
// Purpose      : fast operation for aggregation operations
// for elementary types, Bounds are equal to ranges 
// for complex types, these functions are overridden by generic definitions
//----------------------------------------------------------------------
// Macro style generic definitions to avoid ambiguity with pair versions.

template <bool IsSigned, typename T> struct IsNegativeFunc          { bool operator()(T v) const { return false; } };
template <               typename T> struct IsNegativeFunc<true, T> { bool operator()(T v) const { return v<0;   } };

template <typename T>
inline Bool IsNegative(T v)
{
	return IsNegativeFunc<is_signed<T>::value, T>()(v);
}

#define INSTANTIATE(T) \
	inline	Float64 AsFloat64(T x)                        { return Float64(x); }         \
	inline	T    LowerBound       (param_type<T>::type a, param_type<T>::type b) { return Min<T>(a, b); } \
	inline	T    UpperBound       (param_type<T>::type a, param_type<T>::type b) { return Max<T>(a, b); } \
	inline	bool IsLowerBound     (param_type<T>::type a, param_type<T>::type b) { return ! (b < a); } \
	inline	bool IsUpperBound     (param_type<T>::type a, param_type<T>::type b) { return ! (a < b); } \
	inline	bool IsStrictlyLower  (param_type<T>::type a, param_type<T>::type b) { return a < b; }	\
	inline	bool IsStrictlyGreater(param_type<T>::type a, param_type<T>::type b) { return b < a; }	\
	inline	void MakeLowerBound(T& a, param_type<T>::type b) { MakeMin(a, b); } \
	inline	void MakeUpperBound(T& a, param_type<T>::type b) { MakeMax(a, b); } \
	inline	void MakeBounds(T& a, T& b)                      { MakeRange(a, b); } \
	inline  void Assign(T& lhs, param_type<T>::type rhs)     { lhs = rhs; } \

	INSTANTIATE_NUM_ELEM

#undef INSTANTIATE

template <IntegralValue T>
void MakeStrictlyGreater(T& ref)
{
	++ref;
}

//----------------------------------------------------------------------
// Section      : Bool
//----------------------------------------------------------------------
// specific implementation for Bool

inline Float64 AsFloat64(Bool x)                 { return x; }
inline Bool   LowerBound     (Bool  a, Bool  b) { return a &&  b; }
inline Bool   UpperBound     (Bool  a, Bool  b) { return a ||  b; }
inline bool IsLowerBound     (Bool  a, Bool  b) { return b || !a; }
inline bool IsUpperBound     (Bool  a, Bool  b) { return a || !b; }
inline bool IsStrictlyLower  (Bool  a, Bool  b) { return b && !a; }
inline bool IsStrictlyGreater(Bool  a, Bool  b) { return a && !b; }
inline void MakeLowerBound   (Bool& a, Bool  b) { a = a && b; }
inline void MakeUpperBound   (Bool& a, Bool  b) { a = a || b; }
inline void MakeBounds       (Bool& a, Bool& b) { MakeRange(a, b); }
inline void Assign           (Bool& a, Bool  b) { a = b; }


//----------------------------------------------------------------------
// Section      : Void
//----------------------------------------------------------------------

inline void Assign(Void& /*lhs*/, Void /*rhs*/) { }

[[noreturn]] RTC_CALL Float64 AsFloat64(const Void& ); // illegal abstract

//----------------------------------------------------------------------
// Section      : Element operations on CharPtr
//----------------------------------------------------------------------

RTC_CALL Float64 AsFloat64(CharPtr x);


//----------------------------------------------------------------------
// Section      : Cardinality
//----------------------------------------------------------------------

inline UInt64 Cardinality(UInt64 i) { return i; }
inline UInt64 Cardinality(Int64  i) { return i; }
inline UInt32 Cardinality(UInt32 i) { return i; }
inline UInt32 Cardinality(Int32  i) { return i; }
inline UInt16 Cardinality(UInt16 i) { return i; }
inline UInt16 Cardinality(Int16  i) { return i; }
inline UInt8  Cardinality(UInt8  i) { return i; }
inline UInt8  Cardinality(Int8   i) { return i; }

void NeverLinkThis(); // this function is never defined, so it can only be used in functions that should never be called, such as Copy Constructors

inline Float32 Cardinality(Float32) { throwIllegalAbstract(MG_POS, "Cardiality"); }
inline Float64 Cardinality(Float64) { throwIllegalAbstract(MG_POS, "Cardiality"); }


//----------------------------------------------------------------------
// Section      : Other generic operations
//----------------------------------------------------------------------

template <class T> inline T Abs(const T& t) { return UpperBound(t, -t); }
template <class T> inline T Sqr(const T& x) { return x*x;               }


#endif // !defined(__RTC_GEO__BASEBOUNDS_H)
