// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __UTL_INCREMENTALLOCK_H
#define __UTL_INCREMENTALLOCK_H

#include <atomic>
#include "cpc/Types.h"

#include "act/MainThread.h"
#include "dbg/Diagnostics.h"

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
	StaticStIncrementalLock() { dms_assert(IsMetaThread());  ++lockCounter; }
	~StaticStIncrementalLock() { dms_assert(IsMetaThread());  --lockCounter; }

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
