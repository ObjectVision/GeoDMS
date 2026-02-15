
// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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
	case ValueClassID::VT_Int8: return UNDEFINED_VALUE(Int8);
	}
	assert(streamTypeID == ValueClassID::VT_UInt8);
	return UNDEFINED_VALUE(UInt8);

}

inline UInt16 Undef16(ValueClassID streamTypeID)
{
	switch (streamTypeID) {
	case ValueClassID::VT_Int16:   return UNDEFINED_VALUE(Int16);
	}
	assert(streamTypeID == ValueClassID::VT_UInt16);
	return UNDEFINED_VALUE(UInt16);
}

inline UInt32 Undef32(ValueClassID streamTypeID)
{
	switch (streamTypeID) {
	case ValueClassID::VT_Int32:
		return UNDEFINED_VALUE(Int32);
	case ValueClassID::VT_Float32:
		constexpr Float32 undef = UNDEFINED_VALUE(Float32);
		return reinterpret_cast<const UInt32&>(undef);
	}
	assert(streamTypeID == ValueClassID::VT_UInt32);
	return UNDEFINED_VALUE(UInt32);
}

inline UInt64 Undef64(ValueClassID streamTypeID)
{
	switch (streamTypeID) {
	case ValueClassID::VT_Int64:
		return UNDEFINED_VALUE(Int64);
	case ValueClassID::VT_Float64:
		constexpr Float64 undef = UNDEFINED_VALUE(Float64);
		return reinterpret_cast<const UInt64&>(undef);
	}
	assert(streamTypeID == ValueClassID::VT_UInt64);
	return UNDEFINED_VALUE(UInt64);
}

#endif // !defined(__RTC_GEO_UNDEF_XX_H)
