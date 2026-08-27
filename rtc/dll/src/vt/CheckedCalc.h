// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Overflow-checked arithmetic: the mul_type<T> widening table, checked
 *  add/sub/mul, and the checked Cardinality of a Range.
 */

#if !defined(__VT_CHECKEDCALC_H)
#define __VT_CHECKEDCALC_H

#include "dbg/Diagnostics.h"
#include "vt/ElemTraits.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "ser/AsString.h"
#include "utl/StrFormat.h"
#include "ser/StringStream.h"

// *****************************************************************************
//						ELEMENTARY BINARY FUNCTORS
// *****************************************************************************

template <typename T>
[[noreturn]] void throwOverflow(CharPtr opName, T a, CharPtr preposition, T b, bool suggestAlternative, CharPtr alternativeFunc, const ValueClass* alternativeValueClass)
{
	SharedStr vcName = AsString(ValueWrap<T>::GetStaticClass()->GetID());
	SharedStr acName;
	if (alternativeValueClass)
		acName = AsString(alternativeValueClass->GetID());

	auto primaryMsg = mySSPrintF("Numeric overflow when {0} {1} values {2} {3} {4}."
		, opName, vcName.c_str(), AsString(a), preposition, AsString(b)
	);

	if (!suggestAlternative)
		throwDmsErrD(primaryMsg.c_str());

	throwDmsErrF("{0}"
		"\nConsider using {1} if your model deals with overflow as null values{2}{3}."
		, primaryMsg
		, alternativeFunc
		, alternativeValueClass ? " or consider converting the arguments to " : ""
		, alternativeValueClass ? acName.c_str() : ""
	);
}

//----------------------------------------------------------------------

template <typename T> struct mul_type;
template <> struct mul_type<UInt8 > { typedef UInt16 type; };
template <> struct mul_type<UInt16> { typedef UInt32 type; };
template <> struct mul_type<UInt32> { typedef UInt64 type; };

template <> struct mul_type<Int8 > { typedef Int16 type; };
template <> struct mul_type<Int16> { typedef Int32 type; };
template <> struct mul_type<Int32> { typedef Int64 type; };

//----------------------------------------------------------------------

template <IntegralValue T>
const ValueClass* NextAddIntegral()
{
	constexpr auto nrBits = nrbits_of_v<T>;
	constexpr bool isSigned = is_signed_v<T>;
	if constexpr (nrBits < 8)
	{
		if constexpr (nrBits == 1)
			return ValueWrap<UInt2>::GetStaticClass();
		else if constexpr (nrBits == 2)
			return ValueWrap<UInt4>::GetStaticClass();
		else
		{
			static_assert(nrBits == 4);
			return ValueWrap<UInt8>::GetStaticClass();
		}
	}
	else
	{
		if constexpr (nrBits <= 16)
		{
			if constexpr (nrBits == 8)
				if constexpr (isSigned)
					return ValueWrap<Int16>::GetStaticClass();
				else
					return ValueWrap<UInt16>::GetStaticClass();
			else
			{
				static_assert(nrBits == 16);
				if constexpr (isSigned)
					return ValueWrap<Int32>::GetStaticClass();
				else
					return ValueWrap<UInt32>::GetStaticClass();
			}
		}
		else
		{
			if constexpr (nrBits == 32)
				if constexpr (isSigned)
					return ValueWrap<Int64>::GetStaticClass();
				else
					return ValueWrap<UInt64>::GetStaticClass();
			else
			{
				static_assert(nrBits == 64);
				return nullptr;
			}
		}
	}
}

template <IntegralValue T>
const ValueClass* NextSubIntegral()
{
	constexpr auto nrBits = nrbits_of_v<T>;
	constexpr bool isSigned = is_signed_v<T>;
	if constexpr (nrBits < 8)
	{
		if constexpr (nrBits == 1)
			return ValueWrap<UInt2>::GetStaticClass();
		else if constexpr (nrBits == 2)
			return ValueWrap<UInt4>::GetStaticClass();
		else
		{
			static_assert(nrBits == 4);
			return ValueWrap<UInt8>::GetStaticClass();
		}
	}
	else
	{
		if constexpr (nrBits <= 16)
		{
			if constexpr (nrBits == 8)
				if constexpr (isSigned)
					return ValueWrap<Int16>::GetStaticClass();
				else
					return ValueWrap<Int8>::GetStaticClass();
			else
			{
				static_assert(nrBits == 16);
				if constexpr (isSigned)
					return ValueWrap<Int32>::GetStaticClass();
				else
					return ValueWrap<Int16>::GetStaticClass();
			}
		}
		else
		{
			if constexpr (nrBits == 32)
				if constexpr (isSigned)
					return ValueWrap<Int64>::GetStaticClass();
				else
					return ValueWrap<Int32>::GetStaticClass();
			else
			{
				static_assert(nrBits == 64);
				if constexpr (isSigned)
					return nullptr;
				else
					return ValueWrap<Int64>::GetStaticClass();
			}
		}
	}
}

