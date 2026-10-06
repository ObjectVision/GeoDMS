// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_MTH_NIGINT_H)
#define __RTC_MTH_NIGINT_H

#include "RtcBase.h"

/******************************************************************************/
//                         (U)Int64 functions
/******************************************************************************/

RTC_CALL UInt64 RTC_UInt32xUInt32toUInt64(UInt32 a, UInt32 b); // calls UInt32x32To64
RTC_CALL Int64  RTC_Int32xInt32toInt64(Int32 a, Int32 b); // calls Int32x32To64

#endif //!defined(__RTC_MTH_NIGINT_H)
