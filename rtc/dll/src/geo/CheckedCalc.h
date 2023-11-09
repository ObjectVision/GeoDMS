// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __RTC_GEO_CHECKEDCALC_H
#define __RTC_GEO_CHECKEDCALC_H

#include "dbg/Check.h"
#include "geo/ElemTraits.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "ser/AsString.h"

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

	auto primaryMsg = mySSPrintF("Numeric overflow when %1% %2% values %3% %4% %5%."
		, opName, vcName.c_str(), AsString(a), preposition, AsString(b)
	);

	if (!suggestAlternative)
		throwDmsErrD(primaryMsg.c_str());

	throwDmsErrF("%1%"
		"\nConsider using %2% if your model deals with overflow as null values%3%%4%."
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

template <typename T> 
typename std::enable_if<! is_signed_v<T>, T >::type
CheckedAdd(T a, T b)
{
	assert(a>=0);
	assert(b>=0);
	T r = a+b;
	assert(r>=0);
	if (r < a || r < b)
		throwDmsErrD("Overflow in addition");
	return r;
}

template <typename T> T
CheckedMul(T a,T b, bool suggestAlternative)
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
inline UInt64 CheckedMul<UInt64>(UInt64 a, UInt64 b, bool suggestAlternative)
{
	UInt64 res = a * b; 
	if ((a && (res / a != b)) || (b && (res / b != a)))
		throwDmsErrD("Overflow in multiplication");
	return res;
}

template <>
inline Int64 CheckedMul<Int64>(Int64 a, Int64 b, bool suggestAlternative)
{
	Int64 res = a * b; 
	if ((a && (res / a != b)) || (b && (res / b != a)))
		throwDmsErrD("Overflow in multiplication");
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


#endif // __RTC_GEO_CHECKEDCALC_H
