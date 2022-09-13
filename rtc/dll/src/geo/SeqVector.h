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

#if !defined(__RTC_GEO_SEQ_VECTOR_H)
#define __RTC_GEO_SEQ_VECTOR_H

#include "RtcBase.h"
#include "dbg/DebugCast.h"

#include <vector>

///=======================================
// std::vector<T>
//=======================================

#include "mem/MyAllocator.h"
#include "set/VectorFunc.h"

//----------------------------------------------------------------------
// Section      : std::vector specific Bounds functions 
//----------------------------------------------------------------------

template <typename Field>
inline void MakeUndefined(std::vector<Field>& v) 
{ 
	vector_clear(v);
}

template <typename Field>
inline void MakeUndefinedOrZero(std::vector<Field>& v) 
{ 
	vector_clear(v);
}

template <typename Field>
inline bool IsDefined(const std::vector<Field>& v) 
{ 
	return v.size(); 
}

template <typename Field>
std::vector<Field> UndefinedOrZero (const std::vector<Field>*) { return std::vector<Field>(); }



#endif // !defined(__RTC_GEO_SEQ_VECTOR_H)
