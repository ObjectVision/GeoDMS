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

#if !defined(__RTC_SER_STRINGSTREAM_H)
#define __RTC_SER_STRINGSTREAM_H
#pragma once

#include "dbg/DebugCast.h"
#include "ser/BinaryStream.h"
#include "ser/PolyStream.h"
#include "ser/FormattedStream.h"
#include "ptr/SharedStr.h"

//----------------------------------------------------------------------
// Section      : Serialization support for SharedStr
//----------------------------------------------------------------------

RTC_CALL BinaryOutStream& operator <<(BinaryOutStream& ar, WeakStr    str);
RTC_CALL BinaryInpStream& operator >>(BinaryInpStream& ar, SharedStr& str);

inline PolymorphOutStream& operator <<(PolymorphOutStream& ar, WeakStr str)
{
	typesafe_cast<BinaryOutStream&>(ar) << str;
	return ar;
}

inline PolymorphInpStream& operator >>(PolymorphInpStream& ar, SharedStr& str)
{
	typesafe_cast<BinaryInpStream&>(ar) >> str;
	return ar;
}

RTC_CALL FormattedInpStream& operator >> (FormattedInpStream& is, SharedStr& cap);

/* REMOVE, replaced by CharPtrRange
inline FormattedOutStream& operator << (FormattedOutStream& os, SharedStr cap)
{
	os << cap.AsRange();
	return os;
}

inline FormattedOutStream& operator << (FormattedOutStream& os, WeakStr cap)
{
	os << cap.AsRange();
	return os;
}
*/
//----------------------------------------------------------------------
// Section      : Serialization support for TokenID
//----------------------------------------------------------------------

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, TokenID value);
RTC_CALL FormattedInpStream& operator >>(FormattedInpStream& str, TokenID& value);

//----------------------------------------------------------------------
// Section      : AssignValueFromCharPtr
//----------------------------------------------------------------------

inline void AssignValueFromCharPtr (CharPtr& value, CharPtr data)               { value = data; }
inline void AssignValueFromCharPtr (SharedStr& value, CharPtr data)               { value = SharedCharArray_Create(data); }
inline void AssignValueFromCharPtrs(SharedStr& value, CharPtr begin, CharPtr end) { value = SharedCharArray_Create(begin, end); }

inline void AssignValueFromCharPtr (TokenID&  value, CharPtr data)               { value = TokenID(data, (mt_tag*) nullptr); }
inline void AssignValueFromCharPtrs(TokenID&  value, CharPtr begin, CharPtr end) { value = TokenID(begin, end, (mt_tag*) nullptr); }

#endif //__RTC_SER_STRINGSTREAM_H