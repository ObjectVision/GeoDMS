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

#if !defined(__PTR_SHAREDPTR_H)
#define __PTR_SHAREDPTR_H

//REMOVE #include <concrt.h>

#include "RtcBase.h"
#include "dbg/Check.h"
#include "dbg/DebugCast.h"
#include "ptr/PtrBase.h"

template <class Ptr>
struct SharedPtrWrap : Ptr
{
	using typename Ptr::pointer;

	SharedPtrWrap() noexcept : Ptr() 
	{
		dms_assert(!Ptr::m_Ptr);
	}

	SharedPtrWrap(const SharedPtrWrap& rhs) noexcept
		: Ptr(rhs)
	{ 
		IncCount();
	}

	SharedPtrWrap(SharedPtrWrap&& rhs) noexcept // we assume no external access to rhs as that would conflict with its rvalue-ness
		: Ptr() 
	{ 
		dms_assert(!Ptr::m_Ptr);
		swap(rhs);
	}

	~SharedPtrWrap() noexcept { DecCount(); }

	void operator = (SharedPtrWrap&& rhs) noexcept
	{
		swap(rhs);
	}

	void operator = (const SharedPtrWrap& rhs) noexcept
	{ 
		*this = SharedPtrWrap(rhs);
	}

	void assign(pointer ptr) noexcept // TODO G8: REMOVE, replace by reset
	{ 
		*this = SharedPtrWrap(ptr);
	}

	void reset(pointer ptr) noexcept
	{
		*this = SharedPtrWrap(ptr);
	}

	void reset() noexcept
	{
		assign(nullptr);
	}

	void swap(SharedPtrWrap& rhs) { omni::swap(Ptr::m_Ptr, rhs.m_Ptr); }
	friend void swap(SharedPtrWrap& a, SharedPtrWrap& b) { a.swap(b); }

protected:
	SharedPtrWrap(pointer ptr) : Ptr(ptr) { IncCount(); }

	void IncCount () const noexcept { if (Ptr::m_Ptr) Ptr::m_Ptr->IncRef(); }
	void DecCount () const noexcept 
	{ 
		if (!Ptr::m_Ptr)
			return;
		Ptr::m_Ptr->Release();
	}
};

template <class T>
struct SharedPtr : SharedPtrWrap<ptr_base<T, copyable> >
{
	using base_type = SharedPtrWrap<ptr_base<T, copyable> >;
	using typename base_type::pointer;

	SharedPtr(std::nullptr_t = nullptr) noexcept {}

	SharedPtr(SharedPtr&& rhs) noexcept
		: base_type(std::move(rhs))
	{ 
		assert(rhs.m_Ptr == nullptr); 
	}
	SharedPtr(const SharedPtr& rhs) = default;

	template <typename U>
	SharedPtr(U* rhs) noexcept
		:	base_type(rhs)
	{}
	template <typename SrcPtr>
	SharedPtr(SharedPtr<SrcPtr>&& rhs) noexcept
		: base_type(rhs.get_ptr())
	{}

	SharedPtr& operator =(const SharedPtr& rhs) = default;
	SharedPtr& operator =(SharedPtr&& rhs) = default;

	template <typename SrcPtr>
	void operator =(SrcPtr&& rhs) noexcept { *this = SharedPtr(std::forward<SrcPtr>(rhs)); }
};

template<typename T>
SharedPtr<T> MakeShared(T* ptr) { return ptr; }

//  -----------------------------------------------------------------------

#endif // __PTR_SHAREDPTR_H
