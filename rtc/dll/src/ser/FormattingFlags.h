// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_SER_FORMATTINGFLAGS_H)
#define __RTC_SER_FORMATTINGFLAGS_H

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
