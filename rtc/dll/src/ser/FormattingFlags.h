// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SER_FORMATTINGFLAGS_H)
#define __RTC_SER_FORMATTINGFLAGS_H

#include "RtcBase.h"
#include "set/BitmaskEnum.h"

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
template <> inline constexpr bool is_bitmask_enum<FormattingFlags> = true;

inline FormattingFlags StreamFlags(FormattingFlags ff) { return ff & FormattingFlags::StreamFlags; }
inline FormattingFlags LispFlags  (FormattingFlags ff) { return ff & FormattingFlags::LispFlags; }
inline bool HasThousandSeparator(FormattingFlags ff) { return flags(ff & FormattingFlags::ThousandSeparator); }
inline bool HasNoLimitInLispExpr(FormattingFlags ff) { return flags(ff & FormattingFlags::NoLimitInLispExpr); }


#endif // __RTC_SER_FORMATTINGFLAGS_H
