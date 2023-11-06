// Copyright (C) 2023 Object Vision b.v. 
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
	ThousandSeparator = 1
};

inline bool HasThousandSeparator(FormattingFlags ff) { return UInt32(ff) & UInt32(FormattingFlags::ThousandSeparator);  }


#endif // __RTC_SER_FORMATTINGFLAGS_H
