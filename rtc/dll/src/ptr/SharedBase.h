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

#if defined(MG_DEBUG)
#define MG_DEBUG_REFCOUNT
#endif

struct SharedBase
{
	using ref_count_t = UInt32;
#if defined(MG_DEBUG_REFCOUNT)
	static const ref_count_t dangling_object_indicator = -1;
#endif

	ref_count_t GetRefCount() const noexcept
	{
#if defined(MG_DEBUG_REFCOUNT)
		MG_ASSERT(m_RefCount != dangling_object_indicator);
#endif
		return m_RefCount;
	}
	bool IsOwned() const noexcept
	{
		if (m_RefCount == 0)
			return false;
#if defined(MG_DEBUG_REFCOUNT)
		if (m_RefCount == dangling_object_indicator)
			return false;
#endif
		return true;
	}

	void IncRef() const noexcept
	{
#if defined(MG_DEBUG_REFCOUNT)
		MG_ASSERT(m_RefCount != dangling_object_indicator);
#endif
		++m_RefCount;
		assert(m_RefCount); // POST CONDITION
	}
	bool DuplRef() const noexcept
	{
		while (true)
		{
			ref_count_t refCount = m_RefCount;
#if defined(MG_DEBUG_REFCOUNT)
			if (refCount == dangling_object_indicator)
				return false;
#endif
			if (!refCount)
				return false;
			if (m_RefCount.compare_exchange_weak(refCount, refCount + 1))
				return true;
		}
	}

	bool DecRef() const noexcept
	{
		assert(m_RefCount); // PRE CONDITION
#if defined(MG_DEBUG_REFCOUNT)
		MG_ASSERT(m_RefCount !=  0);
		MG_ASSERT(m_RefCount != dangling_object_indicator);
#endif
		auto result = --m_RefCount;
#if defined(MG_DEBUG_REFCOUNT)
		if (!result) // last ptr, so no longer MT access possible, only set dangling pointer detector once
		{
			result = m_RefCount.exchange(dangling_object_indicator);
			MG_ASSERT(!result);
		}
#endif
		return result;
	}

// See Notes above for reasons for non-inclusion of Release method.
// The following commented method prototype is for derivations that can access the concrete-type destructor

//	void Release() const { if (!DecRef()) delete this;	}

protected:
	SharedBase() noexcept = default;
	SharedBase(const SharedBase&) : m_RefCount(0) {}
	SharedBase(SharedBase&&) = delete;

   ~SharedBase() { assert(!IsOwned()); }

	SharedBase& operator =(const SharedBase&) = delete; // DONT COPY m_RefCount
	SharedBase& operator =(SharedBase&&) = delete;

private:
	mutable std::atomic<ref_count_t> m_RefCount = 0;
};


#endif // __RTC_PTR_SHAREDOBJBASE_H
