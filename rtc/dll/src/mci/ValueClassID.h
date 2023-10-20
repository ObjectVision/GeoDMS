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

#if !defined(__RTC_MCI_VALUECLASSID_H)
#define __RTC_MCI_VALUECLASSID_H

//----------------------------------------------------------------------
// enumeration type for element-types
//----------------------------------------------------------------------
// An enumeration type that represents the values that can be put in a unit

#include "RtcTypeModel.h"

enum class ValueClassID : UInt8 {
	VT_UInt32 = 0,
	VT_Int32 = 1,
	VT_UInt16 = 2,
	VT_Int16 = 3,
	VT_UInt8 = 4,
	VT_Int8 = 5,

	VT_UInt64 = 6,
	VT_Int64 = 7,

	VT_Float64 = 8,
	VT_Float32 = 9,
	VT_Float80 = 10,

	VT_Bool = 11,
	VT_UInt1 = VT_Bool,
#if defined(DMS_TM_HAS_UINT2)
	VT_UInt2 = 12,
#endif
	VT_UInt4 = 13,

	VT_SPoint = 14,
	VT_WPoint = 15,
	VT_IPoint = 16,
	VT_UPoint = 17,
	VT_FPoint = 18,
	VT_DPoint = 19,

	VT_SRect = VT_SPoint + 6,
	VT_WRect = VT_WPoint + 6,
	VT_IRect = VT_IPoint + 6,
	VT_URect = VT_UPoint + 6,
	VT_FRect = VT_FPoint + 6,
	VT_DRect = VT_DPoint + 6,

#if defined(DMS_TM_HAS_INT_SEQ)
	VT_SArc = VT_SPoint + 12,
	VT_WArc = VT_WPoint + 12,
	VT_IArc = VT_IPoint + 12,
	VT_UArc = VT_UPoint + 12,
#endif
	VT_FArc = VT_FPoint + 12,
	VT_DArc = VT_DPoint + 12, // 31

	VT_FirstPolygon = 25,
#if defined(DMS_TM_HAS_INT_SEQ)
	VT_SPolygon = VT_SArc + 6,
	VT_WPolygon = VT_WArc + 6,
	VT_IPolygon = VT_IArc + 6,
	VT_UPolygon = VT_UArc + 6,
#endif
	VT_FPolygon = VT_FArc + 6,
	VT_DPolygon = VT_DArc + 6, // 37
	VT_FirstAfterPolygon = 38,

	VT_SharedStr = 38,
	VT_String = VT_SharedStr,
	VT_TokenID = 39,
	VT_Void = 40,

	VT_FirstRange = 41,
	VT_RangeUInt32 = VT_UInt32 + VT_FirstRange,
	VT_RangeInt32 = VT_Int32 + VT_FirstRange,
	VT_RangeUInt16 = VT_UInt16 + VT_FirstRange,
	VT_RangeInt16 = VT_Int16 + VT_FirstRange,
	VT_RangeUInt8 = VT_UInt8 + VT_FirstRange,
	VT_RangeInt8 = VT_Int8 + VT_FirstRange,

	VT_RangeUInt64 = VT_UInt64 + VT_FirstRange,
	VT_RangeInt64 = VT_Int64 + VT_FirstRange,

	VT_RangeFloat64 = VT_Float64 + VT_FirstRange,
	VT_RangeFloat32 = VT_Float32 + VT_FirstRange, // 50
	VT_RangeFloat80 = VT_Float80 + VT_FirstRange, // 51

	VT_NumSeqBase = 52,
	VT_UInt32Seq = VT_UInt32 + VT_NumSeqBase, // 52
	VT_Int32Seq = VT_Int32 + VT_NumSeqBase, // 53
	VT_UInt16Seq = VT_UInt16 + VT_NumSeqBase, // 54
	VT_Int16Seq = VT_Int16 + VT_NumSeqBase, // 55
	VT_UInt8Seq = VT_UInt8 + VT_NumSeqBase, // 56
	VT_Int8Seq = VT_Int8 + VT_NumSeqBase, // 57

	VT_UInt64Seq = VT_UInt64 + VT_NumSeqBase, // 58
	VT_Int64Seq = VT_Int64 + VT_NumSeqBase, // 59

	VT_Float64Seq = VT_Float64 + VT_NumSeqBase, // 60
	VT_Float32Seq = VT_Float32 + VT_NumSeqBase, // 61
	VT_Float80Seq = VT_Float80 + VT_NumSeqBase, // 62
	VT_Count = 63,
	VT_Unknown  = 64
};

#endif // __RTC_MCI_VALUECLASSID_H
