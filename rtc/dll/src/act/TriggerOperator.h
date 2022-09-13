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

#ifndef __RTC_ACT_TRIGGEROPERATOR_H
#define __RTC_ACT_TRIGGEROPERATOR_H

#include "RtcBase.h"

#include "act/UpdateMark.h"
#include "parallel.h"
#include "ptr/OwningPtr.h"

//----------------------------------------------------------------------
// RemainingTargetCount
//----------------------------------------------------------------------

extern "C" RTC_CALL void DMS_CONV DMS_NotifyCurrentTargetCount();

//----------------------------------------------------------------------
// DemandManagement:: IncInterestDetector and LiftInterest; detect late expression of demand and promises
//----------------------------------------------------------------------

RTC_CALL void IncRemainingTargetCount();
RTC_CALL void DecRemainingTargetCount();

#if defined(MG_DEBUG_INTERESTSOURCE)

#include <set>

namespace DemandManagement {
	using ActorSet = std::set<const Actor*>;

	void AddTempTarget    (const Actor* a);
	void ReleaseTempTarget(const Actor* a);

	struct IncInterestDetector // detect late expression of demand 
	{
		RTC_CALL IncInterestDetector(CharPtr contextDescr);
		RTC_CALL ~IncInterestDetector();
		RTC_CALL static void CheckInc(); 

		CharPtr m_PrevDetectorContextDescr;
	};

	struct BlockIncInterestDetector // promise to complete every new promise within the context of this frame (finalize all new interest), thus allow new promises
	{
		RTC_CALL BlockIncInterestDetector();
		RTC_CALL ~BlockIncInterestDetector();

	private:
		UInt32 m_OrgIncInterestDetectorCount;
	};

	extern RTC_CALL leveled_critical_section sd_UpdatingInterestSet;
	extern RTC_CALL ActorSet sd_InterestSet;

	struct IncInterestFence; // monitors the creation and destruction of interest and guarantees that all new Interest will be gone by the end of this context
	struct IncInterestGate;  // suspends monitoring the creation and destruction of interest

}
#endif // defined(MG_DEBUG_INTERESTSOURCE)

//----------------------------------------------------------------------
// SuspendTrigger
//----------------------------------------------------------------------

namespace SuspendTrigger 
{
	RTC_CALL bool MustSuspend() noexcept;
	RTC_CALL void MarkProgress() noexcept;
	RTC_CALL void DoSuspend() noexcept;     // set result for next MustSuspend and DidSuspend
	RTC_CALL bool DidSuspend() noexcept;    // Returns result from last MustSuspend call without calling the trigger-func again (unless a Blocker is active
	RTC_CALL void Resume() noexcept;        // resets the last result of MustSuspend
	RTC_CALL bool IncReportedErrorCount() noexcept;


	RTC_CALL void IncSuspendLevel() noexcept;
	RTC_CALL void DecSuspendLevel() noexcept;

	struct TryFrame { //: InterestRetainContextBase {
		RTC_CALL TryFrame() noexcept;
		RTC_CALL ~TryFrame() noexcept;
		static RTC_CALL bool IsActive() noexcept;
		static RTC_CALL void ConsiderSuspendException();
		struct task_suspended_exception {};
	};
	struct BlockerBase
	{
		RTC_CALL BlockerBase ();
		RTC_CALL ~BlockerBase();

		RTC_CALL static bool IsBlocked();
	};

	struct SilentBlocker : BlockerBase
	{
		RTC_CALL SilentBlocker ();
		RTC_CALL ~SilentBlocker();
	};

	

#if defined(MG_DEBUG_INTERESTSOURCE)

struct SilentBlockerGate : SilentBlocker
{
	RTC_CALL SilentBlockerGate();
	RTC_CALL ~SilentBlockerGate();


	OwningPtr<DemandManagement::IncInterestGate> m_InterestGate;
};

struct FencedBlocker : SilentBlocker
	{
		RTC_CALL FencedBlocker ();
		RTC_CALL ~FencedBlocker ();

		OwningPtr<DemandManagement::IncInterestFence> m_InterestFence;
	};

#else

	typedef SilentBlocker FencedBlocker;
	typedef SilentBlocker SilentBlockerGate;

#endif


#if defined(MG_DEBUG_DATA)

#	include "utl/IncrementalLock.h"

	struct ApplyLock : DynamicIncrementalLock<UInt32>
	{
		RTC_CALL ApplyLock();
	};

#endif

}; // namespace SuspendTrigger

#endif // __RTC_ACT_TRIGGEROPERATOR_H
