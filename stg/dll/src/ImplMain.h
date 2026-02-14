// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STGIMPL_IMPLMAIN_H)
#define __STGIMPL_IMPLMAIN_H

#include "RtcBase.h"
#include "StgBase.h"

//	define coulore locale
using Boolean = bool;
using UByte = UInt8;
using Short = short;
using UShort = unsigned short;
using Long = long;
using Double = double;
using Float = float;

//	define common OS dependent structures
struct tagRGBQUAD;
using RGBQUAD = tagRGBQUAD;


struct SAMPLEFORMAT
{
	enum ENUM {
		SF_UINT = 1, /* !unsigned integer data */
		SF_INT = 2, /* !signed integer data */
		SF_IEEEFP = 3, /* !IEEE floating point data */
		SF_VOID = 4, /* !untyped data */
		SF_COMPLEXINT = 5, /* !complex signed int */
		SF_COMPLEXIEEEFP = 6  /* !complex ieee floating */
	} m_Value;

	STGIMPL_CALL SAMPLEFORMAT(bool isSigned, bool isFloat, bool isComplex);
	STGIMPL_CALL SAMPLEFORMAT(const ValueClass* vc);
};


#endif // __STGIMPL_IMPLMAIN_H
