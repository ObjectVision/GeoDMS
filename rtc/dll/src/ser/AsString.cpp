// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ser/AsString.h"
#include "geo/iterrange.h"
#include "ptr/SharedStr.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"
#include "ser/StringStream.h"
#include "ser/SequenceArrayStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/mySPrintF.h"
#include "utl/Quotes.h"


//----------------------------------------------------------------------
// helper funcs
//----------------------------------------------------------------------

void StringRef_resize_uninitialized(StringRef& res, SizeT n)
{
	res.resize_uninitialized(n);
}

Char* begin_ptr(StringRef& res)
{
	if (!res.IsDefined())
		return {};
	return &*res.begin();
}

CharPtr begin_ptr(StringCRef& res)
{
	if (!res.IsDefined())
		return {};
	return &*res.begin();
}

Char* end_ptr(StringRef& res)
{
	if (!res.IsDefined())
		return {};
	return &*res.end();
}

CharPtr end_ptr(StringCRef& res)
{
	if (!res.IsDefined())
		return {};
	return &*res.end();
}

SizeT StrLen(const StringCRef& x, SizeT maxLen)
{
	MakeMin(maxLen, x.size());
	return StrLen(x.begin(), maxLen);
}


//----------------------------------------------------------------------
// AsString
//----------------------------------------------------------------------

bool AsCharArray(SA_ConstReference<char> value, char* buffer, SizeT bufLen, FormattingFlags)
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

SizeT AsCharArraySize(SA_ConstReference<char> value, streamsize_t maxLen, FormattingFlags) { return Min<streamsize_t>(maxLen, value.size()); }

void AsString(StringRef& res, const double& value, UInt8 decPos)
{
	if (IsDefined(value))
	{
		char charBuf[255 + 26];
		auto n = snprintf(charBuf, 255 + 26, "%.*G", int(decPos), value);
		if (n > 0)
			res.assign(charBuf, charBuf + n);
	}
	else
		res.assign(Undefined());
}


//----------------------------------------------------------------------
// WriteDataString
//----------------------------------------------------------------------

SharedStr AsString(const StringCRef& v) 
{ 
	return v.IsDefined() 
		? SharedStr(begin_ptr(v), end_ptr(v)) 
		: SharedStr(Undefined()); 
}

RTC_CALL void WriteDataString(FormattedOutStream& out, WeakStr  v)
{ 
	DoubleQuote(out, v.begin(), v.send()); 
}

RTC_CALL void WriteDataString(FormattedOutStream& out, CharPtr  v) 
{ 
	DoubleQuote(out, v); 
}

RTC_CALL void WriteDataString(FormattedOutStream& out, const SharedStr& v) 
{ 
	WriteDataString(out, typesafe_cast<WeakStr>(v)); 
}

