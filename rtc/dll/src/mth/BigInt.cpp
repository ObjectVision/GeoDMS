// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "mth/BigInt.h"

/******************************************************************************/
//                         (U)Int64 functions
/******************************************************************************/

RTC_CALL UInt64 RTC_UInt32xUInt32toUInt64(UInt32 a, UInt32 b)
{
	return UInt64(a) * UInt64(b);
}

RTC_CALL Int64 RTC_Int32xInt32toInt64(Int32 a, Int32 b)
{
	return Int64(a) * Int64(b);
}
