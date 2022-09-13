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

#if !defined(__RTC_MEM_MANAGEDALLOCDATA_IPP)
#define __RTC_MEM_MANAGEDALLOCDATA_IPP

#include "mem/ManagedAllocData.h"

#include "mem/MyAllocator.h"

//----------------------------------------------------------------------
// managed_alloc_data
//----------------------------------------------------------------------


template <typename V>
managed_alloc_data<V>::managed_alloc_data() 
{}

template <typename V>
managed_alloc_data<V>::managed_alloc_data(size_type sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	dms_assert(sz);
	first  = CreateMyAllocator<V>()->allocate(sz MG_DEBUG_ALLOCATOR_SRC_PARAM);
	second = first + sz;
	raw_awake_or_init(first, second, mustClear);
	m_Capacity = sz;
}

template <typename V>
managed_alloc_data<V>::managed_alloc_data(size_type sz, size_type capacity, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	dms_assert(capacity);
	dms_assert(sz <= capacity);
	first  = CreateMyAllocator<V>()->allocate(capacity MG_DEBUG_ALLOCATOR_SRC_PARAM);
	second = first + sz;
	raw_awake_or_init(first, second, mustClear);
	m_Capacity = capacity;
}

template <typename V>
managed_alloc_data<V>::managed_alloc_data(const_iterator first_, const_iterator last_, size_type capacity MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	dms_assert(capacity);
	dms_assert(size_type(last_ - first_) <= capacity);

	first  = CreateMyAllocator<V>()->allocate(capacity MG_DEBUG_ALLOCATOR_SRC_PARAM);

	second = raw_copy(first_, last_, first);
	m_Capacity = capacity;
}

template <typename V>
managed_alloc_data<V>::~managed_alloc_data()
{
	if (m_Capacity)
	{
		destroy_range(first, second);

		CreateMyAllocator<V>()->deallocate(first, m_Capacity);
	}
}
template <typename V>
typename managed_alloc_data<V>::size_type
managed_alloc_data<V>::max_size() const
{
	return CreateMyAllocator<V>()->max_size();
}


#endif //!defined(__RTC_MEM_MANAGEDALLOCDATA_IPP)
