// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_GEO_CONVERSIONS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/BaseBounds.h"
#include "geo/ConversionBase.h"
#include "geo/Geometry.h"
#include "geo/Round.h"
#include "geo/SequenceArray.h"
#include "geo/Undefined.h"
#include "ser/AsString.h"
#include "ser/FormattedStream.h"

#define __RTC_GEO_CONVERSIONS_H

//----------------------------------------------------------------------
// Section      : has_equivalent_null
//----------------------------------------------------------------------

template <typename T, typename U> constexpr bool has_equivalent_null_v = 
	(is_bitvalue_v<T> || is_bitvalue_v<U> 
		? is_bitvalue_v<T> && is_bitvalue_v<U> 
		: U(UNDEFINED_OR_ZERO(T)) == UNDEFINED_OR_ZERO(U)
	);

template <typename T, typename U> struct has_equivalent_null : std::bool_constant<has_equivalent_null_v<T, U>> {};
template <typename T>             struct has_equivalent_null<T, T> : std::true_type {};

//----------------------------------------------------------------------
// Bool conversions
//----------------------------------------------------------------------

// numeric -> Bool conversion
template <NumericValue Src, typename ExceptFunc, typename ConvertFunc>
Bool Convert4(const Src& val, const Bool* dummy2, const ExceptFunc* dummy3, const ConvertFunc* dummy4) 
{
	if constexpr (has_undefines_v<Src>)
	{
		if (!IsDefined(val))
			return UNDEFINED_OR_ZERO(Bool);
	}
	return Bool(val != Src());
}

// Bool to anything conversion (including Bool itself)
template <typename Dst, typename ExceptFunc, typename ConvertFunc>
inline Dst Convert4(Bool val, const Dst* dummy, const ExceptFunc* dummyFunc, const ConvertFunc* dummy4) 
{
	return Dst(val);
}

//----------------------------------------------------------------------
// numeric (non Bool) conversions
//----------------------------------------------------------------------

struct ok_func
{
	template <typename T>
	bool operator() (T ) const { return true; }
};

template <typename T>
struct check_defined_func
{
	bool operator () (typename param_type<T>::type v) const
	{
		return IsDefined(v);
	}
};

template <typename T, typename U>
struct check_min_func
{
	bool operator () (typename param_type<T>::type v) const
	{
		return v >= T(MIN_VALUE(U));
	}
};

template <typename T, typename U>
struct check_max_func
{
	bool operator () (typename param_type<T>::type v) const
	{
		return v <= T(MAX_VALUE(U));
	}
};

template <typename T, typename U>
struct check_def_mf
	:	std::conditional< (is_bitvalue<T>::value || has_equivalent_null<T,U>::value)
		,	ok_func
		,	check_defined_func<T>
	>
{};

template <typename T, typename U>
struct check_min_mf
	:	std::conditional< (is_signed<T>::value && ((!is_signed<U>::value) || ( nrvalbits_of<U>::value < nrvalbits_of<T>::value)) )
		,	check_min_func<T,U>
		,	ok_func
	>
{};

template <typename T, typename U>
struct check_max_mf 
	:	std::conditional< (nrvalbits_of<U>::value < nrvalbits_of<T>::value)
		,	check_max_func<T,U>
		,	ok_func
	>
{};

template <typename U>
struct DnConvertFunc
{
	template <typename T>
	U operator ()(const T& val) const
	{
		return RoundDown<sizeof(U)>(val);
	}
	template <typename T>
	struct rebind
	{
		typedef DnConvertFunc<T> type;
	};
};

template <typename U>
struct UpConvertFunc
{
	template <typename T>
	U operator ()(const T& val) const
	{
		return RoundUp<sizeof(U)>(val);
	}
	template <typename T>
	struct rebind
	{
		typedef UpConvertFunc<T> type;
	};
};

template <typename U>
struct DefaultConvertFunc
{
	template <typename T>
	U operator ()(const T& val) const
	{
		return U(val);
	}
	template <typename T>
	struct rebind
	{
		typedef DefaultConvertFunc<T> type;
	};
};

template <typename T, typename U, typename CheckDefFunc, typename CheckMinFunc, typename CheckMaxFunc, typename ExceptFunc, typename ConvertFunc>
struct NumericDefinedConverter // throws on undefined values
{
	U operator ()(const T& val) const
	{
		if (!definedF(val) || !minF(val) || !maxF(val))
			return exceptF.template apply<U>(val);
		return convF(val);
	}
	CheckDefFunc definedF;
	CheckMinFunc minF;
	CheckMaxFunc maxF;
	ConvertFunc  convF;
	ExceptFunc   exceptF;
};

template <typename T, typename U, typename CheckMinFunc, typename CheckMaxFunc, typename ExceptFunc, typename ConvertFunc>
struct NumericNonnullConverter // converts undefined values
{
	U operator ()(const T& val) const
	{
		if (!minF(val) || !maxF(val))
			return exceptF.template apply<U>(val);
		return convF(val);
	}
	CheckMinFunc minF;
	CheckMaxFunc maxF;
	ConvertFunc  convF;
	ExceptFunc   exceptF;
};

template <typename T, typename U, typename CheckDefFunc, typename CheckMinFunc, typename CheckMaxFunc, typename ExceptFunc, typename ConvertFunc>
struct NumericCheckedConverter // converts undefined values
{
	U operator ()(const T& val) const
	{
		if (!definedF(val))
			return UNDEFINED_VALUE(U);
		if (!minF(val) || !maxF(val))
			return exceptF.template apply<U>(val);
		return convF(val);
	}
	CheckDefFunc definedF;
	CheckMinFunc minF;
	CheckMaxFunc maxF;
	ConvertFunc  convF;
	ExceptFunc   exceptF;
};

