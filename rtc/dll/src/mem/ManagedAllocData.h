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

#if !defined(__RTC_MEM_MANAGEDALLOCDATA_H)
#define __RTC_MEM_MANAGEDALLOCDATA_H

#include "mem/AllocData.h"
#include "mem/FixedAlloc.h"

//----------------------------------------------------------------------
// managed_alloc_data
//----------------------------------------------------------------------

template <typename V>
struct managed_alloc_data : alloc_data<V>
{
	using typename alloc_data<V>::size_type;
	using alloc_data<V>::first;
	using alloc_data<V>::second;
	using alloc_data<V>::m_Capacity;

	managed_alloc_data();

	typedef typename sequence_traits<V>::const_pointer const_iterator;
	managed_alloc_data(size_type sz, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG);
	managed_alloc_data(size_type sz, size_type capacity, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG);
	managed_alloc_data(const_iterator first_, const_iterator last_, size_type capacity MG_DEBUG_ALLOCATOR_SRC_ARG);

	~managed_alloc_data();

	size_type max_size() const;
};


#endif //!defined(__RTC_MEM_MANAGEDALLOCDATA_H)
