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

#include "RtcPCH.h"
#pragma hdrstop

//============================= Parallel

#include "Parallel.h"
#include "act/MainThread.h"
#include "utl/Environment.h"

#include "parallel.h"
#include "LockLevels.h"

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


