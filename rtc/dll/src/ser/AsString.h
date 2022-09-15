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

#ifndef __SER_ASSTRING_H
#define __SER_ASSTRING_H


#include "geo/IterRange.h"
#include "ptr/SharedStr.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"
#include "ser/StringStream.h"
#include "ser/SequenceArrayStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/Quotes.h"

//----------------------------------------------------------------------
// AsString
//----------------------------------------------------------------------

template <class T>
inline SharedStr AsString(const T& value, FormattingFlags ff = FormattingFlags::None)
{
	SizeT size = AsCharArraySize(value, -1, ff);
	if (!size)
		return SharedStr();
	SharedCharArray* resultPtr = SharedCharArray::CreateUninitialized(size+1);
	SharedStr result(resultPtr);
	AsCharArray(value, result.begin(), size, ff);
	resultPtr->back() = char('\0');
	return result;
}

inline SharedStr AsString(WeakStr   value) { return SharedStr(value); }
inline SharedStr AsString(SharedStr value) { return value; }
inline SharedStr AsString(CharPtr   value) { return SharedStr(value); }
inline SharedStr AsString(FormattedOutStream& out, const SharedStr&  v) { return v; }

// ===================== AsDataStr

template <typename T>
inline SharedStr AsDataStr(const T& v) { return AsString(v);    }
inline SharedStr AsDataStr(Bool     v) { return SharedStr(v ? "true" : "false");  }
inline SharedStr AsDataStr(WeakStr  v) { return DoubleQuote(v.c_str()); }
inline SharedStr AsDataStr(CharPtr  v) { return DoubleQuote(v); }
inline SharedStr AsDataStr(const SharedStr&  v) { return AsDataStr(typesafe_cast<WeakStr>(v)); }

template <typename T>
inline void WriteDataString(FormattedOutStream& out, const T& v) { out << v;    }
inline void WriteDataString(FormattedOutStream& out, Bool     v) { out << (v ? "true" : "false");   }
inline void WriteDataString(FormattedOutStream& out, WeakStr  v) { DoubleQuote(out, v.begin(), v.send()); }
inline void WriteDataString(FormattedOutStream& out, CharPtr  v) { DoubleQuote(out, v); }
inline void WriteDataString(FormattedOutStream& out, const SharedStr& v) { WriteDataString(out, typesafe_cast<WeakStr>(v)); }

template <typename T> 
inline   SharedStr AsRgbStr(const T& v) { return AsDataStr(v); }
RTC_CALL SharedStr AsRgbStr(UInt32   v); // defined in StringStream.cpp

template <class T>
void AssignValueFromCharPtr(T& value, CharPtr data) 
{ 
	MemoInpStreamBuff memoStr(data);
	FormattedInpStream inpStream(&memoStr);
	inpStream >> value;
}

template <class T>
void AssignValueFromCharPtrs(T& value, CharPtr begin, CharPtr end)
{ 
	MemoInpStreamBuff memoStr(begin, end);
	FormattedInpStream inpStream(&memoStr);
	inpStream >> value;
}

template <class T>
void AssignValueFromCharPtrs_Checked(T& value, CharPtr begin, CharPtr end)
{
	return AssignValueFromCharPtrs(value, begin, end);
}

template <class T>
inline bool AsCharArray(const T& value, char* buffer, SizeT bufLen, FormattingFlags ff)
{
	SilentMemoOutStreamBuff memoBuf(buffer, buffer + bufLen);
	FormattedOutStream outStream(&memoBuf, ff);
	outStream << value ;
	if (memoBuf.CurrPos() >= bufLen)
		return false;
	outStream << char('\0');
	return true;
}


inline bool AsCharArray(CharPtr value, char* buffer, SizeT bufLen, FormattingFlags ff)
{ 
//	strncpy(buffer, value, bufLen); 
	while ((bufLen--) && (*buffer++ = *value++)) ; 
	return !buffer[-1];
}

inline bool AsCharArray(WeakStr value, char* buffer, SizeT bufLen, FormattingFlags ff)
{ 
	SizeT size = value.ssize();
	buffer = fast_copy(value.begin(), value.begin()+Min<SizeT>(size, bufLen), buffer);
	if (size >= bufLen)
		return false;
	*buffer = char(0);
	return true;
}

