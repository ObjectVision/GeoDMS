
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

#if !defined(__RTC_GEO_MPF_H)
#define __RTC_GEO_MPF_H

#include "RtcBase.h"

#include <type_traits>

//======================== mpl: meta programmed functions (compile time)

namespace mpf {

	template <bit_size_t i> struct exp2
	{
		static const auto value = std::integral_constant<std::size_t, (01UL << i)>::value;
		static_assert(exp2::value && (i>=0));
	};

	template <bit_block_t i> struct log2
	{
		static const auto value = std::integral_constant<std::size_t, log2<(i >> 1)>::value + 1>::value;
		static_assert(exp2<value>::value == i);  // we don't want to solve roundoff issues, so i must be a power of 2
	}; 

	template <> struct log2<1>
	{
		static const auto value = std::integral_constant< std::size_t, 0>::value;
	};

	template <bit_block_t i> const std::size_t log2_v = log2<i>::value;
}


#endif // !defined(__RTC_GEO_MPF_H)
