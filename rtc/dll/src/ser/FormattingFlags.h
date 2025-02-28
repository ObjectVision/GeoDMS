// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SER_FORMATTINGFLAGS_H)
#define __RTC_SER_FORMATTINGFLAGS_H

#include "RtcBase.h"

//----------------------------------------------------------------------
// FormattingFlags
//----------------------------------------------------------------------

enum class FormattingFlags : UInt32 {
	None = 0,
	ThousandSeparator = 1 << 0,
	NoLimitInLispExpr = 1 << 1, 

	StreamFlags= ThousandSeparator,
	LispFlags = NoLimitInLispExpr,
};

// Provide a bitwise AND operator for FormattingFlags:
constexpr inline FormattingFlags operator &(FormattingFlags lhs, FormattingFlags rhs) noexcept
{
	using T = std::underlying_type_t<FormattingFlags>;
	static_assert(std::is_integral<T>::value, "Underlying type must be an integral type");
	return static_cast<FormattingFlags>(static_cast<T>(lhs) & static_cast<T>(rhs));
}


inline FormattingFlags StreamFlags(FormattingFlags ff) { return ff & FormattingFlags::StreamFlags; }
inline FormattingFlags LispFlags  (FormattingFlags ff) { return ff & FormattingFlags::LispFlags; }
inline bool HasThousandSeparator(FormattingFlags ff) { return (ff & FormattingFlags::ThousandSeparator) != FormattingFlags::None; }
inline bool HasNoLimitInLispExpr(FormattingFlags ff) { return (ff & FormattingFlags::NoLimitInLispExpr) != FormattingFlags::None; }


#endif // __RTC_SER_FORMATTINGFLAGS_H
