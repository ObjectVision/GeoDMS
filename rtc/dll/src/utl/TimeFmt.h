// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Date/time formatting: FileDateTime construction and its readable
 *  rendering, current and session-start time strings, and seconds since
 *  1970. Split out of utl/Environment.h (header-hygiene-2026-08.md §5A).
 */

#if !defined(__UTL_TIMEFMT_H)
#define __UTL_TIMEFMT_H

#include "cpc/Types.h"
#include "ptr/SharedStr.h"

RTC_CALL SharedStr AsDateTimeString(FileDateTime t);
RTC_CALL SharedStr GetCurrentTimeStr();
RTC_CALL SharedStr GetSessionStartTimeStr();

RTC_CALL FileDateTime AsFileDateTime(UInt32 hiDW, UInt32 loDW);

RTC_CALL Int64 GetSecsSince1970();

#endif // __UTL_TIMEFMT_H
