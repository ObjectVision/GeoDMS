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

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "act/Actor.h"
#include "act/MainThread.h"
#include "act/UpdateMark.h"
#include "dbg/Check.h"
#include "dbg/SeverityType.h"
#include "ptr/SharedStr.h"
#include "utl/mySPrintF.h"
#include "Parallel.h"

#include <algorithm>

#if defined(MG_DEBUG_TS_SOURCE)
#include <map>
#endif
//  -----------------------------------------------------------------------
//	 move to environemnt.h
//  -----------------------------------------------------------------------

namespace UpdateMarker {

	namespace impl {

		RTC_CALL std::atomic<TimeStamp> tsLast     = tsBereshit;    // zero TS are not permitted
		RTC_CALL std::atomic<bool>      bCommitted = true; // each Actor is initialized with m_LastChangeTS := 1;
		THREAD_LOCAL TimeStamp tsActive   = 0;    // indicates no active context: use GetFreshTS for changes
		THREAD_LOCAL UInt32    iDetermineChangeLockCount = 0;
	}

	RTC_CALL TimeStamp GetLastChangeTS(const Actor* actor) 
	{ 
		return (actor) ? actor->GetLastChangeTS() : tsBereshit;
	}

	RTC_CALL bool IsInActiveState()
	{
		return impl::tsActive != 0; 
	}

	RTC_CALL bool IsInDetermineState()
	{
		return impl::iDetermineChangeLockCount;
	}

	RTC_CALL TimeStamp GetActiveTS(MG_DEBUG_TS_SOURCE_CODE(CharPtr cause)) 
	{ 
		return (IsInActiveState())
			?	impl::tsActive 
			:	(IsInDetermineState())
				?	GetLastTS()
				:	GetFreshTS(MG_DEBUG_TS_SOURCE_CODE(cause)); 
	}

	RTC_CALL bool CheckTS(TimeStamp changeTS)
	{
		dms_assert( IsInDetermineState() );
		return 	(IsInActiveState()) 
				?	(changeTS <= impl::tsActive)
				:	(changeTS <= impl::tsLast);
	}

//  -----------------------------------------------------------------------
//  struct ChangeSourceLock 
//  -----------------------------------------------------------------------

#if defined(MG_DEBUG_TS_SOURCE)

	THREAD_LOCAL CharPtr       g_CurrChangedContext = "";
	THREAD_LOCAL const Actor*  g_CurrChangedActor   = nullptr;

	std::map<TimeStamp, SharedStr> s_ChangeSources;

	void  ReportActiveContext(CharPtr callerFunc)
	{
		reportF(SeverityTypeID::ST_MajorTrace, "%s called during %s(%s) for TimeFrame %u and DetermineChangeLockCount %u (Last %s TimeFrame = %u)",
			callerFunc,
			g_CurrChangedContext,
			g_CurrChangedActor ? g_CurrChangedActor->GetSourceName().c_str() : "",
			impl::tsActive,
			impl::iDetermineChangeLockCount,
			impl::bCommitted.load() ? "Committed" : "Uncommitted",
			impl::tsLast.load()
		);

		for (auto& changeSource: s_ChangeSources)
			reportF(SeverityTypeID::ST_MinorTrace, "ts %d was caused by %s", changeSource.first, changeSource.second);
//		dms_check(0);
	}

	ChangeSourceLock::ChangeSourceLock(TimeStamp ts, CharPtr contextDescr)
		:	m_OldTimeStamp   (impl::tsActive)
		,	m_OldContextDescr(g_CurrChangedContext)
		,	m_OldActor       (g_CurrChangedActor)
	{
		MakeMax(ts, tsBereshit);
		CheckActivation(ts, contextDescr);
//		dms_assert(ts <= impl::tsActive || impl::tsActive == 0);
		impl::tsActive = ts;
		g_CurrChangedContext = contextDescr;
		g_CurrChangedActor   = nullptr;
	}

	ChangeSourceLock::ChangeSourceLock(const Actor* actor, CharPtr contextDescr)
		: ChangeSourceLock(GetLastChangeTS(actor), contextDescr)
	{
//		dms_assert(IsMainThread());
		g_CurrChangedActor = actor;
	}

	ChangeSourceLock::~ChangeSourceLock() 
	{
		TimeStamp closingFrame = impl::tsActive;
		impl::tsActive = m_OldTimeStamp; 
		CheckActivation(closingFrame, "ChangeSourceLock::~ChangeSourceLock()");
		g_CurrChangedContext = m_OldContextDescr;
		g_CurrChangedActor   = m_OldActor;
	}

	void ChangeSourceLock::CheckActivation(TimeStamp ts, CharPtr contextDescr)
	{
		if (!IsInActiveState())
			return;
		if (ts <= impl::tsActive)
			return;

		ReportActiveContext(mySSPrintF("Activation of TimeFrame %u from %s", ts, contextDescr).c_str());
	}