inline bool AsCharArray(const SharedStr& value, char* buffer, SizeT bufLen, FormattingFlags ff) { return AsCharArray(typesafe_cast<WeakStr>(value), buffer, bufLen, ff); }
inline bool AsCharArray(SA_ConstReference<char> value, char* buffer, SizeT bufLen, FormattingFlags ff)
{
	SizeT size;
	CharPtr valueBegin;
	if (!value.IsDefined())
	{
		size = UNDEFINED_VALUE_STRING_LEN;
		valueBegin = UNDEFINED_VALUE_STRING;
	}
	else
	{
		size = value.size();
		valueBegin = value.begin();
	}
	buffer = fast_copy(valueBegin, valueBegin + Min<SizeT>(size, bufLen), buffer);
	if (size >= bufLen)
		return false;
	*buffer = char(0);
	return true;
}


template <class T>
inline SizeT AsCharArraySize(const T& value, streamsize_t maxLen, FormattingFlags ff)
{
	FiniteNullOutStreamBuff nullBuf(maxLen);
	FormattedOutStream outStream(&nullBuf, ff);
	outStream << value ;
	return Min<streamsize_t>(nullBuf.CurrPos(), maxLen);
}

inline SizeT AsCharArraySize(CharPtr value, streamsize_t maxLen, FormattingFlags ff) { return StrLen(value, maxLen); }
inline SizeT AsCharArraySize(WeakStr value, streamsize_t maxLen, FormattingFlags ff) { return Min<streamsize_t>(maxLen, value.ssize()); }
inline SizeT AsCharArraySize(const SharedStr& value, streamsize_t maxLen, FormattingFlags ff) { return Min<streamsize_t>(maxLen, value.ssize()); }
inline SizeT AsCharArraySize(SA_ConstReference<char> value, streamsize_t maxLen, FormattingFlags ff) { return Min<streamsize_t>(maxLen, value.size()); }

//----------------------------------------------------------------------
// Section : IString, used for returning string-handles to ClientAppl
//----------------------------------------------------------------------
// Implementation in StringStream.cpp; 
// DEBUG: all members check the this pointer before using it and leaks are detected at the destruction of iStringComponentLock
//----------------------------------------------------------------------

struct IString : SharedStr
{
	typedef IStringHandle Handle;

	static RTC_CALL Handle Create(CharPtr strVal);
	static RTC_CALL Handle Create(CharPtr strBegin, CharPtr strEnd);
	static RTC_CALL Handle Create(WeakStr strVal);
	static RTC_CALL Handle Create(TokenID strVal);

	static RTC_CALL void    Release  (Handle);
	static RTC_CALL CharPtr AsCharPtr(Handle);


private:
	IString(CharPtr strVal);
	IString(CharPtr strBegin, CharPtr strEnd);
	IString(WeakStr strVal);
	IString(TokenID strVal);

	~IString();
	void Init();
};

//----------------------------------------------------------------------
// Section      : various StringRef functions
//----------------------------------------------------------------------

inline   SharedStr AsString(const StringCRef& v) { return v.IsDefined() ? SharedStr(begin_ptr(v), end_ptr(v)) : SharedStr(Undefined()); }
RTC_CALL SharedStr AsDataStr(const StringCRef& v);
RTC_CALL void WriteDataString(FormattedOutStream& out, const StringCRef& v);

template <typename T>
inline void AsString(StringRef& res, const T& v)
{
	SizeT n = AsCharArraySize(v, -1, FormattingFlags::None);
	dms_assert(n);
	res.resize_uninitialized(n);
	AsCharArray(v, &*res.begin(), n, FormattingFlags::None);
}


inline SizeT StrLen(const StringCRef& x, SizeT maxLen)
{
	return StrLen(x.begin(), x.end());
}



inline void AsString(StringRef& res, const double& value, UInt8 decPos)
{
	if (IsDefined(value))
	{
		char charBuf[255 + 26];
		UInt32 n = _snprintf(charBuf, 255 + 26, "%.*G", int(decPos), value);
		dms_assert(n);
		res.assign(charBuf, charBuf + n);
	}
	else
		res.assign(Undefined());
}

RTC_CALL void AsHex(StringRef& res, UInt32 v);
RTC_CALL void AsHex(StringRef& res, StringCRef v);


#endif // __SER_ASSTRING_H