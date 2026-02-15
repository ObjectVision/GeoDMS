// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_ACT_ACTORLOCK_H)
#define __RTC_ACT_ACTORLOCK_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "act/Actor.h"
#include "cs_lock_map.h"

//  -----------------------------------------------------------------------

using actor_section_lock_map = cs_lock_map<const Actor*>;

extern RTC_CALL leveled_std_section sg_CountSection;
extern RTC_CALL actor_section_lock_map sg_ActorLockMap;

#endif // __RTC_ACT_ACTORLOCK_H
