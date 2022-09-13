//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

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

#if defined(MG_DEBUG)
#define MG_DEBUG_ALLOCATOR
#endif

#if defined(MG_DEBUG_ALLOCATOR)

#define MG_DEBUG_ALLOCATOR_SRC(X) ,X
#define MG_DEBUG_ALLOCATOR_SRC_STR(X) , SharedStr(X)
#define MG_DEBUG_ALLOCATOR_SRC_IS    MG_DEBUG_ALLOCATOR_SRC(IndexedString())
#define MG_DEBUG_ALLOCATOR_SRC_SA    MG_DEBUG_ALLOCATOR_SRC(SequenceArrayString())
#define MG_DEBUG_ALLOCATOR_SRC_EMPTY MG_DEBUG_ALLOCATOR_SRC(SharedStr())
#define MG_DEBUG_ALLOCATOR_SRC_ARG   MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)
#define MG_DEBUG_ALLOCATOR_SRC_ARG_D MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr = SharedStr())
#define MG_DEBUG_ALLOCATOR_SRC_PARAM MG_DEBUG_ALLOCATOR_SRC(srcStr)

#else //defined(MG_DEBUG_ALLOCATOR)

#define MG_DEBUG_ALLOCATOR_SRC(X)
#define MG_DEBUG_ALLOCATOR_SRC_STR(X)
#define MG_DEBUG_ALLOCATOR_SRC_IS
#define MG_DEBUG_ALLOCATOR_SRC_SA
#define MG_DEBUG_ALLOCATOR_SRC_EMPTY
#define MG_DEBUG_ALLOCATOR_SRC_ARG
#define MG_DEBUG_ALLOCATOR_SRC_ARG_D
#define MG_DEBUG_ALLOCATOR_SRC_PARAM

#endif //defined(MG_DEBUG_ALLOCATOR)
RTC_CALL void* AllocateFromStock(SizeT sz MG_DEBUG_ALLOCATOR_SRC_ARG);
RTC_CALL void  LeaveToStock(void* ptr, SizeT sz);
extern std::atomic<bool> s_ReportingRequestPending;
void ReportFixedAllocStatus();


#endif // __RTC_MEM_FIXED_ALLOC_H
