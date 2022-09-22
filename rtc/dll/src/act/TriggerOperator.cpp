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

#include "act/Actor.h"
#include "act/InterestRetainContext.h"
#include "act/TriggerOperator.h"

#include "act/UpdateMark.h"
#include "dbg/DmsCatch.h"
#include "set/QuickContainers.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"
#include "utl/MySPrintF.h"
#include "LockLevels.h"

#include "RtcInterface.h"

//----------------------------------------------------------------------
// RemainingTargetCount
//----------------------------------------------------------------------

namespace {
	leveled_critical_section sc_NotifyTargetCount(item_level_type(0), ord_level_type::NotifyTargetCount, "NotifyTargetCount");
	UInt32 g_RemainingTargetCount = 0;
	UInt32 g_MaxTargetCount = 0;


	clock_t s_LastMsgTime = 0;
	std::atomic<CharPtr> g_LastMsg = nullptr;
	const SizeT BUFFER_SIZE = 75;
	char s_buff[BUFFER_SIZE];

#if defined(MG_DEBUG)
		std::atomic<UInt32> gd_NotifyReentrantGuard;
	#endif

	THREAD_LOCAL UInt32 g_NrTargetsAtEnter = -1;
}	// anonymous namespace


#include <time.h>


void ProgressNotifyMsg(CharPtr msg)
{
	dms_assert(sc_NotifyTargetCount.isLocked());
	if (IsMainThread())
		ProgressMsg(msg);
	else
		g_LastMsg = msg;
}

static const UInt32 s_specialBreak[10] = {
	10000, 5000, 2000, 1000, 1000,
	 1000, 1000, 1000,  500, 200
};

void NotifyRemainingTargetCount(UInt32 nrCount, UInt32 maxCount)
{
	if (InterestRetainContextBase::IsActive())
		return;

#if defined(MG_DEBUG)
	dms_assert(!gd_NotifyReentrantGuard);
	StaticMtIncrementalLock<gd_NotifyReentrantGuard> reentrantGuard;
#endif

	StaticMtIncrementalLock<g_DispatchLockCount> dispatchLock;

	clock_t currTime = clock();
	if (nrCount > 1)
	{
		// BEGIN notification filtering policy
		if (nrCount > 16 && s_LastMsgTime)
		{

			clock_t silenceDuration = (currTime - s_LastMsgTime) / CLOCKS_PER_SEC;
			if (!silenceDuration)
				return;
			if (silenceDuration < 10) // less than 10 secs
			{
				clock_t silenceDurationTicks = (3*(currTime - s_LastMsgTime)) / CLOCKS_PER_SEC;
				dms_assert(silenceDurationTicks < 30);
				UInt32 threshold = (1u << silenceDurationTicks);
				if (nrCount > threshold)
				{
					if (nrCount % s_specialBreak[silenceDuration])
						return;
				}
			}
		}
		// END  notification filtering policy

		ProgressNotifyMsg(myFixedBufferAsCString(s_buff, BUFFER_SIZE, "%u/%u active DataItems", nrCount, maxCount));
	}
	else if (nrCount)
		ProgressNotifyMsg("1 active DataItem");
	else
		ProgressNotifyMsg("No active DataItems");
	s_LastMsgTime = currTime;
}

extern "C" RTC_CALL void DMS_CONV DMS_NotifyCurrentTargetCount()
{
	dms_assert(IsMainThread());
	ProcessMainThreadOpers();
	if (!g_LastMsg)
		return;
	if (g_DispatchLockCount)
		return;

	leveled_critical_section::scoped_lock notifyLock(sc_NotifyTargetCount);
	if (g_LastMsg)
	{
		ProgressMsg(g_LastMsg);
		g_LastMsg = nullptr;
	}
}

RTC_CALL void NotifyCurrentTargetCount()
{
	leveled_critical_section::scoped_lock notifyLock(sc_NotifyTargetCount);
	s_LastMsgTime = 0;
}


