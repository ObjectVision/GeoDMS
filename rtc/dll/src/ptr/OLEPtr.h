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
#ifndef __RTC_PTR_OLEPTR_H
#define __RTC_PTR_OLEPTR_H

// Check
#include "dbg/Check.h"
#include "ptr/PtrBase.h"

template <class T>
class OlePtr : public ptr_base<T, noncopyable> // TODO: Make movable
{
	using base_type = ptr_base<T, noncopyable>;
  public:
	explicit OlePtr (T* ptr = 0)           : base_type(ptr) {}
	explicit OlePtr (const OlePtr<T>& ptr) : base_type(ptr.get_ptr()) {}

	~OlePtr () { Release(); }

	void Reset (T* ptr = 0)
	{
		if (this->m_Ptr != ptr)
		{
			Release();
			this->m_Ptr = ptr;
		}
	}

	void operator = (OlePtr<T>& rhs) { Reset(rhs.get_ptr()); }
	void operator = (T*         rhs) { Reset(rhs);           }

 private:
	friend UInt32 Release(T*);
	void Release () 
	{
		dms_check_not_debugonly; 
		if (this->has_ptr()) ::Release(this->get_ptr());
	}
};

#define SUPPORT_OLEPTR(Cls) UInt32 Release(Cls* ptr) { return ptr->Release(); }
#endif // __RTC_PTR_OLEPTR_H