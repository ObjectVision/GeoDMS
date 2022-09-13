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

//----------------------------------------------------------------------
// static pointers for construction on demand
//----------------------------------------------------------------------

#ifndef __RTC_PTR_WEAKPTR_H
#define __RTC_PTR_WEAKPTR_H

#include "dbg/Check.h"
#include "ptr/PtrBase.h"
#include "utl/swap.h"

/*
 *	WeakPtr: pointer with object semantics, one can derive from it and access is checked
 */


template <class Ptr>
struct WeakPtrWrap : Ptr
{
	using typename Ptr::pointer;

	WeakPtrWrap(pointer ptr = nullptr) noexcept 
		: Ptr(ptr)
	{}

	WeakPtrWrap(const WeakPtrWrap& oth    ): Ptr(oth.m_Ptr) {}

	void assign(pointer ptr)                { Ptr::m_Ptr = ptr; }
	void reset()                            { Ptr::m_Ptr = nullptr;   }
	void operator =(pointer ptr)            { assign(ptr); }
	void operator =(const WeakPtrWrap& rhs) { assign(rhs.m_Ptr); }

	void swap(WeakPtrWrap& oth) { omni::swap(Ptr::m_Ptr, oth.m_Ptr); }
};

template <class T>
struct WeakPtr : WeakPtrWrap< ptr_base<T, copyable> >
{
	using base_type = WeakPtrWrap< ptr_base<T, copyable> >;
	using typename base_type::pointer;

	WeakPtr(pointer ptr = nullptr): base_type(ptr) {}
};

#endif // __RTC_PTR_WEAKPTR_H
