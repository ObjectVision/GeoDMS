// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
///////////////////////////////////////////////////////////////////////////// 

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_PTR_STATIC_H)
#define __RTC_PTR_STATIC_H

#include "dbg/Check.h"
#include "utl/NonCopyable.h"

template <class T>
struct static_ptr: geodms::rtc::noncopyable
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
		assert(!is_null());
		static_assert(sizeof(T) != 0, "Type must be complete");
		delete m_Ptr;
		m_Ptr = nullptr;
	}
protected:
	pointer m_Ptr;
};

//  -----------------------------------------------------------------------

#endif // __RTC_PTR_STATIC_H
