// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_MCI_VALUECLASSID_H)
#define __RTC_MCI_VALUECLASSID_H

//----------------------------------------------------------------------
// enumeration type for element-types
//----------------------------------------------------------------------
// An enumeration type that represents the values that can be put in a unit

#include "rtctypemodel.h"

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

	NrPointTypes = 6,

	VT_SRect = VT_SPoint + NrPointTypes,
	VT_WRect = VT_WPoint + NrPointTypes,
	VT_IRect = VT_IPoint + NrPointTypes,
	VT_URect = VT_UPoint + NrPointTypes,
	VT_FRect = VT_FPoint + NrPointTypes,
	VT_DRect = VT_DPoint + NrPointTypes,

	VT_SArc = VT_SPoint + 2 * NrPointTypes,
	VT_WArc = VT_WPoint + 2 * NrPointTypes,
	VT_IArc = VT_IPoint + 2 * NrPointTypes,
	VT_UArc = VT_UPoint + 2 * NrPointTypes,

	VT_FArc = VT_FPoint + 2 * NrPointTypes,
	VT_DArc = VT_DPoint + 2 * NrPointTypes, // 31

	VT_FirstPolygon = 32,

	VT_SPolygon = VT_SArc + NrPointTypes,
	VT_WPolygon = VT_WArc + NrPointTypes,
	VT_IPolygon = VT_IArc + NrPointTypes,
	VT_UPolygon = VT_UArc + NrPointTypes,

	VT_FPolygon = VT_FArc + NrPointTypes,
	VT_DPolygon = VT_DArc + NrPointTypes, // 37
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


inline bool IsPolygonType(ValueClassID vid)
{
	return (vid >= ValueClassID::VT_FirstPolygon && vid < ValueClassID::VT_FirstAfterPolygon);
}


#endif // __RTC_MCI_VALUECLASSID_H
