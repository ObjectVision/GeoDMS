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

SharedStr AsDateTimeString(FileDateTime t);
SharedStr GetCurrentTimeStr();
SharedStr GetSessionStartTimeStr();

RTC_CALL FileDateTime AsFileDateTime(UInt32 hiDW, UInt32 loDW); // exported: stg ODBCImp references it in Debug links (/OPT:REF strips the reference in Release)

Int64 GetSecsSince1970();

#endif // __UTL_TIMEFMT_H
