// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__STGIMPL_IMPLMAIN_H)
#define __STGIMPL_IMPLMAIN_H

#include "RtcBase.h"
#include "StgBase.h"
#include "ptr/SharedStr.h"

//	define coulore locale
typedef bool           Boolean;

typedef UInt8          UByte;
typedef short          Short;
typedef unsigned short UShort;
typedef long           Long;
typedef double         Double;
typedef float          Float;

//	define common OS dependent structures
typedef struct tagRGBQUAD RGBQUAD;


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
	//	operator ENUM () const { return m_Value; }
};


#endif // __STGIMPL_IMPLMAIN_H
