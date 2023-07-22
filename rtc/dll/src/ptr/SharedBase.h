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

#include <atomic>

#define MG_DEBUG_REFCOUNT

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

	SharedBase& operator =(const SharedBase& ) {} // DONT COPY m_RefCount
	SharedBase& operator =(SharedBase&&) = delete;

	ref_count_t GetCount() const noexcept { return m_RefCount; }

private:
	mutable std::atomic<ref_count_t> m_RefCount = 0;
};


#endif // __RTC_PTR_SHAREDOBJBASE_H
