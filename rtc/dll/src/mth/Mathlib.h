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

#if !defined(__RTC_MTH_MATHLIB_H)
#define __RTC_MTH_MATHLIB_H

/******************************************************************************/
//                         Analytical Functions
/******************************************************************************/

template <class T> inline bool Even    (const T& t) { return t % 2 == 0; }
template <class T> inline bool Odd     (const T& t) { return ! Even(t); }
template <class T> inline bool PowerOf2(const T& t) { return (t & (t-1)) == 0; }
template <class T> inline T    sqr     (const T& v) { return v*v; }

#endif //!defined(__RTC_MTH_MATHLIB_H)
