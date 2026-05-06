// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


/*
 *  Name        : rtc/cpc/Types.h
 *  Description : Basic types as a result of the compiler & platform configuration
 */

#if !defined(__CPC_TYPES_H)
#define __CPC_TYPES_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "CompChar.h"
#include <stdint.h>

//----------------------------------------------------------------------
// Address mode: define DMS_64 on 64-bit pointer platforms (Windows-x64,
// Linux-x64, etc.), DMS_32 otherwise. Portable across MSVC/GCC/Clang.
//----------------------------------------------------------------------

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || (UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu)
#	define DMS_64
#else
#	define DMS_32
#endif

#include <rtctypemodel.h>

#define TYPEID(T) ((const T*)nullptr)

//----------------------------------------------------------------------
// Basic type synonyms and size independent types.
//----------------------------------------------------------------------

typedef char           Byte;
typedef signed char    Int8;
typedef unsigned char  UInt8;

// Void: each Void value is equivalent to any other Void value;
// useful as a domain of non-indexed data-items 
// can be the CountType of a ValueCountPair when the Counts are not needed, such as for UniqueValue 

struct Void 
{
	void operator ++() {}
	void operator +=(const Void&) {}
}; 

//----------------------------------------------------------------------
// Basic types with fixed size.
//----------------------------------------------------------------------

#if defined(CC_INT_16)
#pragma message("CC_INT_16")
	typedef int             Int16;
	typedef unsigned int    UInt16;
#elif defined(CC_SHORT_16)
	typedef short           Int16;
	typedef unsigned short  UInt16;
#else
#	error "Unable to define 16 bit integers."
#endif

#if defined(CC_FLOAT_32)
	typedef float				Float32;
#else
#	error "Unable to define 32 bit float."
#endif

#if defined(CC_DOUBLE_64)
	typedef double				Float64;
#else
#	error "Unable to define 64 bit float."
#endif

#if defined(CC_LONGDOUBLE_80)
	typedef long double			Float80;
#endif

#if defined(CC_WHCHAR_T)
	typedef wchar_t         Char16;
#else
	typedef Int16           Char16;
#endif

// Some Useful types that can be modified here
#if defined(__GNUC__)
	using UInt64 = uint64_t;
	using Int64 = int64_t;
	using UInt32 = uint32_t;
	using Int32 = int32_t;

#elif defined(_MSC_VER)
	using UInt64 = unsigned __int64;
	using Int64 = __int64;

#if defined(CC_INT_32)
	using Int32 = int;
	using UInt32 = unsigned int;
#elif defined(CC_LONG_32)
	typedef long            Int32;
	typedef unsigned long   UInt32;
#else
#	error "Unable to define 32 bit integers."
#endif

#else
#	error "Unable to define integers."
#endif

typedef UInt8       DimType;
typedef char        Char;
typedef Char const* CharPtr;
using SecsSince1970 = UInt32;
using ClockTicks = UInt32;
using strpos_t = UInt32;
using decpos_t = UInt8;
//  -----------------------------------------------------------------------
//  persistent real world connection
//  -----------------------------------------------------------------------

using FileDateTime     = UInt64; // connection with the real world (as represented by a time stamped Persistent File system); don't confuse it with TimeStamp, which sequences transcient events
using TimeStamp        = UInt32;    // only used for internal chronological ordering
using phase_number     = UInt32;
using interest_count_t = UInt32;

//----------------------------------------------------------------------
// Commonly used OS-API related types
//----------------------------------------------------------------------

typedef long          LONG;
typedef int           DMS_LONG;
typedef unsigned int  UINT;
#if defined(_MSC_VER)
typedef unsigned long DWORD;    // Windows LLP64: unsigned long is 32-bit
#else
typedef unsigned int  DWORD;    // Linux LP64: unsigned long is 64-bit, use unsigned int for 32-bit DWORD
#endif
//	typedef unsigned char UByte;

/* Types use for passing & returning polymorphic values */
#define STRICT
typedef void* HANDLE;
#define WINDECL_HANDLE(name) typedef struct name##__* name

#if !defined(_W64)
#if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) && _MSC_VER >= 1300
#define _W64 __w64
#else
#define _W64
#endif
#endif

#if defined(DMS_32)

	typedef _W64 UINT     UINT_PTR;
	typedef _W64 LONG     LONG_PTR;

#endif //defined(DMS_32)

#if defined(DMS_64)

	typedef uint64_t UINT_PTR;
	typedef int64_t  LONG_PTR;

#endif //defined(DMS_64)

/* Types use for passing & returning polymorphic values */
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

#define STRICT
typedef void* HANDLE;
#define WINDECL_HANDLE(name) typedef struct name##__* name

//----------------------------------------------------------------------
// Commonly used std related types
//----------------------------------------------------------------------

#include <utility>

namespace dms {
	using diff_type  = std::ptrdiff_t;
	using size_type  = std::size_t;
	using filesize_t = std::size_t;
}


//----------------------------------------------------------------------
// Memory model
//----------------------------------------------------------------------

using SizeT = UInt64;
using DiffT = Int64;
using TokenT = UInt32;

#endif // __CPC_TYPES_H
