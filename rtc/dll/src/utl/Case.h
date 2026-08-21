// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __UTL_CASE_H
#define __UTL_CASE_H

#include "RtcBase.h"

struct SharedStr;

RTC_CALL void UpperCase(StringRef& ref, CharPtr b, CharPtr e);
RTC_CALL void LowerCase(StringRef& ref, CharPtr b, CharPtr e);

SharedStr AsLowerCase(CharPtr b, CharPtr e);
SharedStr AsLowerCase(CharPtr zStr);


#endif // __UTL_CASE_H