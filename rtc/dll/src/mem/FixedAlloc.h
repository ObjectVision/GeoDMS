// Copyright (C) 2023 Object Vision b.v. 
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

	define MG_DEBUG_ALLOCATOR to maintain allocation statistics.

	This allocator fulfills the allocator requirements as described in
	section 20.1 of the April 1995 WP and the STL specification, as
	documented by A. Stephanov & M. Lee, December 6, 1994.
*/

#include "RtcBase.h"
#include <atomic>

#if defined(MG_DEBUG)
//#define MG_DEBUG_ALLOCATOR
#endif

#if defined(MG_DEBUG_ALLOCATOR)

#define MG_DEBUG_ALLOCATOR_SRC(X) ,X
#define MG_DEBUG_ALLOCATOR_SRC_IS    MG_DEBUG_ALLOCATOR_SRC("IndexedString")
#define MG_DEBUG_ALLOCATOR_SRC_SA    MG_DEBUG_ALLOCATOR_SRC("SequenceArrayString")
#define MG_DEBUG_ALLOCATOR_SRC_EMPTY MG_DEBUG_ALLOCATOR_SRC("")
#define MG_DEBUG_ALLOCATOR_SRC_ARG   MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr)
#define MG_DEBUG_ALLOCATOR_SRC_ARG_D MG_DEBUG_ALLOCATOR_SRC(CharPtr srcStr = "")
#define MG_DEBUG_ALLOCATOR_SRC_PARAM MG_DEBUG_ALLOCATOR_SRC(srcStr)

#else //defined(MG_DEBUG_ALLOCATOR)

#define MG_DEBUG_ALLOCATOR_SRC(X)
#define MG_DEBUG_ALLOCATOR_SRC_IS
#define MG_DEBUG_ALLOCATOR_SRC_SA
#define MG_DEBUG_ALLOCATOR_SRC_EMPTY
#define MG_DEBUG_ALLOCATOR_SRC_ARG
#define MG_DEBUG_ALLOCATOR_SRC_ARG_D
#define MG_DEBUG_ALLOCATOR_SRC_PARAM

#endif //defined(MG_DEBUG_ALLOCATOR)
RTC_CALL void* AllocateFromStock(size_t sz MG_DEBUG_ALLOCATOR_SRC_ARG);
RTC_CALL void  LeaveToStock(void* ptr, size_t sz);
extern std::atomic<bool> s_ReportingRequestPending;
extern std::atomic<bool> s_BlockNewAllocations;
void ReportFixedAllocStatus();
void ReportFixedAllocFinalSummary();


#endif // __RTC_MEM_FIXED_ALLOC_H
