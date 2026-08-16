// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Win32/OS error reporting: GetSystemErrorText and the
 *  throwSystemError/throwLastSystemError helpers that decorate a failing
 *  system call with its OS error text (plus the MAX_PATH hint), and the
 *  retry loop ManageSystemError. Split out of utl/Environment.h
 *  (header-hygiene-2026-08.md §5A).
 */

#if !defined(__UTL_PLATFORMERROR_H)
#define __UTL_PLATFORMERROR_H

#include "cpc/Types.h"
#include "dbg/Check.h"
#include "ptr/SharedStr.h"

namespace platform {
	RTC_CALL SharedStr GetSystemErrorText(DWORD lastErr);
	RTC_CALL DWORD GetLastError();
	RTC_CALL bool isCharPtrAndExceeds_MAX_PATH(CharPtr xFileName);
}

template<typename T>
bool isCharPtrAndExceeds_MAX_PATH(const T& xFileName) { return false;  }

template<typename ...Args>
[[noreturn]] void throwSystemError(DWORD lastErr, CharPtr format, Args&&... args)
{
	throwErrorF("WindowsSystem", "{}:\nErrorCode {}: {}{}"
	,	mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str()
	,	lastErr
	,	::platform::GetSystemErrorText(lastErr).c_str()
	,	(... || isCharPtrAndExceeds_MAX_PATH(args)) ? "\nNote that filenames cannot be longer than 260 characters" : ""
	);
}

template<typename ...Args>
[[noreturn]] void throwLastSystemError(CharPtr format, Args&&... args) {
	throwSystemError<Args...>(::platform::GetLastError(), format, std::forward<Args>(args)...);
}

bool ManageSystemError(UInt32& retryCounter, CharPtr format, CharPtr fileName, bool throwOnError, bool doRetry);

#endif // __UTL_PLATFORMERROR_H
