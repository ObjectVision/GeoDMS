// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_MEM_FIXED_ALLOC_H)
#define __RTC_MEM_FIXED_ALLOC_H

/*
	fixed_allocator is an allocator that has an efficient
	memory management implementation for allocating limited-size objects.
	It can only allocate and deallocate objects of the same size by
	maintaining a linked list of free areas. The area that is managed
	is allocated by a suppying allocator, called ArrayAlloc, which is used
	to claim large blocks at a time.

	Complexity:

	Apart from the incidental large block allocation, allocation and
	deallocation is done in constant time.

	Template Parameters:

	BclokAlloc, the original allocator for type T

	Defines:

	define MG_DEBUG_ALLOCATOR to maintain allocation statistics in RtcBase.h

	This allocator fulfills the allocator requirements as described in
	section 20.1 of the April 1995 WP and the STL specification, as
	documented by A. Stephanov & M. Lee, December 6, 1994.
*/

#include "RtcBase.h"
#include <atomic>

#if defined(MG_DEBUG)
//#define MG_DEBUG_ALLOCATOR
#endif

// #define MG_DEBUG_ALLOCATOR

#if defined(MG_DEBUG_ALLOCATOR)

#define MG_DEBUG_ALLOCATOR_TXT(X) X
#define MG_DEBUG_ALLOCATOR_FIRST(X) X,
#define MG_DEBUG_ALLOCATOR_SRC(X) ,X
#define MG_DEBUG_ALLOCATOR_SRC_IS    MG_DEBUG_ALLOCATOR_SRC("IndexedString")
#define MG_DEBUG_ALLOCATOR_SRC_SA    MG_DEBUG_ALLOCATOR_SRC("SequenceArrayString")
#define MG_DEBUG_ALLOCATOR_SRC_EMPTY MG_DEBUG_ALLOCATOR_SRC("")
#define MG_DEBUG_ALLOCATOR_TXT_ARG   MG_DEBUG_ALLOCATOR_TXT(CharPtr srcStr)
#define MG_DEBUG_ALLOCATOR_FIRST_ARG   MG_DEBUG_ALLOCATOR_FIRST(CharPtr srcStr)
#define MG_DEBUG_ALLOCATOR_SRC_ARG   MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr)
#define MG_DEBUG_ALLOCATOR_SRC_ARG_D MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "")
#define MG_DEBUG_ALLOCATOR_TXT_PARAM MG_DEBUG_ALLOCATOR_TXT(srcStr)
#define MG_DEBUG_ALLOCATOR_FIRST_PARAM MG_DEBUG_ALLOCATOR_FIRST(srcStr)
#define MG_DEBUG_ALLOCATOR_SRC_PARAM MG_DEBUG_ALLOCATOR_SRC(srcStr)

#else //defined(MG_DEBUG_ALLOCATOR)

#define MG_DEBUG_ALLOCATOR_TXT(X)
#define MG_DEBUG_ALLOCATOR_SRC(X)
#define MG_DEBUG_ALLOCATOR_FIRST(X)
#define MG_DEBUG_ALLOCATOR_SRC_IS
#define MG_DEBUG_ALLOCATOR_SRC_SA
#define MG_DEBUG_ALLOCATOR_SRC_EMPTY
#define MG_DEBUG_ALLOCATOR_TXT_ARG
#define MG_DEBUG_ALLOCATOR_SRC_ARG
#define MG_DEBUG_ALLOCATOR_FIRST_ARG
#define MG_DEBUG_ALLOCATOR_SRC_ARG_D
#define MG_DEBUG_ALLOCATOR_TXT_PARAM
#define MG_DEBUG_ALLOCATOR_FIRST_PARAM
#define MG_DEBUG_ALLOCATOR_SRC_PARAM

#endif //defined(MG_DEBUG_ALLOCATOR)

// Per-operation allocation attribution: every >=4 KiB block AllocateFromStock hands out is
// registered to the OperationContext that allocated it (a mutexed std::map insert/erase per
// (de)allocation), which is what lets a FREE discharge the operation that allocated -- the basis
// of the per-operation live/peak figures the memory-estimate calibration is graded against.
//
// ON for calibration runs, OFF for production and performance runs: the register is the cost, and
// without it the per-operation counters cannot be maintained, so those members and their charge
// entry points are compiled out along with it (OperationContext.h). The always-on cheap counters
// (PeakLiveLarge, the size histogram, the admission ledger) do NOT depend on this switch.
//
// Lives here, not in FixedAlloc.cpp: OperationContext.h conditions data members on it, so every
// module must see the same setting -- a header both include is what guarantees that.
// See doc/development/schedule-with-lookahead.md SS8.1.13/8.1.19.
// OFF for the 20.10.0 release builds (2026-08-05): calibration instrumentation only.
// #define MG_CACHE_COLLECTDATA

// Free-store drainage (doc/development/schedule-with-lookahead.md SS8.1.32): give freed < 2 MB
// object stores back to the OS once the machine's RAM use passes MemoryFlushThreshold. On by
// default; /CF (or MemoryDrainage=0) switches it off. Cached like PerformanceLogging, so the
// allocation path pays one relaxed load -- and the setter is what makes /CF take effect at once
// instead of at the next pressure poll.
RTC_CALL void SetFreeStackDrainageEnabled(bool enabled);
RTC_CALL bool IsFreeStackDrainageEnabled();

// Bytes of freed-but-still-committed object stores: the reclaimable dead pool. Drainage hands it
// back on demand, so an occupancy measure that counts it overstates what the process really holds.
RTC_CALL SizeT GetFreeStackDeadBytes();

RTC_CALL void* AllocateFromStock(size_t sz MG_DEBUG_ALLOCATOR_SRC_ARG);
RTC_CALL void  LeaveToStock(void* ptr, size_t sz);
extern std::atomic<bool> s_ReportingRequestPending;
extern std::atomic<bool> s_BlockNewAllocations;
void ReportFixedAllocStatus();
RTC_CALL void ReportFixedAllocFinalSummary();

#endif // __RTC_MEM_FIXED_ALLOC_H
