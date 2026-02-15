// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_DBG_CHECKPTR_H)
#define __RTC_DBG_CHECKPTR_H

#include "RtcBase.h"
//----------------------------------------------------------------------
// CheckPtr
//----------------------------------------------------------------------

// MG_DEBUG installs code to check the validity of Object* and their dynamic-types. 
// Complexity: O(log(#Objects)) if MG_CHECKPTR is defined in Persistent.cpp 
// ifdef  MG_DEBUG, but not MG_CHECKPTR, only non-null check is performed. 
// ifndef MG_DEBUG, even this check is omitted.

#if defined(MG_DEBUG)
	RTC_CALL void CheckPtr(const Object* item, const Class* cls, CharPtr dmsFunc);
#else
	inline void CheckPtr(const Object*, const Class*, CharPtr) {}
#endif


#endif // __RTC_DBG_CHECKPTR_H
