// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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