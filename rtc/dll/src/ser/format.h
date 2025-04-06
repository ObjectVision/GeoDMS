// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once


#if !defined(__RTC_SER_FORMAT_H)
#define __RTC_SER_FORMAT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <boost/format.hpp>
#include "cpc/Types.h"

//----------------------------------------------------------------------
// format usage
//----------------------------------------------------------------------

template <typename Init>
auto modOperChain(Init&& init)
{
	return init;
}

template<typename Init, typename Arg0, typename ...Args>
auto modOperChain(Init&& init, Arg0&& arg0, Args&&... args)
{
	return modOperChain(std::forward<Init>(init) % std::forward<Arg0>(arg0), std::forward<Args>(args)...);
}


template<typename ...Args>
auto mgFormat(CharPtr msg, Args&&... args)
{
	return modOperChain(boost::format(msg), std::forward<Args>(args)...);
}

//----------------------------------------------------------------------
// format and consume rvalue references as values to avoid keeping TokenStr locks alive after the call
//----------------------------------------------------------------------

template <typename T>
using remove_rvalue_reference_t = std::conditional_t<std::is_rvalue_reference_v<T>, std::remove_reference_t<T>, T>;

#pragma warning (push)
#pragma warning (disable: 26800)

template <typename ...Args>
void release_resources(Args...) 
{
}

template<typename ...Args>
std::string mgFormat2string(CharPtr msg, Args&&... args)
{
	auto result = str(mgFormat<Args...>(msg, std::forward<Args>(args)...));

	release_resources<remove_rvalue_reference_t<Args>...>(std::forward<Args>(args)...);

	return result;
}

#pragma warning (pop)

#endif // __RTC_SER_FORMAT_H
