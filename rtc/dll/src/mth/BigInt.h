// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_MTH_NIGINT_H)
#define __RTC_MTH_NIGINT_H

#include "RtcBase.h"

#include "geo/BaseBounds.h"

#include <vector>


/******************************************************************************/
//                          struct UInt
/******************************************************************************/
namespace Big {
	typedef UInt32                 radix_type;
	typedef std::vector<radix_type> container_type;

	struct UInt : container_type
	{		
		UInt() {}
		RTC_CALL explicit UInt(radix_type v);

		RTC_CALL void operator +=(const UInt& rhs);
		RTC_CALL void operator *=(const UInt& rhs);
		RTC_CALL void operator /=(const UInt& rhs);
		RTC_CALL void operator >>=(size_type shift);

		RTC_CALL void operator *=(UInt32 factor);
		RTC_CALL void operator ++();
		RTC_CALL void StripLSB(UInt32 p);
		RTC_CALL size_type ShiftToOdd();

		inline bool IsOne() const { return size() == 1 && (*this)[0] == 1; }

	private:
		RTC_CALL void add(const UInt& rhs, size_type shift);
	};

/******************************************************************************/
//                          UInt operations
/******************************************************************************/

	RTC_CALL UInt operator + (const UInt& a, const UInt& b);
	RTC_CALL UInt operator * (const UInt& a, const UInt& b);
	RTC_CALL UInt operator / (const UInt& a, const UInt& b);
	RTC_CALL UInt operator >>(const UInt& a, UInt32 shift);

} // namespace Big

/******************************************************************************/
//                         (U)Int64 functions
/******************************************************************************/

RTC_CALL UInt64 RTC_UInt32xUInt32toUInt64(UInt32 a, UInt32 b); // calls UInt32x32To64
RTC_CALL Int64  RTC_Int32xInt32toInt64(Int32 a, Int32 b); // calls Int32x32To64

#endif //!defined(__RTC_MTH_NIGINT_H)
