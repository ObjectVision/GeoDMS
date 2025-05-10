// Copyright (C) 2025 Object Vision b.v. 
// License: GNU GPL 3
///////////////////////////////////////////////////////////////////////////// 

#if defined(_MSC_VER)
#pragma once
#endif


/*
 *  Name        : act/UpdateMark.h
 *  SubSystem   : RTL
 *  Description : Update marker is a class that wraps time stamp functionality
 */

#if !defined(__ACT_UPDATEMARK_H)
#define __ACT_UPDATEMARK_H

#include "cpc/Types.h"
#include "RtcBase.h"
#include "Parallel.h"
#include "act/ActorEnums.h"
#include "utl/IncrementalLock.h"
#include "utl/noncopyable.h"
#include "utl/swapper.h"

struct Actor;

#if defined(MG_ITEMLEVEL)
RTC_CALL item_level_type GetItemLevel(const Actor*);
#else
inline item_level_type GetItemLevel(const Actor*) { return item_level_type(0); }
#endif

#if defined(MG_DEBUG)
#define MG_DEBUG_TS_SOURCE
#endif

#if defined(MG_DEBUG_TS_SOURCE)
#define MG_DEBUG_TS_SOURCE_CODE(x) x
#else
#define MG_DEBUG_TS_SOURCE_CODE(x)
#endif

//  -----------------------------------------------------------------------
//  struct UpdateMark interface
//  -----------------------------------------------------------------------

namespace UpdateMarker
{
	namespace impl {

		RTC_CALL extern std::atomic<TimeStamp> tsLast;
		RTC_CALL extern std::atomic<bool>      bCommitted;

	}

	const TimeStamp tsBereshit = 1;

	RTC_CALL bool IsInActiveState();
	RTC_CALL bool IsInDetermineState();

	RTC_CALL void TriggerFreshTS(MG_DEBUG_TS_SOURCE_CODE(CharPtr cause));
	RTC_CALL TimeStamp GetFreshTS(MG_DEBUG_TS_SOURCE_CODE(CharPtr cause));

	inline TimeStamp LastTS()    { return impl::tsLast; }
	inline TimeStamp GetLastTS() { impl::bCommitted = true; return LastTS(); }
	RTC_CALL TimeStamp GetActiveTS(MG_DEBUG_TS_SOURCE_CODE(CharPtr cause));
	inline bool IsLoadingConfig() { return IsInActiveState() && GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("IsLoadingConfig")) == tsBereshit; }
	RTC_CALL bool CheckTS(TimeStamp changeTS);

	inline bool HasActiveChangeSource()
	{
		return IsInActiveState() || IsInDetermineState();
	}

#if defined(MG_DEBUG_TS_SOURCE)
	void  ReportActiveContext(CharPtr callerFunc);
#endif

//  -----------------------------------------------------------------------
//  struct ChangeSourceLock interface
//  -----------------------------------------------------------------------

	struct ChangeSourceLock : private geodms::rtc::noncopyable
	{
		TimeStamp m_OldTimeStamp;

		RTC_CALL ChangeSourceLock(TimeStamp ts, CharPtr contextDescr);
		RTC_CALL ChangeSourceLock(const Actor* actor, CharPtr contextDescr);
		RTC_CALL ~ChangeSourceLock();

#if defined(MG_DEBUG_TS_SOURCE)
		RTC_CALL static void CheckActivation(TimeStamp ts, CharPtr contextDescr);
		RTC_CALL static void CheckDetermineState();

	private:
		CharPtr      m_OldContextDescr;
		const Actor* m_OldActor;
#endif

	};

//  -----------------------------------------------------------------------
//  struct DetermineChangeLock interface
//  -----------------------------------------------------------------------

	struct DetermineChangeLock : private geodms::rtc::noncopyable
	{
		RTC_CALL DetermineChangeLock();
		RTC_CALL ~DetermineChangeLock();

	private:
		TimeStamp m_PrevActiveChangeSource;
	};

	typedef TimeStamp* TimeStampPtr;
	RTC_CALL void Renumber(TimeStampPtr* first, TimeStampPtr* last);

//  -----------------------------------------------------------------------
//  struct DetermineChangeLock interface
//  -----------------------------------------------------------------------

	struct PrepareDataInvalidatorLock
	{
		RTC_CALL PrepareDataInvalidatorLock();
		RTC_CALL ~PrepareDataInvalidatorLock();
		RTC_CALL static bool IsLocked();
	};

}	// end namespace UpdateMarker

#endif // __ACT_UPDATEMARK_H
