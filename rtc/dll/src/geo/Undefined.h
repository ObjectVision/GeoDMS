
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

#if !defined(__RTC_GEO_UNDEFINED_H)
#define __RTC_GEO_UNDEFINED_H

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

template <typename T> struct has_9999_as_null: std::false_type {};
#if !defined(MG_USE_QNAN)
template <>           struct has_9999_as_null<  Int16> : std::true_type {};
template <>           struct has_9999_as_null<  Int32> : std::true_type {};
template <>           struct has_9999_as_null<  Int64> : std::true_type {};
template <>           struct has_9999_as_null<Float32> : std::true_type {};
template <>           struct has_9999_as_null<Float64> : std::true_type {};
#endif

inline Void    UndefinedValue(const Void*)    { return Void(); }
inline UInt64  UndefinedValue(const UInt64*)  { return 0xFFFFFFFFFFFFFFFFui64; }  // 2^64-1 = 4294967295
inline UInt32  UndefinedValue(const UInt32*)  { return 0xFFFFFFFF; }  // 2^32-1 = 4294967295
inline Int32   UndefinedValue(const long*)    { return 0x80000000; }
inline UInt32  UndefinedValue(const unsigned long*) { return 0xFFFFFFFF; }
inline UInt16  UndefinedValue(const UInt16*)  { return 0xFFFF; }      // 2^16-1 = 65535
inline UInt8   UndefinedValue(const UInt8*)   { return 0xFF; }        // 2^08-1 =  255
inline Int8    UndefinedValue(const Int8*)    { return Int8(0x80); }  // 2^07   = -128

#if defined(MG_USE_QNAN)
inline Int32   UndefinedValue(const Int32*)   { return 0x80000000; }
inline Int64   UndefinedValue(const Int64*)   { return 0x8000000000000000i64; }
inline Int16   UndefinedValue(const Int16*)   { return Int16(0x8000); }       
inline Float32 UndefinedValue(const Float32*) { return std::numeric_limits<Float32>::quiet_NaN(); }
inline Float64 UndefinedValue(const Float64*) { return std::numeric_limits<Float64>::quiet_NaN(); }
#else
inline Int32   UndefinedValue(const Int32*)   { return -9999; }
inline Int64   UndefinedValue(const Int64*)   { return -9999; }
inline Int16   UndefinedValue(const Int16*)   { return -9999; }
inline Float32 UndefinedValue(const Float32*) { return -9999; }
inline Float64 UndefinedValue(const Float64*) { return -9999; }
#endif

#if defined(DMS_TM_HAS_FLOAT80)
inline Float80 UndefinedValue(const Float80*)  { return std::numeric_limits<T>::quiet_NaN(); }
#endif

template <typename T> struct has_max_as_null: std::false_type {};
template <>           struct has_max_as_null< UInt64> : std::true_type {};
template <>           struct has_max_as_null< UInt32> : std::true_type {};
template <>           struct has_max_as_null< UInt16> : std::true_type {};
template <>           struct has_max_as_null< UInt8 > : std::true_type {};

template <typename T> struct has_min_as_null: std::false_type {};
template <>           struct has_min_as_null< long > : std::true_type {};

#if defined(MG_USE_QNAN)
template <>           struct has_min_as_null< Int64> : std::true_type {};
template <>           struct has_min_as_null< Int32> : std::true_type {};
template <>           struct has_min_as_null< Int16> : std::true_type {};
template <>           struct has_min_as_null< Int8 > : std::true_type {};
#endif

template <typename T, typename U> struct has_equivalent_null : std::bool_constant<has_9999_as_null<T>::value &&  has_9999_as_null<U>::value > {};
template <typename T>             struct has_equivalent_null<T,T> : std::true_type {};

#define UNDEFINED_VALUE_STRING "null"
#define UNDEFINED_VALUE_STRING_LEN (sizeof(UNDEFINED_VALUE_STRING)-1)

inline CharPtr       UndefinedValue(const CharPtr*) { return nullptr; }

template <typename T> constexpr bool has_undefines_v = !is_bitvalue_v<T>;
template <typename T> using has_undefines = std::bool_constant<has_undefines_v<T>>;

//----------------------------------------------------------------------
// Section      : UndefinedOrZero
//----------------------------------------------------------------------

template <typename T>
T UndefinedOrZero(const T* x) { return UndefinedValue(x); }

template <typename T>
T UndefinedOrMax(const T* x) { return UndefinedValue(x); }

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
inline bool IsDefined(const T& v) { return v != UNDEFINED_VALUE(T); }

#if defined(MG_USE_QNAN)
inline bool IsDefined(Float32 v) { return !isnan(v); }
inline bool IsDefined(Float64 v) { return !isnan(v); }
#endif

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
