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

#if !defined(__RTC_PTR_STATIC_H)
#define __RTC_PTR_STATIC_H

#include "dbg/Check.h"

#include <boost/checked_delete.hpp>
#include <boost/noncopyable.hpp>

template <class T>
struct static_ptr: boost::noncopyable
{
	typedef T  value_type;
	typedef T* pointer;
	typedef T& reference;

	bool has_ptr() const { return m_Ptr != nullptr; }
	bool is_null() const { return m_Ptr == nullptr; }

	pointer   get_ptr() const { return m_Ptr; }
	reference get_ref() const { dms_assert(m_Ptr != nullptr); return *m_Ptr; }

	operator pointer () const { return get_ptr(); }
	pointer   operator ->() const { return &get_ref(); }
	reference operator * () const { return  get_ref(); }

	void assign(pointer ptr) 
	{ 
		dms_assert(is_null()); // may only be called once, or after reset
		dms_assert(ptr);      // and make client prevent unnessecary looping 
		m_Ptr = ptr; 
	}

	void reset()
	{
		dms_assert(!is_null());
		boost::checked_delete(m_Ptr);
		m_Ptr = nullptr;
	}
protected:
	pointer m_Ptr;
};

//  -----------------------------------------------------------------------

#endif // __RTC_PTR_STATIC_H
