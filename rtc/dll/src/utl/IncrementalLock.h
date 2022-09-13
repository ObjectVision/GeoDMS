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
#pragma once

#ifndef __UTL_INCREMENTALLOCK_H
#define __UTL_INCREMENTALLOCK_H

#include <atomic>
#include "cpc/Types.h"

#include "act/MainThread.h"
#include "dbg/Check.h"

//#include <gsl/gsl>

template <typename Ord = UInt32>
struct DynamicIncrementalLock
{
	DynamicIncrementalLock(Ord& c) : m_LockCounterPtr(&c) { ++*m_LockCounterPtr;  }
	DynamicIncrementalLock (const DynamicIncrementalLock& rhs) : m_LockCounterPtr(rhs.m_LockCounterPtr) { ++*m_LockCounterPtr; }

	~DynamicIncrementalLock ()                         
	{ 
		--*m_LockCounterPtr;
	}
	void operator =(const DynamicIncrementalLock&) = delete;
	void operator =(DynamicIncrementalLock&&) = delete;

private: // Data Members for Class Attributes
	Ord* m_LockCounterPtr;
};

template <std::atomic<UInt32>& lockCounter>
struct StaticMtIncrementalLock
{
	StaticMtIncrementalLock() { ++lockCounter; }
	~StaticMtIncrementalLock() { --lockCounter; }

	StaticMtIncrementalLock(const StaticMtIncrementalLock& rhs) = delete;
	StaticMtIncrementalLock(StaticMtIncrementalLock&& rhs) = delete;
};

template <UInt32& lockCounter>
struct StaticStIncrementalLock
{
	StaticStIncrementalLock() { dms_assert(IsMainThread());  ++lockCounter; }
	~StaticStIncrementalLock() { dms_assert(IsMainThread());  --lockCounter; }

	StaticStIncrementalLock(const StaticStIncrementalLock& rhs) = delete;
	StaticStIncrementalLock(StaticStIncrementalLock&& rhs) = delete;
};

template <typename CounterType, CounterType& lockCounter>
struct StaticMtDecrementalLock
{
	StaticMtDecrementalLock() { --lockCounter; }
	~StaticMtDecrementalLock() { ++lockCounter; }

	StaticMtDecrementalLock(const StaticMtDecrementalLock& rhs) = delete;
	StaticMtDecrementalLock(StaticMtDecrementalLock&& rhs) = delete;
};

template <typename Ord = UInt32>
struct DynamicConditionalIncrementalLock
{
	DynamicConditionalIncrementalLock(Ord& c, bool isActive)
		:	m_LockCounterPtr(isActive ? &++c : nullptr)
	{
	}
	~DynamicConditionalIncrementalLock() 
	{ 
		if (m_LockCounterPtr)
			--*m_LockCounterPtr; 
	}

private:
	DynamicConditionalIncrementalLock(const DynamicConditionalIncrementalLock& oth)
		:	m_LockCounterPtr(oth.m_LockCounterPtr)
	{
		if (m_LockCounterPtr)
			++*m_LockCounterPtr; 
	}
private:
	Ord* m_LockCounterPtr;
};


#endif // __UTL_INCREMENTALLOCK_H
