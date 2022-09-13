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

/*
 *  Name        : rtc/cpc/Types.h
 *  Description : Basic types as a result of the compiler & platform configuration
 */

#if !defined(__CPC_TYPES_H)
#define __CPC_TYPES_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "cpc/CompChar.h"
#include "RtcTypeModel.h"

#define TYPEID(T) ((const T*)nullptr)

//----------------------------------------------------------------------
// Basic type synonyms and size independent types.
//----------------------------------------------------------------------

typedef char           Byte;
typedef signed char    Int8;
typedef unsigned char  UInt8;
//typedef void         Void;    // useful as a domain of non-indexed data-items 
struct                 Void {}; // fix problem with const Void& in some instantiations

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

#if defined(CC_INT_32)
	typedef int             Int32;
	typedef unsigned int    UInt32;
#elif defined(CC_LONG_32)
	typedef long            Int32;
	typedef unsigned long   UInt32;
#else
#	error "Unable to define 32 bit integers."
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
typedef unsigned __int64 UInt64;
typedef          __int64 Int64;

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
using interest_count_t = UInt32;

//----------------------------------------------------------------------
// Commonly used OS-API related types
//----------------------------------------------------------------------

typedef long          LONG;
typedef int           DMS_LONG;
typedef unsigned int  UINT;
typedef unsigned long DWORD;
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

	typedef unsigned __int64 UINT_PTR;
	typedef __int64          LONG_PTR;

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
	typedef _W64 std::ptrdiff_t diff_type;
	typedef _W64 std::size_t size_type;
	typedef std::size_t filesize_t;
}


//----------------------------------------------------------------------
// Memory model
//----------------------------------------------------------------------

#include "RtcTypeModel.h"

typedef UInt64 SizeT;
typedef Int64  DiffT;
typedef UInt32 TokenT;

#endif // __CPC_TYPES_H
