// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
///////////////////////////////////////////////////////////////////////////// 

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_UTL_SWAPPER_H)
#define __RTC_UTL_SWAPPER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "utl/NonCopyable.h"
#include "vt/ElemTraits.h"

//----------------------------------------------------------------------
// tmp_swapper gives a variable another value for the lifetime of the swapper
//----------------------------------------------------------------------

template <class T>
struct tmp_swapper  : private geodms::rtc::noncopyable
{
	tmp_swapper(T& resourceHandle, typename param_type<T>::type tmpValue)
		:	m_ResourceHandleRef (resourceHandle)
		,	m_ResourceHandleCopy(resourceHandle) 
	 { 
		 resourceHandle = tmpValue;
	 }
	~tmp_swapper() // restore old situation
	 { 
		m_ResourceHandleRef = m_ResourceHandleCopy;
	 }

	using param_t = typename param_type<T>::type;

	param_t PreviousValue() const { return m_ResourceHandleCopy; }

	T& GetRef() { return m_ResourceHandleRef; }

protected:
	T& m_ResourceHandleRef;
	T  m_ResourceHandleCopy;
};

#endif // __RTC_UTL_SWAPPER_H
