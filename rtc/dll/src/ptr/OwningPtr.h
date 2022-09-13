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

//	Template OwningPtr holds onto a pointer obtained via new and deletes that
//	object when it itself is destroyed (such as when leaving block scope). It
//	can be used to make calls to new() exception safe and as data-members. Copy
//	& assingment are non-const; the pointer is cleared after transfer
//	vector<owning_ptr< T > > is therefore not allowed.

#if !defined(__RTC_PTR_OWNINGPTR_H)
#define __RTC_PTR_OWNINGPTR_H

#include "dbg/Check.h"
#include "ptr/PtrBase.h"

#include <boost/checked_delete.hpp>

template <class T>
struct OwningPtr : ptr_base<T, movable>
{
	using base_type = ptr_base<T, movable>;
	using typename base_type::pointer;

	OwningPtr(pointer ptr = nullptr) noexcept
		: base_type(ptr)
	{}
	OwningPtr(OwningPtr&& org) noexcept
		: base_type(org.release())
	{}

	~OwningPtr () noexcept { reset(); }

	void    init   (pointer ptr)       noexcept { dms_assert(this->is_null()); this->m_Ptr = ptr; }
	pointer release()                  noexcept { pointer tmp_ptr = this->m_Ptr; this->m_Ptr = nullptr; return tmp_ptr; }
	void    reset  ()                  noexcept { assign(nullptr); }
	void    assign (pointer ptr)       noexcept { dms_assert(this->m_Ptr != ptr || !ptr); std::swap(this->m_Ptr, ptr); boost::checked_delete(ptr); }
	void    swap   (OwningPtr<T>& oth) noexcept { std::swap(this->m_Ptr, oth.m_Ptr); }

	void operator = (OwningPtr&& rhs) noexcept { assign(rhs.release()); }

	friend void swap(OwningPtr& a, OwningPtr& b) noexcept { a.swap(b); }

	// illegal copy ctors
	OwningPtr(const OwningPtr<T>& oth) = delete;
};


#endif // __RTC_PTR_OWNINGPTR_H
