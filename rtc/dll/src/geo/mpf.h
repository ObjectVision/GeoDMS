
// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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
