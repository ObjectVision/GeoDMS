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

enum class FormattingFlags {
	None = 0,
	ThousandSeparator = 1,
	NoLimitInLispExpr = 2,

	StreamFlags= ThousandSeparator,
	LispFlags = NoLimitInLispExpr,
};

// Provide a bitwise AND operator for FormattingFlags:
inline FormattingFlags operator &(FormattingFlags lhs, FormattingFlags rhs)
{
	using T = std::underlying_type_t<FormattingFlags>;
	return static_cast<FormattingFlags>(static_cast<T>(lhs) & static_cast<T>(rhs));
}


inline FormattingFlags StreamFlags(FormattingFlags ff) { return ff & FormattingFlags::StreamFlags; }
inline FormattingFlags LispFlags  (FormattingFlags ff) { return ff & FormattingFlags::LispFlags; }
inline bool HasThousandSeparator(FormattingFlags ff) { return (ff & FormattingFlags::ThousandSeparator) != FormattingFlags::None; }
inline bool HasNoLimitInLispExpr(FormattingFlags ff) { return (ff & FormattingFlags::NoLimitInLispExpr) != FormattingFlags::None; }


#endif // __RTC_SER_FORMATTINGFLAGS_H
