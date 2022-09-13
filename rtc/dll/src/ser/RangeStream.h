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

/*
 *  Description : Determine for each range type whether it is a binary streamable type
 *  Definition  : a "binary streamable type" is a type that contains no pointers
 */

#ifndef __SER_RANGEUTIL_H
#define __SER_RANGEUTIL_H

// Range
#include "dbg/Check.h"
#include "geo/Range.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"

//----------------------------------------------------------------------
// Section      : range streaming operators
//----------------------------------------------------------------------


template <class T> inline
BinaryOutStream& operator <<(BinaryOutStream& os, const Range<T>& r)
{
//	os << r.first << r.second;
	os.Buffer().WriteBytes((const char*)&r, sizeof(Range<T>));
	return os;
}

template <class T> inline
BinaryInpStream& operator >>(BinaryInpStream& is, Range<T>& r)
{
//	is >> r.first >> r.second;
	is.Buffer().ReadBytes((char*)&r, sizeof(Range<T>));
	return is;
}

template <class T>
FormattedOutStream& operator << (FormattedOutStream& os, const Range<T>& r)
{
	os << "[" << r.first << ", " << r.second << ") ";
	return os;
}

template <class T>
FormattedInpStream& operator >> (FormattedInpStream& is, Range<T>& r)
{
	is >> "[" >> r.first >> ", " >> r.second >> ")";
	return is;
}


#endif // __SER_RANGEUTIL_H
