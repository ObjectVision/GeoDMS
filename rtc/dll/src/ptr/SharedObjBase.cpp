// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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

void TileBase::DecoupleShadowFromOwner()
{
	throwIllegalAbstract(MG_POS, "TileBase::DecoupleShadowFromOwner()");
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
