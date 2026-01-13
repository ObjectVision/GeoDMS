// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

//  -----------------------------------------------------------------------
//  Name        : SharedBase.h
//  Description : SharedBase is a possible base class for objects that are
//                referred to by SharedPtr.
//                It offers RefCount() and AddRef(), 
//                but not Release(), which should be implemented
//                by descending class since SharedBase has no
//                virtual calls and therefore no virtual dtor to 
//                allow a descending class to be non-polymorphic
//	Note:         When your class is polymorphic (has a virtual dtor),
//                derive from PersistentSharedObj
//  -----------------------------------------------------------------------

#if !defined(__RTC_PTR_SHAREDOBJBASE_H)
#define __RTC_PTR_SHAREDOBJBASE_H

#include "RtcBase.h"
#include "dbg/Check.h"

#include <atomic>

struct SharedBase
{
	using ref_count_t = UInt32;

	RTC_CALL ref_count_t GetRefCount() const noexcept;
	RTC_CALL bool IsOwned() const noexcept;

	RTC_CALL void AdoptRef() const noexcept;
	RTC_CALL void IncRef() const noexcept;
	RTC_CALL bool DuplRef() const noexcept;
	RTC_CALL bool DecRef() const noexcept;
	RTC_CALL void Abandon() const noexcept;

// See Notes above for reasons for non-inclusion of Release method.
// The following commented method prototype is for derivations that can access the concrete-type destructor

//	void Release() const { if (!DecRef()) delete this;	}

protected:
	SharedBase() noexcept = default;
	SharedBase(const SharedBase&) : m_RefCount(0) {}
	SharedBase(SharedBase&&) = delete;

	RTC_CALL ~SharedBase();

	SharedBase& operator =(const SharedBase&) = delete; // DONT COPY m_RefCount
	SharedBase& operator =(SharedBase&&) = delete;

private:
	mutable std::atomic<ref_count_t> m_RefCount = 0;
};


#endif // __RTC_PTR_SHAREDOBJBASE_H
