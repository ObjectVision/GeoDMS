// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_ACT_INTERESTRETAINCONTEXT_H)
#define __RTC_ACT_INTERESTRETAINCONTEXT_H

#include "act/TriggerOperator.h"

//  -----------------------------------------------------------------------
//  context for holding interest beyond the current stack frame
//  -----------------------------------------------------------------------

struct InterestRetainContextBase
#if defined(MG_DEBUG_INTERESTSOURCE)
	: DemandManagement::BlockIncInterestDetector
#endif
{
	RTC_CALL InterestRetainContextBase();
	RTC_CALL ~InterestRetainContextBase();

	RTC_CALL void Add(const Actor* actor); // only allow for Add when there is a context visible
	RTC_CALL static bool IsActive();
};

struct SilentInterestRetainContext : SuspendTrigger::SilentBlocker, InterestRetainContextBase { using SuspendTrigger::SilentBlocker::SilentBlocker; };
struct FencedInterestRetainContext : SuspendTrigger::FencedBlocker, InterestRetainContextBase { using SuspendTrigger::FencedBlocker::FencedBlocker; };


#endif //!defined(__RTC_ACT_INTERESTRETAINCONTEXT_H)
