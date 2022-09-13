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

#if !defined(__STGIMPL_IMPLMAIN_H)
#define __STGIMPL_IMPLMAIN_H

#include "RtcBase.h"
#include "ptr/SharedStr.h"

#ifdef DMSTGIMPL_EXPORTS
#	define STGIMPL_CALL __declspec(dllexport)
#else
#	ifdef DMSTGIMPL_STATIC
#		define STGIMPL_CALL
#	else
#		define STGIMPL_CALL __declspec(dllimport)
#	endif
#endif

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
