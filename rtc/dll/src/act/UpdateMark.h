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
#include "utl/swapper.h"

#include <boost/noncopyable.hpp>
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

	struct ChangeSourceLock : private boost::noncopyable
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

	struct DetermineChangeLock : private boost::noncopyable
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
