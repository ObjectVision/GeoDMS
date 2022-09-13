
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

#if !defined(__RTC_GEO_UNDEF_XX_H)
#define __RTC_GEO_UNDEF_XX_H

#include "geo/Undefined.h"
#include "mci/ValueClassID.h"
#include "dbg/Check.h"

//----------------------------------------------------------------------
// Section      : Undef for valueClass for specific sizes.
//----------------------------------------------------------------------

inline UInt8 Undef08(ValueClassID streamTypeID)
{
	switch (streamTypeID) {
	case VT_Int8:   return UNDEFINED_VALUE(Int8);
	}
	dms_assert(streamTypeID == VT_UInt8);
	return UNDEFINED_VALUE(UInt8);

}

inline UInt16 Undef16(ValueClassID streamTypeID)
{
	switch (streamTypeID) {
	case VT_Int16:   return UNDEFINED_VALUE(Int16);
	}
	dms_assert(streamTypeID == VT_UInt16);
	return UNDEFINED_VALUE(UInt16);
}

inline UInt32 Undef32(ValueClassID streamTypeID)
{
	switch (streamTypeID) {
	case VT_Int32:
		return UNDEFINED_VALUE(Int32);
	case VT_Float32:
		Float32 undef = UNDEFINED_VALUE(Float32);
		return reinterpret_cast<UInt32&>(undef);
	}
	dms_assert(streamTypeID == VT_UInt32);
	return UNDEFINED_VALUE(UInt32);
}

inline UInt64 Undef64(ValueClassID streamTypeID)
{
	switch (streamTypeID) {
	case VT_Int64:
		return UNDEFINED_VALUE(Int64);
	case VT_Float64:
		Float64 undef = UNDEFINED_VALUE(Float64);
		return reinterpret_cast<UInt64&>(undef);
	}
	dms_assert(streamTypeID == VT_UInt64);
	return UNDEFINED_VALUE(UInt64);
}

#endif // !defined(__RTC_GEO_UNDEF_XX_H)
