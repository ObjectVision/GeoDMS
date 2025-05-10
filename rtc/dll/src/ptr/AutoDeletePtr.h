// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
///////////////////////////////////////////////////////////////////////////// 

#if defined(_MSC_VER)
#pragma once
#endif


//----------------------------------------------------------------------
// static pointers for construction on demand
//----------------------------------------------------------------------

#ifndef __RTC_PTR_AUTODELETEPTR_H
#define __RTC_PTR_AUTODELETEPTR_H

#include "ptr/PtrBase.h"

/*
 *	AutoDeletePtr: pointer with object semantics, one can derive from it and access is checked
 */

template <typename T>
struct AutoDeletePtr : ptr_base<T, geodms::rtc::noncopyable>
{
	AutoDeletePtr(T* ptr) : ptr_base<T, geodms::rtc::noncopyable>(ptr) {}
	~AutoDeletePtr() 
	{
		if (this->get_ptr())
			this->get_ptr()->EnableAutoDelete();
	}
};

#endif // __RTC_PTR_AUTODELETEPTR_H
