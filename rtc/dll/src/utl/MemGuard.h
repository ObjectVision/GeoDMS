// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__RTC_UTL_MEMGUARD_H)
#define __RTC_UTL_MEMGUARD_H

#include "RtcBase.h"

void WaitForAvailableMemory(std::size_t requestedSize = 0);
void ConsiderMakingFreeSpace(SizeT sz);

RTC_CALL bool IsLowOnFreeRAM();

// Physical memory this process is allowed to consider, i.e. the machine's RAM after the
// MemoryRAM_MAX_GB clamp that simulates a smaller machine. The budget a resource-aware scheduler
// divides up (doc/development/schedule-with-lookahead.md §5.1). 0 when it cannot be determined.
SizeT TotalAllowedPhysicalMemory();


#endif // __RTC_UTL_MEMGUARD_H
