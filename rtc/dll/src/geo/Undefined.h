// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_GEO_UNDEFINED_H)
#define __RTC_GEO_UNDEFINED_H

#include <string_view>

// Types
#include "cpc/Types.h"
#include "geo/ElemTraits.h"

#include <limits>

#define MG_USE_QNAN

#define UNDEFINED_VALUE(T)    UndefinedValue (TYPEID(T))
#define UNDEFINED_OR_ZERO(T)  UndefinedOrZero(TYPEID(T))
#define UNDEFINED_OR_MAX(T)   UndefinedOrMax (TYPEID(T))

//----------------------------------------------------------------------
// Section      : UndefinedValue implementation
//----------------------------------------------------------------------

//inline Float32 QNaN_Value(const Float80*)  { char ldNan[ 4] = { 0xFF, 0xFF, 0xFF, 0x7F } return  *(float*) ldNan }
//inline Float64 QNaN_Value(const Float80*)  { char ldNan[ 8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F } return  *(double*) ldNan }
//inline Float80 QNaN_Value(const Float80*)  { char ldNan[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F } return  *(long double*) ldNan }

inline constexpr Void    UndefinedValue(const Void*)    { return Void(); }

#if defined(_MSC_VER)
inline constexpr UInt64  UndefinedValue(const UInt64*)  { return 0xFFFFFFFFFFFFFFFFui64; }  // 2^64-1 = 4294967295
inline constexpr Int64   UndefinedValue(const Int64*) { return 0x8000000000000000i64; }
inline constexpr Int32   UndefinedValue(const long*) { return 0x80000000; }
inline constexpr UInt32  UndefinedValue(const unsigned long*) { return 0xFFFFFFFF; }
#else
inline constexpr UInt64  UndefinedValue(const UInt64*) { return 0xFFFFFFFFFFFFFFFFul; }  // 2^64-1 = 4294967295
inline constexpr Int64   UndefinedValue(const Int64*) { return 0x8000000000000000l; }
#endif

inline constexpr UInt32  UndefinedValue(const UInt32*)  { return 0xFFFFFFFF; }  // 2^32-1 = 4294967295
inline constexpr UInt16  UndefinedValue(const UInt16*)  { return 0xFFFF; }      // 2^16-1 = 65535
inline constexpr UInt8   UndefinedValue(const UInt8*)   { return 0xFF; }        // 2^08-1 =  255
inline constexpr Int8    UndefinedValue(const Int8*)    { return Int8(0x80); }  // 2^07   = -128

inline constexpr Int32   UndefinedValue(const Int32*)   { return 0x80000000; }
inline constexpr Int16   UndefinedValue(const Int16*)   { return Int16(0x8000); }
inline constexpr Float32 UndefinedValue(const Float32*) { return std::numeric_limits<Float32>::quiet_NaN(); }
inline constexpr Float64 UndefinedValue(const Float64*) { return std::numeric_limits<Float64>::quiet_NaN(); }

#if defined(DMS_TM_HAS_FLOAT80)
inline Float80 UndefinedValue(const Float80*)  { return std::numeric_limits<T>::quiet_NaN(); }
#endif

template <typename T> struct has_max_as_null: std::false_type {};
template <>           struct has_max_as_null< UInt64> : std::true_type {};
template <>           struct has_max_as_null< UInt32> : std::true_type {};
template <>           struct has_max_as_null< UInt16> : std::true_type {};
template <>           struct has_max_as_null< UInt8 > : std::true_type {};

template <typename T> struct has_min_as_null: std::false_type {};

#if defined(_MSC_VER)
template <>           struct has_min_as_null< long > : std::true_type {};
#endif //defined(_MSC_VER)

template <>           struct has_min_as_null< Int64> : std::true_type {};
template <>           struct has_min_as_null< Int32> : std::true_type {};
template <>           struct has_min_as_null< Int16> : std::true_type {};
template <>           struct has_min_as_null< Int8 > : std::true_type {};


#define UNDEFINED_VALUE_STRING "null"
#define UNDEFINED_VALUE_STRING_LEN (sizeof(UNDEFINED_VALUE_STRING)-1)

inline CharPtr          UndefinedValue(const CharPtr*) { return nullptr; }
inline std::string_view UndefinedValue(const std::string_view*) { return { UNDEFINED_VALUE_STRING, 4 }; }

template <typename T> constexpr bool has_undefines_v = !is_bitvalue_v<T>;
template <typename T> constexpr bool has_min_as_null_v = has_min_as_null<T>::value;
template <typename T> using has_undefines = std::bool_constant<has_undefines_v<T>>;
template <typename T> concept NullableValue = has_undefines_v<T> && std::regular<T>;

