// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

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
//	SeqLock() {}

	SeqLock(Seq& seq, dms_rw_mode rw)
		:	base_type(&seq)
	{
		MGD_CHECKDATA(!seq.IsLocked());
		seq.Lock(rw);
	}
/*
	SeqLock(SeqLock&& rhs)
		: base_type(rhs.m_Seq)
	{
		MGD_CHECKDATA(this->m_Seq.has_ptr  ());
		MGD_CHECKDATA(this->m_Seq->IsLocked());
	}
*/
	~SeqLock()
	{
		reset();
	}
/*
	void operator =(SeqLock&& rhs)
	{
		swap(rhs);
		rhs.reset();
	}
	*/

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
};

#endif // __RTC_MEM_SEQLOCK_H
