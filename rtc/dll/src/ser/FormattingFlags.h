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

inline FormattingFlags StreamFlags(FormattingFlags ff) { return FormattingFlags(UInt32(ff) & UInt32(FormattingFlags::StreamFlags)); }
inline FormattingFlags LispFlags  (FormattingFlags ff) { return FormattingFlags(UInt32(ff) & UInt32(FormattingFlags::LispFlags)); }
inline bool HasThousandSeparator(FormattingFlags ff) { return UInt32(ff) & UInt32(FormattingFlags::ThousandSeparator);  }
inline bool HasNoLimitInLispExpr(FormattingFlags ff) { return UInt32(ff) & UInt32(FormattingFlags::NoLimitInLispExpr); }


#endif // __RTC_SER_FORMATTINGFLAGS_H
