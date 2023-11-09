// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_THROWITEMERROR_H)
#define __RTC_THROWITEMERROR_H

#include "ptr/SharedStr.h"
class PersistentSharedObj;

	//	====================== Why here, TODO G8 move to separate header
[[noreturn]] RTC_CALL void throwItemError(ErrMsgPtr msgPtr);

[[noreturn]] RTC_CALL void throwItemError(const PersistentSharedObj* self, WeakStr msgStr);
[[noreturn]] RTC_CALL void throwItemError(const PersistentSharedObj* self, CharPtr msg);

template<typename ...Args>
[[noreturn]] void throwItemErrorF(const PersistentSharedObj* self, CharPtr msg, Args&&... args) {
	throwItemError(self, mgFormat2SharedStr(msg, std::forward<Args>(args)...));
}


#endif // __RTC_THROWITEMERROR_H`
