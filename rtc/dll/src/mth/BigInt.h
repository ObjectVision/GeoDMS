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

#if !defined(__RTC_MTH_NIGINT_H)
#define __RTC_MTH_NIGINT_H

#include "RtcBase.h"

#include "geo/SeqVector.h"

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
