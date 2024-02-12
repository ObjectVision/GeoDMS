// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SER_ASSTRING_H

#include "RtcBase.h"

#include "ptr/SharedStr.h"
#include "ser/FormattingFlags.h"
#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/Quotes.h"

#define __SER_ASSTRING_H

//----------------------------------------------------------------------
// AsString
//----------------------------------------------------------------------

template <class T>
inline SharedStr AsString(const T& value, FormattingFlags ff = FormattingFlags::ThousandSeparator)
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
//inline SharedStr AsString(FormattedOutStream&, const SharedStr&  v) { return v; }

// ===================== AsDataStr

template <typename T>
inline SharedStr AsDataStr(const T& v) { return AsString(v);    }
inline SharedStr AsDataStr(Bool     v) { return SharedStr(v ? "true" : "false");  }
inline SharedStr AsDataStr(WeakStr  v) { return DoubleQuote(v.c_str()); }
inline SharedStr AsDataStr(CharPtr  v) { return DoubleQuote(v); }
inline SharedStr AsDataStr(const SharedStr&  v) { return AsDataStr(typesafe_cast<WeakStr>(v)); }

template <typename T>
inline void WriteDataString(FormattedOutStream& out, const T& v) { out << v;    }
RTC_CALL void WriteDataString(FormattedOutStream& out, Bool     v);
RTC_CALL void WriteDataString(FormattedOutStream& out, WeakStr  v);
RTC_CALL void WriteDataString(FormattedOutStream& out, CharPtr  v);
RTC_CALL void WriteDataString(FormattedOutStream& out, const SharedStr& v);

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
	SilentMemoOutStreamBuff memoBuf(ByteRange(buffer, buffer + bufLen));
	FormattedOutStream outStream(&memoBuf, ff);
	outStream << value ;
	if (memoBuf.CurrPos() >= bufLen)
		return false;
	outStream << char('\0');
	return true;
}


inline bool AsCharArray(CharPtr value, char* buffer, SizeT bufLen, FormattingFlags)
{ 
//	strncpy(buffer, value, bufLen); 
	while ((bufLen--) && (*buffer++ = *value++)) ; 
	return !buffer[-1];
}

inline bool AsCharArray(WeakStr value, char* buffer, SizeT bufLen, FormattingFlags)
{ 
	SizeT size = value.ssize();
	buffer = fast_copy(value.begin(), value.begin()+Min<SizeT>(size, bufLen), buffer);
	if (size >= bufLen)
		return false;
	*buffer = char(0);
	return true;
}

inline bool AsCharArray(const SharedStr& value, char* buffer, SizeT bufLen, FormattingFlags ff) 
{ 
	return AsCharArray(typesafe_cast<WeakStr>(value), buffer, bufLen, ff); 
}

RTC_CALL bool AsCharArray(SA_ConstReference<char> value, char* buffer, SizeT bufLen, FormattingFlags);

template <class T>
inline SizeT AsCharArraySize(const T& value, streamsize_t maxLen, FormattingFlags ff)
{
	FiniteNullOutStreamBuff nullBuf(maxLen);
	FormattedOutStream outStream(&nullBuf, ff);
	outStream << value ;
	return Min<streamsize_t>(nullBuf.CurrPos(), maxLen);
}

inline SizeT AsCharArraySize(CharPtr value, streamsize_t maxLen, FormattingFlags) { return StrLen(value, maxLen); }
inline SizeT AsCharArraySize(WeakStr value, streamsize_t maxLen, FormattingFlags) { return Min<streamsize_t>(maxLen, value.ssize()); }
inline SizeT AsCharArraySize(const SharedStr& value, streamsize_t maxLen, FormattingFlags) { return Min<streamsize_t>(maxLen, value.ssize()); }
RTC_CALL SizeT AsCharArraySize(SA_ConstReference<char> value, streamsize_t maxLen, FormattingFlags);

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

RTC_CALL SharedStr AsString(const StringCRef& v);
RTC_CALL SharedStr AsDataStr(const StringCRef& v);
RTC_CALL void WriteDataString(FormattedOutStream& out, const StringCRef& v);


RTC_CALL void StringRef_resize_uninitialized(StringRef& res, SizeT n);
RTC_CALL Char*   begin_ptr(StringRef& res);
RTC_CALL CharPtr begin_ptr(StringCRef& res);
RTC_CALL Char* end_ptr(StringRef& res);
RTC_CALL CharPtr end_ptr(StringCRef& res);

template <typename T>
inline void AsString(StringRef& res, const T& v)
{
	SizeT n = AsCharArraySize(v, -1, FormattingFlags::None);
	assert(n);
	StringRef_resize_uninitialized(res, n);
	AsCharArray(v, begin_ptr(res), n, FormattingFlags::None);
}


RTC_CALL SizeT StrLen(const StringCRef& x, SizeT maxLen);

RTC_CALL void AsString(StringRef& res, const double& value, UInt8 decPos);
RTC_CALL void AsHex(StringRef& res, UInt32 v);
RTC_CALL void AsHex(StringRef& res, StringCRef v);

#endif // __SER_ASSTRING_H