void IncRemainingTargetCount()
{
#if defined(MG_DEBUG_INTERESTSOURCE)
	DemandManagement::IncInterestDetector::CheckInc();
#endif // MG_DEBUG_INTERESTSOURCE

	leveled_critical_section::scoped_lock notifyLock(sc_NotifyTargetCount);
	MakeMax(g_MaxTargetCount, ++g_RemainingTargetCount);
	NotifyRemainingTargetCount(g_RemainingTargetCount, g_MaxTargetCount);
}


#if defined(MG_DEBUGREPORTER)
static bool sd_ReportedDone = false;
#endif //defined(MG_DEBUGREPORTER)

void DecRemainingTargetCount()
{
	dms_assert(g_RemainingTargetCount);

	leveled_critical_section::scoped_lock notifyLock(sc_NotifyTargetCount);
	NotifyRemainingTargetCount(--g_RemainingTargetCount, g_MaxTargetCount);

#if defined(MG_DEBUGREPORTER)

	// DEBUG REPORT ON decreasing to 300 000 targets
	if (false) // g_RemainingTargetCount == 340000 || g_RemainingTargetCount == 325000 || g_RemainingTargetCount == 290000)
	{
		sd_ReportedDone = true;
		AddMainThreadOper(
			DBG_DebugReport
		);
	}

#endif defined(MG_DEBUGREPORTER)
}

//----------------------------------------------------------------------
// EnterNotifyBlock
//----------------------------------------------------------------------

	void EnterNotificationBlock()
	{
		dbg_assert(g_NrTargetsAtEnter == -1);
		g_NrTargetsAtEnter = g_RemainingTargetCount;
	}

	void LeaveNotificationBlock()
	{
		dms_assert(g_NrTargetsAtEnter != -1);
		if (g_NrTargetsAtEnter != g_RemainingTargetCount)
		{
			leveled_critical_section::scoped_lock notifyLock(sc_NotifyTargetCount);
			NotifyRemainingTargetCount(g_RemainingTargetCount, g_MaxTargetCount);
		}
		MG_DEBUGCODE( g_NrTargetsAtEnter = -1; )
	}


#if defined(MG_DEBUG_INTERESTSOURCE)

namespace DemandManagement {
	RTC_CALL ActorSet sd_InterestSet;
	leveled_critical_section sd_UpdatingInterestSet(item_level_type(0), ord_level_type::UpdatingInterestSet, "UpdatingInterestSet");

//  -----------------------------------------------------------------------
//  IncInterestFence
//  -----------------------------------------------------------------------

	IncInterestFence* s_CurrFence   = nullptr;
	UInt32            s_TargetCount = 0;

	struct IncInterestFence // monitors the creation and destruction of interest and guarantees that all new Interest will be gone by the end of this context
	{
		IncInterestFence();
		~IncInterestFence();

		std::map<const Actor*, UInt32> m_TempTargets;
		IncInterestFence*  m_PrevFence;
	};

	IncInterestFence::IncInterestFence()
		:	m_PrevFence(s_CurrFence)
	{
		dms_assert(IsMainThread());
		dms_assert(!IsMultiThreaded2());

		s_CurrFence = this;
	}

	IncInterestFence::~IncInterestFence()
	{
		dms_assert(IsMainThread());
		dms_assert(!IsMultiThreaded2());

		dms_assert(s_CurrFence == this);
		for (auto i = m_TempTargets.begin(), e=m_TempTargets.end(); i!=e; ++i)
		{
			reportF(SeverityTypeID::ST_MajorTrace, "IncInterest %u broke through Fence %s", i->second, i->first->GetSourceName().c_str());
		}
		s_CurrFence = m_PrevFence;
	}

	struct IncInterestGate // disables the monitoring of the creation and destruction of interest in the current stack frame
	{
		IncInterestGate();
		~IncInterestGate();

		IncInterestFence*  m_PrevFence;
	};

	IncInterestGate::IncInterestGate()
		: m_PrevFence(s_CurrFence)
	{
		dms_assert(IsMainThread());
		dms_assert(!IsMultiThreaded2());

		s_CurrFence = nullptr;
	}

	IncInterestGate::~IncInterestGate()
	{
		dms_assert(IsMainThread());
		dms_assert(!IsMultiThreaded2());

		dms_assert(s_CurrFence == nullptr);
		s_CurrFence = m_PrevFence;
	}