//----------------------------------------------------------------------

// CheckedAdd / CheckedSub, for unsigned and signed integrals alike.
//
// suggestAlternative == true offers the caller the ..._or_null attribute operator and the next
// wider value type, which is the right advice when the overflow came from an attribute
// expression. Callers whose overflow has a different remedy -- discrete_alloc's shadow price
// arithmetic, see DiscrAlloc.cpp -- pass false and add their own.

template <typename T>
requires (!is_signed_v<T>)
auto CheckedAdd(T a, T b, bool suggestAlternative = true) -> T
{
	assert(a>=0);
	assert(b>=0);
	T r = a+b;
	assert(r>=0);
	if (r < a || r < b)
		throwOverflow("adding", a, "and", b, suggestAlternative, "add_or_null", NextAddIntegral<T>());
	return r;
}

// Signed counterpart of the unsigned CheckedAdd above.
//
// Signed overflow is still UB in C++20, so the sum is formed in the unsigned representation --
// where wrapping is defined -- and converted back, which C++20 made well defined by fixing the
// representation to two's complement. The sign of b then decides which way the result must have
// moved: a + b can only end up below a when b is negative, so a mismatch is exactly an overflow.
template <typename T>
requires is_signed_integral_v<T>
auto CheckedAdd(T a, T b, bool suggestAlternative = true) -> T
{
	using unsigned_t = std::make_unsigned_t<T>;
	T r = T(unsigned_t(a) + unsigned_t(b));
	if ((b >= 0) != (r >= a))
		throwOverflow("adding", a, "and", b, suggestAlternative, "add_or_null", NextAddIntegral<T>());
	return r;
}

// Unsigned subtraction has no representable negative result, so any borrow is an overflow.
template <typename T>
requires (!is_signed_v<T>)
auto CheckedSub(T a, T b, bool suggestAlternative = true) -> T
{
	if (a < b)
		throwOverflow("subtracting", b, "from", a, suggestAlternative, "sub_or_null", NextSubIntegral<T>());
	return a - b;
}

// Signed subtraction, by the same argument as the signed CheckedAdd above: a - b can only end up
// below a when b is positive.
template <typename T>
requires is_signed_integral_v<T>
auto CheckedSub(T a, T b, bool suggestAlternative = true) -> T
{
	using unsigned_t = std::make_unsigned_t<T>;
	T r = T(unsigned_t(a) - unsigned_t(b));
	if ((b <= 0) != (r >= a))
		throwOverflow("subtracting", b, "from", a, suggestAlternative, "sub_or_null", NextSubIntegral<T>());
	return r;
}

template <typename T>
auto CheckedMul(T a,T b, bool suggestAlternative) -> T
{
	if constexpr (is_integral_v<T>)
	{
		typename mul_type<T>::type r = a;
		r *= b;
		if (r != T(r))
			throwOverflow("multiplying", a, "and", b, suggestAlternative, "mul_or_null", NextAddIntegral<T>());
		return r;
	}
	else
		return a * b;
}

template <>
inline auto CheckedMul<UInt64>(UInt64 a, UInt64 b, bool suggestAlternative) -> UInt64
{
	UInt64 res = a * b; 
	if ((a && (res / a != b)) || (b && (res / b != a)))
		throwOverflow("multiplying", a, "and", b, suggestAlternative, "mul_or_null", nullptr);
	return res;
}

template <>
inline auto CheckedMul<Int64>(Int64 a, Int64 b, bool suggestAlternative) -> Int64
{
	Int64 res = a * b; 
	if ((a && (res / a != b)) || (b && (res / b != a)))
		throwOverflow("multiplying", a, "and", b, suggestAlternative, "mul_or_null", nullptr);
	return res;
}

//----------------------------------------------------------------------
// Section      : Geometric operators
//----------------------------------------------------------------------

template <class T> inline
SizeT Cardinality(const Point<T>& v) { return CheckedMul<SizeT>(Cardinality(v.first), Cardinality(v.second), false); }

template <class T> inline
product_type_t<T> Area(const Point<T>& v)
{
	return CheckedMul< product_type_t<T>>(v.first, v.second, false);
}

template <class T> inline
SizeT
Cardinality(Range<T> r)
{
	return IsDefined(r)
		? r.empty()
		? 0
		: Cardinality(Size(r))
		: UNDEFINED_VALUE(SizeT);
}

template <> inline
SizeT
Cardinality(Range<Void>) { return 1; }

template <class T> inline
typename product_type<T>::type
Area(Range<Point<T> > r)
{
	return IsDefined(r)
		? r.empty()
		? 0
		: Area(Size(r))
		: UNDEFINED_VALUE(typename product_type<T>::type);
}


#endif // __VT_CHECKEDCALC_H
