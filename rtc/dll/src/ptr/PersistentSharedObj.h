// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__PTR_PERSISTENTSHAREDOBJ_H)
#define __PTR_PERSISTENTSHAREDOBJ_H

#include "ptr/SharedObj.h"
#include "ptr/SharedStr.h"
#include "throwItemError.h"

class PersistentSharedObj : public SharedObj
{
public:
	RTC_CALL virtual const PersistentSharedObj* GetParent() const;

	RTC_CALL SharedStr GetSourceName() const override;

	// NON VIRTUAL ROUTINES BASED ON THE ABOVE INTERFACE
	RTC_CALL SharedStr GetFullName() const;
	RTC_CALL SharedStr GetPrefixedFullName() const;

	RTC_CALL const PersistentSharedObj* GetRoot() const;
	RTC_CALL bool DoesContain(const PersistentSharedObj* subItemCandidate) const;

	RTC_CALL const SourceLocation* GetLocation() const override ;

	RTC_CALL SharedStr GetRelativeName(const PersistentSharedObj* context) const;
	RTC_CALL SharedStr GetFindableName(const PersistentSharedObj* subItem) const;

	[[noreturn]] RTC_CALL void throwItemError(WeakStr msgStr) const { ::throwItemError(this, msgStr); }
	[[noreturn]] void throwItemError(CharPtr msg) const { ::throwItemError(this, SharedStr(msg)); }

	template<typename ...Args>
	[[noreturn]] void throwItemErrorF(CharPtr msg, Args&&... args) const {
		::throwItemErrorF(this, msg, std::forward<Args>(args)...);
	}

	DECL_ABSTR(RTC_CALL, Class);
};

#endif // __PTR_PERSISTENTSHAREDOBJ_H`