	void AddTempTarget(const Actor* a)
	{
		dms_assert(a);

		dms_assert(IsMainThread());
		dms_assert(!s_CurrFence || IsMainThread() );

		if (IsMainThread() && s_CurrFence)
			s_CurrFence->m_TempTargets[a] = s_TargetCount++;

		leveled_critical_section::scoped_lock lock(sd_UpdatingInterestSet);
		sd_InterestSet.insert(a);
	}

	void ReleaseTempTarget(const Actor* a)
	{
		dms_assert(a);

		dms_assert(!s_CurrFence || IsMainThread() );

		if (IsMainThread() && s_CurrFence)
			s_CurrFence->m_TempTargets.erase(a);

		leveled_critical_section::scoped_lock lock(sd_UpdatingInterestSet);
		sd_InterestSet.erase(a);
	}

//  -----------------------------------------------------------------------
//  struct IncInterestDetector
//  -----------------------------------------------------------------------

	THREAD_LOCAL UInt32  g_IncInterestDetectorCount = 0;
	THREAD_LOCAL CharPtr g_CurrIncInterestDetectorContextDescr = nullptr;

	IncInterestDetector::IncInterestDetector(CharPtr contextDescr)
		:	m_PrevDetectorContextDescr(g_CurrIncInterestDetectorContextDescr)
	{
		++g_IncInterestDetectorCount;
		g_CurrIncInterestDetectorContextDescr = contextDescr;

	}

	IncInterestDetector::~IncInterestDetector()
	{
		g_CurrIncInterestDetectorContextDescr = m_PrevDetectorContextDescr;
		dms_assert(g_IncInterestDetectorCount);
		--g_IncInterestDetectorCount;
	}

	void IncInterestDetector::CheckInc()
	{
		if (!g_IncInterestDetectorCount)
			return;
 		reportD(SeverityTypeID::ST_MajorTrace, "IncInterestDetector Failure: Starting Interest while in ", g_CurrIncInterestDetectorContextDescr);
//		dms_assert(0);
	}

	BlockIncInterestDetector::BlockIncInterestDetector()
		:	m_OrgIncInterestDetectorCount(g_IncInterestDetectorCount)
	{
		if (!g_IncInterestDetectorCount)
			return;

		g_IncInterestDetectorCount = 0;
	}
	BlockIncInterestDetector::~BlockIncInterestDetector()
	{
		dms_assert(g_IncInterestDetectorCount == 0);
		g_IncInterestDetectorCount = m_OrgIncInterestDetectorCount;
	}

} // end of namespace DemandManagement

#endif //defined(MG_DEBUG_INTERESTSOURCE)

//----------------------------------------------------------------------
// SuspendTrigger
//----------------------------------------------------------------------

namespace SuspendTrigger {

	bool s_bLastResult = false;
	bool s_MustSuspend = false;
	bool s_ProgressMade = false;
	std::atomic<SizeT> s_ReportedErrorCount = 0;
	const SizeT s_MaxReportedErrorCount = 4;

	THREAD_LOCAL UInt32 s_SuspendLevel = 0;

	void IncSuspendLevel() noexcept { ++s_SuspendLevel; }
	void DecSuspendLevel() noexcept { --s_SuspendLevel; }

	UInt32 s_TryFrameCount = 0;
	TryFrame::TryFrame() noexcept
	{
		if (IsMainThread())
			++s_TryFrameCount;
	}
	TryFrame:: ~TryFrame() noexcept
	{
		if (IsMainThread())
			--s_TryFrameCount;
	}

	bool TryFrame::IsActive() noexcept { return IsMainThread() && s_TryFrameCount;  }

	void TryFrame::ConsiderSuspendException()
	{
		if (IsActive() && MustSuspend())
			throw task_suspended_exception();
	}


	#if defined(MG_DEBUG_DATA)
		THREAD_LOCAL UInt32 gd_TriggerApplyLockCount = 0;
	#endif

	void MarkProgress() noexcept
	{
		if (!BlockerBase::IsBlocked())
			s_ProgressMade = true;
	}

