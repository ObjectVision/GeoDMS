
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

#if !defined(__RTC_GEO_MINMAX_H)
#define __RTC_GEO_MINMAX_H

#include "geo/ElemTraits.h"
#include "utl/swap.h"

//----------------------------------------------------------------------
// Section      : lexicographical ordering: MakeMin, MakeMax, etc.
//----------------------------------------------------------------------
// Full name    : Aggregation helper definitioin
// Purpose      : fast operation for aggregation operations
//----------------------------------------------------------------------

template <class T>
inline T Min(T a, T b) { return (a<b) ? a : b; }

template <class T>
inline T Max(T a, T b) { return (a<b) ? b : a; }

template <class T>
inline bool MakeMin(T& a, std::type_identity_t<T> b) { if (!(b<a)) return false; a=b; return true; }

template <class T>
inline bool MakeMax(T& a, std::type_identity_t<T> b) { if (!(a<b)) return false; a=b; return true; }

template <class T> 
inline void MakeRange(T& a, T& b) { if (b<a) omni::swap(a, b); }

#endif // !defined(__RTC_GEO_MINMAX_H)
