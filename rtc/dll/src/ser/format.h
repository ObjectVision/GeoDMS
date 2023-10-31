// Copyright (C) 2023 Object Vision b.v. 
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

template<typename ...Args>
std::string mgFormat2string(CharPtr msg, Args&&... args)
{
	return str(mgFormat(msg, std::forward<Args>(args)...));
}


#endif // __RTC_SER_FORMAT_H
