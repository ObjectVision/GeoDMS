// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

//  -----------------------------------------------------------------------
//  Name        : SharedBase.h
//  Description : SharedBase is a possible base class for objects that are
//                referred to by SharedPtr.
//                It offers RefCount() and AddRef(), 
//                but not Release(), which should be implemented
//                by descending class since SharedBase has no
//                virtual calls and therefore no virtual dtor to 
//                allow a descending class to be non-polymorphic
//	Note:         When your class is polymorphic (has a virtual dtor),
//                derive from PersistentSharedObj
//  -----------------------------------------------------------------------

#include "ptr/SharedBase.h"
#include "act/Actor.h"
#include "act/ActorEnums.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_REFCOUNT
#else
//#define MG_DEBUG_REFCOUNT
#endif

#if defined(MG_DEBUG_REFCOUNT)
	static const SharedBase::ref_count_t dangling_object_indicator = -1;
#endif

SharedBase::~SharedBase()
{ 
	assert(!IsOwned()); 

}

auto SharedBase::GetRefCount() const noexcept -> ref_count_t
{
#if defined(MG_DEBUG_REFCOUNT)
	MG_CHECK(m_RefCount != dangling_object_indicator);
#endif
	return m_RefCount;
}

bool SharedBase::IsOwned() const noexcept
{
	if (m_RefCount == 0)
		return false;
#if defined(MG_DEBUG_REFCOUNT)
	if (m_RefCount == dangling_object_indicator)
		return false;
#endif
	return true;
}

void SharedBase::AdoptRef() const noexcept
{
	++m_RefCount;
	assert(m_RefCount >= 1); // POST CONDITION, no other threads should have access yet
}

void SharedBase::IncRef() const noexcept
{
#if defined(MG_DEBUG_REFCOUNT)
	MG_ASSERT(m_RefCount != 0);
	MG_ASSERT(m_RefCount != dangling_object_indicator);
#endif
	++m_RefCount;
	assert(m_RefCount); // POST CONDITION
}

void SharedBase::IncMutableRef() const noexcept
{
	AdoptRef();
}

bool SharedBase::DuplRef() const noexcept
{
	while (true)
	{
		ref_count_t refCount = m_RefCount;
#if defined(MG_DEBUG_REFCOUNT)
		if (refCount == dangling_object_indicator)
			return false;
#endif
		if (!refCount)
			return false;
		if (m_RefCount.compare_exchange_weak(refCount, refCount + 1))
			return true;
	}
}

void SharedBase::Abandon() const noexcept
{
	assert(!m_RefCount); // PRE CONDITION
	
#if defined(MG_DEBUG_REFCOUNT)

	MG_ASSERT(m_RefCount == 0);
	auto result = m_RefCount.exchange(dangling_object_indicator);
	if (result)
	{
		reportF(SeverityTypeID::ST_Error, "Unexepcted RefCount %d at object with ptr %x", result, this);
		MG_CHECK(!result);
	}

#endif

}

bool SharedBase::decrement_count() const noexcept
{
	assert(m_RefCount); // PRE CONDITION
#if defined(MG_DEBUG_REFCOUNT)
	MG_ASSERT(m_RefCount != 0);
	MG_ASSERT(m_RefCount != dangling_object_indicator);
#endif
	auto result = --m_RefCount;
	return result;
}

void SharedBase::DecMutableRef() const noexcept
{
	decrement_count(); // return to newly created object state if not already shared
}

bool SharedBase::DecRef() const noexcept
{
	if (decrement_count()) 
		return true;

	// last shared ptr, so no longer shared access possible, only set dangling pointer detector once
	Abandon();
	return false;
}

//============================= Parallel

#include "Parallel.h"
#include "act/MainThread.h"
#include "utl/Environment.h"

#include "Parallel.h"
#include "LockLevels.h"
#include <condition_variable>

leveled_std_section s_CountedMutexSection({}, ord_level_type::CountedMutexSection, "CountedMutextSection");

