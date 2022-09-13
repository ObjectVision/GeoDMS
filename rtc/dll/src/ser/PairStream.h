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

#ifndef __RTC_SER_PAIRSTREAM_H
#define __RTC_SER_PAIRSTREAM_H

#include "geo/Pair.h"
#include "ser/FormattedStream.h"

//----------------------------------------------------------------------
// Section      : Serialization support for Tuples
//----------------------------------------------------------------------

template <class T1, class T2> inline
BinaryOutStream& operator <<(BinaryOutStream& ar, const Pair<T1, T2>& t)
{
	ar << t.first << t.second;
	return ar;
}

template <class T1, class T2> inline
BinaryInpStream& operator >>(BinaryInpStream& ar,       Pair<T1, T2>& t)
{
	ar >> t.first >> t.second;
	return ar;
}

template <class T1, class T2> inline
FormattedOutStream& operator << (FormattedOutStream& os, const Pair<T1, T2>& p)
{
	os << "{" << p.first << ", " << p.second << "}";
	return os;
}

template <class T1, class T2> inline
FormattedInpStream& operator >> (FormattedInpStream& is, Pair<T1, T2>& p)
{
	is >> "{" >> p.first >> ", " >> p.second >> "}";
	return is;
}

#endif // __RTC_SER_PAIRSTREAM_H