template <typename T, typename U, typename ExceptFunc, typename ConvertFunc>
typename std::enable_if_t<is_numeric_v<T> && is_numeric<U>::value, U>
Convert4(const T& val, const U* /*dummy*/, const ExceptFunc*, const ConvertFunc*)
{
	using CDefF = typename check_def_mf<T, U>::type;
	using CMinF = typename check_min_mf<T, U>::type;
	using CMaxF = typename check_max_mf<T, U>::type;
	if constexpr (has_undefines<U>::value)
		return NumericCheckedConverter<T, U, CDefF, CMinF, CMaxF, ExceptFunc, ConvertFunc>()(val);
	else
		return NumericDefinedConverter<T, U, CDefF, CMinF, CMaxF, ExceptFunc, ConvertFunc>()(val);
}

template <typename T, typename U, typename ExceptFunc, typename ConvertFunc>
typename std::enable_if_t<is_numeric_v<T> && is_numeric<U>::value, U>
ConvertNonNull4(const T& val, const U* /*dummy*/, const ExceptFunc*, const ConvertFunc*)
{
	return NumericNonnullConverter<T, U, typename check_min_mf<T, U>::type, typename check_max_mf<T, U>::type, ExceptFunc, ConvertFunc>()(val);
}

//template <bit_size_t N, typename Block, typename U>
//U Convert2(const bit_reference<N, Block>& ref, const U* dummy, const ExceptFunc* dummyFunc)
//{
//	return Convert<U>(bit_value<N>(ref) );
//}

template <typename Dst, bit_size_t N, typename Block, typename ExceptFunc, typename ConvertFunc>
Dst Convert4(const bit_reference<N, Block>& ref, const Dst* /*dummy*/, const ExceptFunc* dummyExceptFunc, const ConvertFunc* dummyConvertFunc)
{
	return Convert4(bit_value<N>(ref), TYPEID(Dst), dummyExceptFunc, dummyConvertFunc);
}

//----------------------------------------------------------------------
// String conversions
//----------------------------------------------------------------------

template <typename T, typename ExceptFunc, typename ConvertFunc>
inline T Convert4(WeakStr val, const T*, const ExceptFunc* dummyExceptFunc, const ConvertFunc* dummyConvertFunc)
{ 
	T result; 
	if (val.IsDefined())
		AssignValueFromCharPtrs(result, val.begin(), val.send()); 
	else
		Assign(result, Undefined());
	return result; 
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
inline T Convert4(StringCRef val, const T*, const ExceptFunc* dummyExceptFunc, const ConvertFunc* dummyConvertFunc)
{
	T result;
	if (val.IsDefined())
		AssignValueFromCharPtrs(result, begin_ptr( val ), end_ptr( val ));
	else
		Assign(result, Undefined());
	return result;
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
inline T Convert4(std::string_view val, const T*, const ExceptFunc* dummyExceptFunc, const ConvertFunc* dummyConvertFunc)
{
	T result;
	if (IsDefined(val))
		AssignValueFromCharPtrs(result, begin_ptr(val), end_ptr(val));
	else
		Assign(result, Undefined());
	return result;
}

template <typename ExceptFunc, typename ConvertFunc>
inline SharedStr Convert4(StringCRef val, const SharedStr*, const ExceptFunc* dummyFunc, const ConvertFunc* dummyConvertFunc) 
{ 
	return val.IsDefined()
		?	SharedStr(val.begin(), val.end())
		:	UNDEFINED_VALUE(SharedStr); 
}

template <typename ExceptFunc, typename ConvertFunc>
inline SharedStr Convert4(std::string_view val, const SharedStr*, const ExceptFunc* dummyFunc, const ConvertFunc* dummyConvertFunc)
{
	return IsDefined(val)
		? SharedStr(val.begin(), val.end())
		: UNDEFINED_VALUE(SharedStr);
}

// conversions to string
template <typename T, typename ExceptFunc, typename ConvertFunc>
inline SharedStr Convert4(const T& val, const SharedStr*, const ExceptFunc* /*dummyExceptFunc*/, const ConvertFunc* /*dummyConvertFunc*/)
{
	return AsString(val);
}


//----------------------------------------------------------------------
// TokenID
//----------------------------------------------------------------------

template <typename ExceptFunc, typename ConvertFunc> inline
TokenID Convert4(TokenID src, const TokenID*, const ExceptFunc* ef, const ConvertFunc* cf)
{
	return src;
}

//----------------------------------------------------------------------
// compound value conversions
//----------------------------------------------------------------------

template <typename Dst, typename Src, typename ExceptFunc, typename ConvertFunc> inline
Point<Dst> Convert4(const Point<Src>& src, const Point<Dst>*, const ExceptFunc* ef, const ConvertFunc* cf)
{
	using Converter = typename ConvertFunc::template rebind<Dst>::type;
	return 
		Point<Dst>(
			Convert4(src.first,  TYPEID(Dst), ef, TYPEID(Converter))
		,	Convert4(src.second, TYPEID(Dst), ef, TYPEID(Converter))
		);
}

template <typename T, typename ExceptFunc, typename ConvertFunc>
inline std::vector<T> Convert4(typename sequence_array<T>::const_reference v, const std::vector<T>*, const ExceptFunc* dummyExceptFunc, const ConvertFunc* dummyConvertFunc)
{ 
	if (!v.IsDefined())
		return ExceptFunc().template apply<std::vector<T>>(v);
	return std::vector<T>(v.begin(), v.end()); 
}


#endif // __RTC_GEO_CONVERSIONS_H