std::condition_variable s_cv_CountedMutexSection;

void counted_mutex::lock()
{
	leveled_std_section::unique_lock lock( s_CountedMutexSection );
	dbg_assert(!m_Count || md_OwningThreadID != GetThreadID());

	s_cv_CountedMutexSection.wait(lock.m_BaseLock, [&] { return m_Count == 0; });
	dms_assert(!m_Count);

	MG_DEBUGCODE(md_OwningThreadID = GetThreadID(); )
	--m_Count;
}

void counted_mutex::lock_shared()
{
	leveled_std_section::unique_lock lock(s_CountedMutexSection);
	dbg_assert(m_Count >= 0 || md_OwningThreadID != GetThreadID());

	s_cv_CountedMutexSection.wait(lock.m_BaseLock, [&] { return m_Count >= 0; });
	dms_assert(m_Count >= 0);

	MG_DEBUGCODE(md_OwningThreadID = GetThreadID(); )
	++m_Count;
	dms_assert(m_Count >= 0);
}

bool counted_mutex::try_lock_shared()
{
	leveled_std_section::unique_lock lock(s_CountedMutexSection);
	dbg_assert(m_Count >= 0 || md_OwningThreadID != GetThreadID());

	if (m_Count < 0)
		return false;
	dms_assert(m_Count >= 0);

	MG_DEBUGCODE(md_OwningThreadID = GetThreadID(); )
	++m_Count;
	dms_assert(m_Count > 0);
	return true;
}

void counted_mutex::unlock()
{
	leveled_std_section::unique_lock lock(s_CountedMutexSection);
	dms_assert(m_Count == -1);
	dbg_assert(md_OwningThreadID == GetThreadID());

	++m_Count;
	MG_DEBUGCODE( md_OwningThreadID = 0; )
	dms_assert(m_Count == 0);
	s_cv_CountedMutexSection.notify_all();
}

void counted_mutex::unlock_shared()
{
	leveled_std_section::unique_lock lock(s_CountedMutexSection);
	dms_assert(m_Count > 0);
//	dbg_assert(md_OwningThreadID == GetThreadID());

	--m_Count;
	MG_DEBUGCODE( md_OwningThreadID = 0; )
	dms_assert(m_Count >= 0);
	if (!m_Count)
		s_cv_CountedMutexSection.notify_all();
}


#if defined(MG_DEBUG_LOCKLEVEL)

THREAD_LOCAL level_type s_LockLevel;
THREAD_LOCAL UInt32 s_LevelCheckBlockCount = 0;

LevelCheckBlocker::LevelCheckBlocker()
{
	++s_LevelCheckBlockCount;
}

LevelCheckBlocker::~LevelCheckBlocker()
{
	--s_LevelCheckBlockCount;
}

bool LevelCheckBlocker::HasLevelCheckBlocked()
{
	return s_LevelCheckBlockCount;
}


RTC_CALL level_type EnterLevel(level_type level)
{
	dms_assert(level.m_Level != ord_level_type(0));
	dms_assert(s_LockLevel.Allow(level));
	std::swap(s_LockLevel, level);
	return level;
}

RTC_CALL void LeaveLevel(level_type& oldLevel)
{
	dms_assert(oldLevel.Allow(s_LockLevel));

	dms_assert(s_LockLevel.m_Prev);
	dms_assert(s_LockLevel.m_Prev == &oldLevel || s_LockLevel.Shared(*s_LockLevel.m_Prev));

	auto pointee = &s_LockLevel;
	while (pointee->m_Prev != &oldLevel)
	{
		pointee = pointee->m_Prev;
		dms_assert(pointee);

		dms_assert(oldLevel.Allow(*pointee));
	}
	dms_assert (pointee->m_Prev == &oldLevel);
	*pointee = oldLevel;
}

#endif // MG_DEBUG_LOCKLEVEL
