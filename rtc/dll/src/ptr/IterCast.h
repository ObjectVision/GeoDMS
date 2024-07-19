// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_PTR_ITERCAST_H)
#define __RTC_PTR_ITERCAST_H

#include "cpc/CompChar.h"
#include "dbg/Check.h"
//#include "geo/SequenceTraits.h"

//=======================================
// conversion of container range to dumb pointers 
//=======================================

template <typename Container>
typename std::iterator_traits<typename Container::iterator>::pointer begin_ptr(Container& data) 
{ 
#if defined(CC_ITERATOR_CHECKED)
	if (data.empty())
		return {};
	return &*data.begin(); 
#else
	return data.begin(); // we don't allow Container::iterator to be silently smart 
#endif 
}
template <typename Container>
typename std::iterator_traits<typename Container::iterator>::pointer  end_ptr(Container& data) 
{
#if defined(CC_ITERATOR_CHECKED)
	if (data.empty())
		return {};
	return (&(data.end()[-1]))+1; 
#else
	return data.end(); // we don't allow Container::iterator to be silently smart
#endif 
}

template <typename Container>
typename std::iterator_traits<typename Container::const_iterator>::pointer  begin_ptr(const Container& data) 
{ 
#if defined(CC_ITERATOR_CHECKED)
	if (data.empty())
		return {};
	return &*data.begin(); 
#else
	return data.begin(); // we don't allow Container::iterator to be silently smart 
#endif 
}
template <typename Container>
typename std::iterator_traits<typename Container::const_iterator>::pointer  end_ptr(const Container& data) 
{
#if defined(CC_ITERATOR_CHECKED)
	if (data.empty())
		return {};
	return (&(data.end()[-1]))+1; 
#else
	return data.end(); // we don't allow Container::iterator to be silently smart
#endif 
}

template <typename Container>
auto cbegin_ptr(const Container& data)
{
	return begin_ptr(data);
}

template <typename Container>
auto cend_ptr(const Container& data)
{
	return end_ptr(data);
}

#endif // !defined(__RTC_PTR_ITERCAST_H)
