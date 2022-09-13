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

#if !defined(__RTC_MEM_ALLOCDATA_H)
#define __RTC_MEM_ALLOCDATA_H

#include "geo/SequenceTraits.h"
#include "geo/IterRange.h"

//----------------------------------------------------------------------
// interfaces to allocated (file mapped?) arrays
//----------------------------------------------------------------------

template <typename V>
struct alloc_data : IterRange<typename sequence_traits<V>::pointer>
{
	using base_type = IterRange<typename sequence_traits<V>::pointer>;
	using typename base_type::size_type;
	using typename base_type::iterator;

	alloc_data() {}

	alloc_data(iterator first, iterator last, size_type cap)
		: base_type(first, last)
		, m_Capacity(cap) {}

	alloc_data(alloc_data&& rhs) : alloc_data()
	{
		this->operator =(rhs);
	}
	alloc_data& operator = (alloc_data&& rhs)
	{
		static_cast<base_type*>(this)->operator=( std::move<base_type&>(rhs) );
		m_Capacity = rhs.m_Capacity;
		static_cast<base_type&>(rhs) = {};
		rhs.m_Capacity = 0;
		return *this;
	}
	size_type capacity() const { return m_Capacity; }

	void swap(alloc_data<V>& oth)
	{
		base_type::swap(oth);
		omni::swap(m_Capacity, oth.m_Capacity);
	}

	size_type m_Capacity = 0;
};

#endif // __RTC_MEM_ALLOCDATA_H