	bool MustSuspend() noexcept
	{
		if (BlockerBase::IsBlocked())
			return false;

		dms_assert(IsMainThread());

		if (s_bLastResult)
			return true;

		if (!s_ProgressMade)
			return false;

		if (s_MustSuspend)
		{
			s_MustSuspend = false;
			s_bLastResult = true;
			return true;
		}

		MGD_CHECKDATA(gd_TriggerApplyLockCount == 0); // find who pulls the trigger


		if (IsMainThread())
			ProcessMainThreadOpers();

		if (s_SuspendLevel || HasWaitingMessages()) // HasWaitingMessages() can send WM_SIZE ... that sets s_SuspendLevel
		{
			s_bLastResult = true;
			return true;
		}
		dms_assert(!s_bLastResult);
		return false;
	}

	void Resume() noexcept
	{
		dms_assert(IsMainThread());
//		dms_assert(!s_SuspendLevel); // receipe for trouble later on
		s_bLastResult  = false;
		s_ProgressMade = false;
		s_MustSuspend = false;
		SizeT oldErrorCount = s_ReportedErrorCount;
		s_ReportedErrorCount = 0;
		if (oldErrorCount > s_MaxReportedErrorCount)
			reportF(SeverityTypeID::ST_MajorTrace, "Skipped %I64u warnings.....", UInt64(oldErrorCount - s_MaxReportedErrorCount));
	}

	void DoSuspend() noexcept
	{
		s_MustSuspend = true;
	}

	bool DidSuspend() noexcept
	{
		if (BlockerBase::IsBlocked()) 
			return false;
		dms_assert(IsMainThread());
		return s_bLastResult;
	}

	bool IncReportedErrorCount() noexcept
	{
		return ++s_ReportedErrorCount > s_MaxReportedErrorCount;
	}


//  -----------------------------------------------------------------------
//  SuspendTrigger::Blocker
//  -----------------------------------------------------------------------

	static UInt32 s_SuspendBlockLevel = 0;    // publicly available for setting IncrementalLocks

	BlockerBase::BlockerBase() 
	{ 
		if (IsMainThread())
		{
			++s_SuspendBlockLevel; 
			dms_assert(s_SuspendBlockLevel); // no overflow
		}
	}

	BlockerBase::~BlockerBase()
	{
		if (IsMainThread())
		{
			dms_assert(s_SuspendBlockLevel); // resource match
			--s_SuspendBlockLevel;
		}
	}

	bool BlockerBase::IsBlocked()
	{
		return IsMainThread() ? s_SuspendBlockLevel : true;
	}

	SilentBlocker::SilentBlocker() 
	{ 
		if (IsMainThread() && s_SuspendBlockLevel == 1)
			EnterNotificationBlock();
	}

	SilentBlocker::~SilentBlocker()
	{
		if (IsMainThread() && s_SuspendBlockLevel == 1)
			LeaveNotificationBlock();
	}

	#if defined(MG_DEBUG_INTERESTSOURCE)

	UInt32 GetRemainingTargetCount()
	{
		return g_RemainingTargetCount;
	}

	SilentBlockerGate::SilentBlockerGate()
	{
		if (!IsMultiThreaded2() && IsMainThread())
			m_InterestGate.assign(new DemandManagement::IncInterestGate);
	}

	SilentBlockerGate::~SilentBlockerGate()
	{};

	FencedBlocker::FencedBlocker()
	{ 
		if (!InterestRetainContextBase::IsActive() && !IsMultiThreaded2() && IsMainThread())
			m_InterestFence.assign(new DemandManagement::IncInterestFence );
	}

	FencedBlocker::~FencedBlocker()
	{};

	#endif
//  -----------------------------------------------------------------------
//  TriggerApplyLock
//  -----------------------------------------------------------------------

#if defined(MG_DEBUG_DATA)

	ApplyLock::ApplyLock()
	:	DynamicIncrementalLock<>(gd_TriggerApplyLockCount)
	{
		dms_assert(!DidSuspend());
	}

#endif
} // namespace SuspendTrigger


