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
#pragma once


#if !defined(__RTC_SER_FORMAT_H)
#define __RTC_SER_FORMAT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <boost/format.hpp>
#include "cpc/types.h"

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
