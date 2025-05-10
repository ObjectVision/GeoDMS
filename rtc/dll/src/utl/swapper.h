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

#include "utl/noncopyable.h"
#include "utl/swap.h"
#include "geo/ElemTraits.h"

//----------------------------------------------------------------------
// mt_swap(per) helps to do thread safe resource swapping/locking
//----------------------------------------------------------------------

template <class T>
void mt_swap(T& a, T& b)
{
//	using namespace std::swap;
//	NYI: Mutex (mutally exclusion around swap function
	omni::swap(a, b);
//	T tmp = a; a=b; b=tmp;
}

template <class T>
struct mt_swapper : private geodms::rtc::noncopyable
{
	 mt_swapper(T& resourceHandle, const T& blockValue)
		: m_ResourceHandleRef(resourceHandle)
		, m_ResourceHandleCopy(blockValue) 
	 { 
		 mt_swap(m_ResourceHandleRef, m_ResourceHandleCopy); 
	 }
	~mt_swapper()
	 { 
		 mt_swap(m_ResourceHandleRef, m_ResourceHandleCopy); 
	 }

	 bool     WasBlocked() const { return m_ResourceHandleRef == m_ResourceHandleCopy; }
	 const T& GetValue  () const { return m_ResourceHandleCopy; }

protected:
	T& m_ResourceHandleRef;
	T  m_ResourceHandleCopy;
};

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
