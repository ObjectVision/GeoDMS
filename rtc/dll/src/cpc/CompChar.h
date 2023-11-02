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

#if !defined(__CPC_COMPCHAR_H)
#define __CPC_COMPCHAR_H

#if defined(_INC_WINDOWS)
#	error("windows.h was included before cpc/CompChar.h: change it.")
#endif

/****************************************************************
 *                                                              *
 *              U S E R   T U N A B L E   S E C T I O N         *
 *                                                              *
 ****************************************************************/

/*
 * This section contains various preprocessor constants that can
 * be set to reflect the properties of your compiler.  For most
 * compilers, settings can be autodetected.
 *
 *	Recent compilation has been done with:
 *		Microsoft Visual C++ 2019, 2022
 */

/****************************************************************
 *                                                              *
 *        C O M P I L E R    D E T E C T I O N                  *
 *                                                              *
 ****************************************************************/

// Recognize compiler and set byteorder, some defines and datatypes

//----------------------------------------------------------------------
// Compiler     : Intel C++
//----------------------------------------------------------------------
#if defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)

#	if defined(CC_COMPILER_NAME)
#		error Second compiler recognized!
#	endif

#	define CC_COMPILER_NAME "Intel C++"
#	define CC_PROCESSOR_INTEL
#	define CC_BYTEORDER_INTEL
#	define CC_POCESSOR_MINVERSION 80486
#	define CC_NO_VTAB_BY_EXPLICIT_INSTANTIATION
#	define CC_UNWINDING_ADM_PROBLEM
// for some reasons my Intel C++ 8.0 in MSVC6.0 has _WCHAR_T_DEFINED but no intrinsic wchar_c,
// so logic as indicated in http://aspn.activestate.com/ASPN/Mail/Message/boost/1614864 fails
#	define BOOST_NO_INTRINSIC_WCHAR_T

#	pragma warning( disable : 525) //  type "RefPtr<I, C>::IPtr" is an inaccessible type (allowed for compatibility)
#	pragma warning( disable :1572) // floating-point equality and inequality comparisons are unreliable
#	pragma warning( disable :1563) // taking the address of a temporary

#	define CC_SHORT_16
#	define CC_INT_32
#	define CC_LONG_32
#	define CC_FLOAT_32
#	define CC_DOUBLE_64
#	define CC_LONGDOUBLE_80

//----------------------------------------------------------------------
// Compiler     : Microsoft Visual C++ (_MSC_VER)
//----------------------------------------------------------------------

#elif defined(_MSC_VER)
#	if defined(CC_COMPILER_NAME)
#		error Second compiler recognized!
#	endif

#	define CC_BYTEORDER_INTEL

#	if _MSC_VER < 1930
#		error "This GeoDMS source is only valid for Microsoft Visual C++ 2022 Version 17.0 or newer"
#	endif
#	define CC_COMPILER_NAME "Microsoft Visual C++"

#	pragma warning( disable : 4200) // nonstandard extension used : zero-sized array in struct/union
#	pragma warning( disable : 4355) // use 'this' in base member initializer list without nagging

#	define CC_STL_1300
#	define CC_ITERATOR_CHECKED
#	define CC_HAS_OVERRIDE
//REMOVE#	define CC_FIX_ASSERT
#	pragma warning( disable : 4244) // conversion, possible loss of data
#	pragma warning( disable : 4267) // conversion, possible loss of data
#	pragma warning( disable : 4646) // function declared with __declspec(noreturn) has non-void return type
#	pragma warning( disable : 4800) // forcing value to bool 'true' or 'false' (performance warning)
#	pragma warning( disable : 4996) // '_vsnprintf' was declared deprecated

#	define CC_SHORT_16
#	define CC_INT_32
#	define CC_LONG_32
#	define CC_FLOAT_32
#	define CC_DOUBLE_64
#	define CC_LONGDOUBLE_64
#	define CC_PROCESSOR_INTEL
#	define CC_POCESSOR_MINVERSION 80486
// See http://msdn.microsoft.com/en-us/library/vstudio/hh697468.aspx
//#	define _ITERATOR_DEBUG_LEVEL 0
//# define _HAS_ITERATOR_DEBUGGING	0
//# define _SECURE_SCL 0
#	pragma warning( disable : 4520) // multiple default constructors specified (1 explicit and 1 by variadic list).

//  boost parameterization

#	define BOOST_FALLTHROUGH [[fallthrough]]

#endif

//----------------------------------------------------------------------
// Compiler     : GCC
//----------------------------------------------------------------------
#ifdef CC_WINGCC

#  ifdef CC_COMPILER_NAME
#    error Second compiler recognized!
#  endif
#	define CC_BYTEORDER_INTEL
#	define CC_SHORT_16
#	define CC_INT_32
#	define CC_LONG_32
#	define CC_FLOAT_32
#	define CC_DOUBLE_64
//#	define CC_LONGDOUBLE_80
#	define CC_PROCESSOR_INTEL
#	define CC_POCESSOR_MINVERSION 80486

#  define CC_COMPILER_NAME "gcc compiler"
#endif

// Stop compiling if there is no compiler recognized in the above statements.
#if ! defined(CC_COMPILER_NAME)
#  error Unknown compiler
#endif

//----------------------------------------------------------------------
// Compiler     : RRRE 4.0.3
//----------------------------------------------------------------------
#ifdef __rrre
#  ifdef CC_COMPILER_NAME
#    error Second compiler recognized!
#  endif
#  define CC_BYTEORDER_INTEL
#  define CC_COMPILER_NAME "rational rose reverse engineering"
#endif

// Stop compiling if there is no compiler recognized in the above statements.
#if ! defined(CC_COMPILER_NAME)
#  error Unknown compiler
#endif

#if defined(CC_TRACE_COMPILER)
#	pragma message(CC_COMPILER_NAME)
#endif

// #undef CC_COMPILER_NAME

//----------------------------------------------------------------------
// End of Compiler capabilities, now the STD capabilities based on boost/confdig
//----------------------------------------------------------------------

#if defined(_DEBUG)
#  define _STLP_DEBUG
#endif

// REMOVE #include <boost/config.hpp>

#if defined(__SGI_STL_PORT) || defined(_STLPORT_VERSION)
	#define CC_STL_PORT
#endif
//----------------------------------------------------------------------
// End STL capabilities, now use these settings
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// assert
//----------------------------------------------------------------------

#if defined(CC_ITERATOR_CHECKED)
	#define assert_iter(X) 
#else
	#define assert_iter(X) dms_assert(X)
#endif

//----------------------------------------------------------------------
// override
//----------------------------------------------------------------------

#if !defined(CC_HAS_OVERRIDE)
#define override
#endif

//----------------------------------------------------------------------
// Macro's with platform dependent constants
//----------------------------------------------------------------------

#define DELIMITER_CHAR '/'

#endif // __CPC_COMPCHAR_H