	void ChangeSourceLock::CheckDetermineState()
	{
		if (!IsInActiveState())
			return;

		ReportActiveContext("CheckDetermineState");
	}


#else

ChangeSourceLock::ChangeSourceLock(TimeStamp ts, CharPtr)
	:	m_OldTimeStamp(impl::tsActive)
{	
	MakeMax(ts, tsBereshit);
	impl::tsActive = ts;
}

ChangeSourceLock::ChangeSourceLock(const Actor* actor, CharPtr x)
	:	ChangeSourceLock(GetLastChangeTS(actor), x)
{}

ChangeSourceLock::~ChangeSourceLock()
{ 
	impl::tsActive = m_OldTimeStamp; 
}

#endif

//  -----------------------------------------------------------------------
//  struct DetermineChangeLock interface
//  -----------------------------------------------------------------------

	DetermineChangeLock::DetermineChangeLock()
		:	m_PrevActiveChangeSource( impl::tsActive )
	{ 
	#if defined(MG_DEBUG_UPDATESOURCE)
		ChangeSourceLock::CheckDetermineState();
	#endif

		impl::tsActive = 0;
		++impl::iDetermineChangeLockCount;

		dms_assert(!IsInActiveState());
	}

	DetermineChangeLock::~DetermineChangeLock() 
	{ 
		dms_assert( impl::tsActive == 0);

		--impl::iDetermineChangeLockCount;
		impl::tsActive = m_PrevActiveChangeSource;
	} 

	namespace impl {

		struct comparePtr {
			bool operator () (TimeStampPtr lhs, TimeStampPtr rhs) const
			{
				return (*lhs < *rhs)
					// provide additional ordering for equivalent pointers in order to detect 
					// illegal shared TimeStamps
					MG_DEBUGCODE( || ((!(*rhs < *lhs)) && (lhs < rhs))) 
				;
			}
		};
	}

	void TriggerFreshTS(MG_DEBUG_TS_SOURCE_CODE(CharPtr cause))
	{
		MG_DEBUG_TS_SOURCE_CODE(
			if (IsInActiveState() || IsInDetermineState())
				ReportActiveContext("TriggerFreshTS");
		)

		if (impl::bCommitted)
		{
			++impl::tsLast;
			assert(impl::tsLast); // we assume no overflow
			impl::bCommitted = false;

			MG_DEBUG_TS_SOURCE_CODE(
				s_ChangeSources[impl::tsLast] = cause;
				reportF(SeverityTypeID::ST_MinorTrace, "ts %d is caused by %s", impl::tsLast.load(), cause);
			)
		}
	}

	TimeStamp GetFreshTS(MG_DEBUG_TS_SOURCE_CODE(CharPtr cause))
	{
		assert(IsMetaThread());

		assert(! IsInActiveState()    );
		assert(! IsInDetermineState() );
		TriggerFreshTS(MG_DEBUG_TS_SOURCE_CODE(cause));
		return impl::tsLast;
	}

	void Renumber(TimeStampPtr* first, TimeStampPtr* last)
	{
		dms_assert(IsMetaThread());

		DBG_START("UpdateMarker", "Renumber", false);

		std::sort(first, last); // sort pointers, just to remove duplicates.
		last = std::unique(first, last); // erasure is not required as caller destroys container anyway.
		std::sort(first, last, impl::comparePtr());

		TimeStamp curr = 0;

		while (first != last && !**first) 
			++first; // skip uninitialized Timestamps

		dms_assert(first == last || **first > 0); 
		for (; first != last; ++first)
		{
			dms_assert(**first > 0); 
			dms_assert(curr <= **first); // stuff was ordered ( and hasn't been tampered with since? )
			if (curr < **first)
			{
				++impl::tsLast;
				curr = **first;
				DBG_TRACE(("%u -> %u", curr, impl::tsLast.load()));
			}
			**first = impl::tsLast;
		}
		impl::bCommitted = true;

		MG_DEBUG_TS_SOURCE_CODE( s_ChangeSources.clear(); )
	}

//  -----------------------------------------------------------------------
//  struct DetermineChangeLock interface
//  -----------------------------------------------------------------------

	THREAD_LOCAL UInt32 s_invalidatorLockCounter = 0;

	PrepareDataInvalidatorLock::PrepareDataInvalidatorLock()
	{
		++s_invalidatorLockCounter;
	}
	PrepareDataInvalidatorLock::~PrepareDataInvalidatorLock()
	{
		--s_invalidatorLockCounter;
	}

	bool PrepareDataInvalidatorLock::IsLocked()
	{
		return s_invalidatorLockCounter;
	}

}	//	namespace UpdateMarker 

extern "C" RTC_CALL TimeStamp DMS_CONV DMS_GetLastChangeTS()
{
	return UpdateMarker::GetLastTS();
}