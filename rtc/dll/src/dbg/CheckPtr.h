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

#if !defined(__RTC_DBG_CHECKPTR_H)
#define __RTC_DBG_CHECKPTR_H

#include "RtcBase.h"
//----------------------------------------------------------------------
// CheckPtr
//----------------------------------------------------------------------

// MG_DEBUG installs code to check the validity of Object* and their dynamic-types. 
// Complexity: O(log(#Objects)) if MG_CHECKPTR is defined in Persistent.cpp 
// ifdef  MG_DEBUG, but not MG_CHECKPTR, only non-null check is performed. 
// ifndef MG_DEBUG, even this check is omitted.

#if defined(MG_DEBUG)
	RTC_CALL void CheckPtr(const Object* item, const Class* cls, CharPtr dmsFunc);
#else
	inline void CheckPtr(const Object* item, const Class* cls, CharPtr dmsFunc) {}
#endif


#endif // __RTC_DBG_CHECKPTR_H
