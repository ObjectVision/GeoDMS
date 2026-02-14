// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_FILERESULT_H)
#define __RTC_FILERESULT_H

#include <expected>

#include "RtcBase.h"

//======================================= FileResult

struct [[nodiscard]] FileResult : std::expected<void, SharedStr>
{
	using std::expected<void, SharedStr>::expected;

	[[noreturn]] RTC_CALL void Throw(CharPtr context) const;

	static FileResult require(bool ok, CharPtr msg)
	{
		if (ok) return {};
		return std::unexpected(SharedStr(msg));
	}
};


#endif // __RTC_FILERESULT_H
