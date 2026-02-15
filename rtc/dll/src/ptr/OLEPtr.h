// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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