template <typename T> concept NonNullableValue = (!has_undefines_v<T>) && std::regular<T>;

//----------------------------------------------------------------------
// Section      : UndefinedOrZero
//----------------------------------------------------------------------

template <typename T>
constexpr T UndefinedOrZero(const T* x) { return UndefinedValue(x); }

template <typename T>
constexpr T UndefinedOrMax(const T* x) { return UndefinedValue(x); }

template <typename Field, typename Alloc>
constexpr std::vector<Field, Alloc> UndefinedOrZero(const std::vector<Field, Alloc>*) { return std::vector<Field>(); }


//----------------------------------------------------------------------
// Section      : MakeUndefined operator; override for burdensome copy constructibles (to avoid return by value)
//----------------------------------------------------------------------

template <typename T>
inline void MakeUndefined(T& v)
{
	v = UNDEFINED_VALUE(T); 
}

template <typename T>
inline void MakeUndefinedOrZero(T& v)
{
	v = UNDEFINED_OR_ZERO(T); 
}

template <typename Field, typename Alloc>
inline void MakeUndefined(std::vector<Field, Alloc>& vec)
{
	if (vec.size())
		vec = std::vector<Field, Alloc>();
}

template <typename Field, typename Alloc>
inline void MakeUndefinedOrZero(std::vector<Field, Alloc>& vec)
{
	MakeUndefined(vec);
}


//----------------------------------------------------------------------
// Undefined value and its casting to Basic types for typed Undefined creation
//----------------------------------------------------------------------

struct Undefined {};

//----------------------------------------------------------------------
// Section      : Assign
//----------------------------------------------------------------------

template <typename T> void Assign(T& lhs, const T& rhs) { lhs = rhs;          }
template <typename T> void Assign(T& lhs, Undefined)    { MakeUndefinedOrZero(lhs); }

//----------------------------------------------------------------------
// Section      : IsDefined predicate, override for bit_value and sequence_ref
//----------------------------------------------------------------------

template <typename T>
inline constexpr bool IsDefined(const T& v) { return v != UNDEFINED_VALUE(T); }

template <typename Field>
inline constexpr bool IsDefined(const std::vector<Field>& v) { return v.size(); }

//inline bool IsDefined(Float32 v) { return fpclassify(v) <= 0; }
//inline bool IsDefined(Float64 v) { return fpclassify(v) <= 0; }
// https://en.wikipedia.org/wiki/Single-precision_floating-point_format
const UInt32 F32_EXP_FLAGS = 0xFF << 23;
const UInt32 F32_SGN_FLAG = 1 << 31;

// https://en.wikipedia.org/wiki/Double-precision_floating-point_format
#if defined(_MSC_VER)
const UInt64 F64_EXP_FLAGS = 0x7FFui64 << 52;
const UInt64 F64_SGN_FLAG = 1ui64 << 63;
#else
const UInt64 F64_EXP_FLAGS = 0x7FFul << 52;
const UInt64 F64_SGN_FLAG = 1ul << 63;
#endif

inline bool IsDefined(Float32 v)
{
	//	return !isnan(v);
	UInt32 vAsUInt32 = *reinterpret_cast<const UInt32*>(&v);
	UInt32 expFlags = vAsUInt32 & F32_EXP_FLAGS;
	return expFlags != F32_EXP_FLAGS;
}

inline bool IsDefined(Float64 v)
{
	//	return !isnan(v);
	UInt64 vAsUInt64 = *reinterpret_cast<const UInt64*>(&v);
	UInt64 expFlags = vAsUInt64 & F64_EXP_FLAGS;
	return expFlags != F64_EXP_FLAGS;
}

bool IsDefined(Void); // CHECK THAT THIS IS NEVER CALLED

//----------------------------------------------------------------------
// Section      : AllDefined predicate for a range of values
//----------------------------------------------------------------------

template <typename CI>
inline bool AllDefined(CI b, CI e)
{ 
	for (; b!=e; ++b)
		if (!IsDefined(*b))
			return false;
	return true;
}

//----------------------------------------------------------------------
// Section      : MakeDefined function
//----------------------------------------------------------------------

template <typename T>
inline void MakeDefined(T& v, const T& d) { if (!IsDefined(v)) v = d; }

template <typename Iter, typename T>
inline void MakeDefined(Iter b, Iter e, const T& d) { for(; b!=e; ++b) MakeDefined(*b, d); }

#endif // !defined(__RTC_GEO_UNDEFINED_H)
