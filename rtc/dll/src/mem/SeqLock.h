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

#if !defined(__RTC_MEM_SEQLOCK_H)
#define __RTC_MEM_SEQLOCK_H

#include "ptr/PtrBase.h"

//----------------------------------------------------------------------
// interfaces to allocated (file mapped?) arrays
//----------------------------------------------------------------------

template <typename Seq>
struct SeqLock : ptr_base<Seq, movable>
{
	using base_type = ptr_base<Seq, movable>;
	SeqLock()
	{}

	SeqLock(Seq& seq, dms_rw_mode rw)
		:	base_type(&seq)
	{
		MGD_CHECKDATA(!seq.IsLocked());
		seq.Lock(rw);
	}
	SeqLock(SeqLock&& rhs)
		: base_type(rhs.m_Seq)
	{
		MGD_CHECKDATA(this->m_Seq.has_ptr  ());
		MGD_CHECKDATA(this->m_Seq->IsLocked());
	}
	void operator =(SeqLock&& rhs)
	{
		swap(rhs);
		rhs.reset();
	}

	void release()
	{
		this->m_Ptr = nullptr;
	}

	void reset()
	{
		if (this->m_Ptr)
		{
			this->m_Ptr->UnLock();
			release();
		}
	}
	~SeqLock()
	{
		reset();
	}
};

#endif // __RTC_MEM_SEQLOCK_H
