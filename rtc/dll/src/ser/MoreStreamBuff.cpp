// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// (BaseStreamBuff and AsString merged in, 2026-08)


#include "dbg/DmsCatch.h"
#include "vt/StringBounds.h"
#include "ser/MoreStreamBuff.h"
#include "ser/StreamException.h"
#include "utl/Environment.h"

// *****************************************************************************
// Section:     Basic streambuffer Interface
// *****************************************************************************


/********** MemoInpStreamBuff Implementation **********/

MemoInpStreamBuff::MemoInpStreamBuff(const Byte* begin, const Byte* end)
	: m_Data(begin, end)
	, m_Curr(begin)
{
	if (end==nullptr && begin != nullptr)
		m_Data.second = begin + StrLen(begin); // go to null termination.
}

void MemoInpStreamBuff::ReadBytes (Byte* data, streamsize_t size) const 
{
	dms_assert(m_Curr <= m_Data.end());
	if (size > streamsize_t(m_Data.end() - m_Curr))
	{
		fast_fill(
			fast_copy(m_Curr, m_Data.end(), data)
		,	data+size
		,	EOF
		);
		m_Curr = m_Data.end();
	}
	else
	{
		CharPtr next = m_Curr + size;
		fast_copy(m_Curr, next, data);
		m_Curr = next;
	}
}

streamsize_t MemoInpStreamBuff::CurrPos() const
{
	return m_Curr - m_Data.begin();
}


CharPtr MemoInpStreamBuff::GetDataBegin()
{
	return m_Data.begin();
}

CharPtr MemoInpStreamBuff::GetDataEnd()
{
	return m_Data.end();
}

void MemoInpStreamBuff::SetCurrPos(streamsize_t pos)
{ 
	m_Curr = m_Data.begin() + pos;
}

/********** MemoOutStreamBuff Implementation **********/

void ThrowingMemoOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
	if (m_Curr + size > m_Data.end())
	{
		throwEndsException("MemoOutStreamBuff");
	}
	memcpy(m_Curr, data, size);
	m_Curr += size;
}

void SilentMemoOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
	dms_assert(m_Curr <= m_Data.end());
	if (m_Curr + size > m_Data.end())
		size = m_Data.end() - m_Curr;
	memcpy(m_Curr, data, size);
	m_Curr += size;
	dms_assert(m_Curr <= m_Data.end());
}

streamsize_t MemoOutStreamBuff::CurrPos() const
{
	return m_Curr - m_Data.begin();
}

/********** NullOutStreamBuff CODE **********/

NullOutStreamBuff::NullOutStreamBuff()
	: m_CurrPos(0) {}

void NullOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
	m_CurrPos += size;
}

streamsize_t NullOutStreamBuff::CurrPos() const
{
	return m_CurrPos;
}

/********** CheckEqualityOutStreamBuff Interface **********/


void CheckEqualityOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
	if (m_Status == match_status::partial)
	{
		assert(CurrPos() <= m_SourceData.size());
		SizeT cmpSize = Min<streamsize_t>(size, m_SourceData.size() - CurrPos());
		if (strncmp(m_SourceData.begin() + CurrPos(), data, cmpSize))
			m_Status = match_status::different;
		m_CurrPos += size;
		if (m_CurrPos > m_SourceData.size())
			m_Status = match_status::overfull;
	}
}


/********** ExternalVectorOutStreamBuff Implementation **********/

ExternalVectorOutStreamBuff::ExternalVectorOutStreamBuff(VectorType& data)
 : m_DataRef(data) {}

void ExternalVectorOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
	m_DataRef.insert(m_DataRef.end(), data, data+size);
}

streamsize_t ExternalVectorOutStreamBuff::CurrPos() const
{
	return m_DataRef.size();
}

/********** VectorOutStreamBuff Implementation **********/

VectorOutStreamBuff::VectorOutStreamBuff()
{}

void VectorOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
	m_Data.insert(m_Data.end(), data, data+size);
}

streamsize_t VectorOutStreamBuff::CurrPos() const
{
	return m_Data.size();
}

/********** CallbackOutStreamBuff Implementation **********/

CallbackOutStreamBuff::CallbackOutStreamBuff(ClientHandle clientHandle, CallbackStreamFuncType func)
	:	m_ClientHandle(clientHandle), m_Func(func), m_ByteCount(0)
{}

void CallbackOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{ 
	m_ByteCount += size;
	m_Func(m_ClientHandle, data, size); 
} 

streamsize_t CallbackOutStreamBuff::CurrPos() const
{
	return m_ByteCount;
}

//----------------------------------------------------------------------
// Creating and reading OutStreamBuff's Interface functions
//----------------------------------------------------------------------

#include "StreamBuffInterface.h"

RTC_CALL VectorOutStreamBuff*
DMS_CONV DMS_VectOutStreamBuff_Create()
{
	DMS_CALL_BEGIN
		return new VectorOutStreamBuff();
	DMS_CALL_END
	return nullptr;
}

RTC_CALL void 
DMS_CONV DMS_OutStreamBuff_Destroy(OutStreamBuff* self)
{
	delete self;
}

RTC_CALL void 
DMS_CONV DMS_OutStreamBuff_WriteBytes(OutStreamBuff* self, const Byte* source, streamsize_t sourceSize)
{
	DMS_CALL_BEGIN
		self->WriteBytes(source, sourceSize);
	DMS_CALL_END
}

RTC_CALL void 
DMS_CONV DMS_OutStreamBuff_WriteChars(OutStreamBuff* self, CharPtr source)
{
	DMS_CALL_BEGIN
		self->WriteBytes(source, StrLen(source));
	DMS_CALL_END
}

// note: apply the following functions only on OutStreamBuffs that have been created as VectOutstreamBuff
RTC_CALL CharPtr 
DMS_CONV DMS_VectOutStreamBuff_GetData( VectorOutStreamBuff* self)
{
	DMS_CALL_BEGIN
		dms_assert(self);
		return &*self->GetData();
	DMS_CALL_END
	return nullptr;
}

RTC_CALL streamsize_t 
DMS_CONV DMS_OutStreamBuff_CurrPos(OutStreamBuff* self)
{
	DMS_CALL_BEGIN
		dms_assert(self);
		return self->CurrPos();
	DMS_CALL_END
	return 0;
}


// ==== BaseStreamBuff ====

/*
 *  Name        : ser\BaseStreamBuff.cpp
 */

#include "ser/BaseStreamBuff.h"
#include "ser/FormattedStream.h"
#include "ser/StreamException.h"
#include "utl/StrFormat.h"
#include "vt/Conversions.h"

// *****************************************************************************
// Section:     streaming exceptions
// *****************************************************************************

[[noreturn]] void throwStreamException(CharPtr name, CharPtr msg)
{
	throwErrorF("Stream","Stream {} has exception '{}'", name, msg);
}

[[noreturn]] void throwEndsException(CharPtr name)
{ 
	throwStreamException(name, "unexpected end of stream");
}

// *****************************************************************************
// Section:     Basic streambuffer Interface
// *****************************************************************************


/********** InpStreamBuff CODE **********/

InpStreamBuff:: InpStreamBuff() {}
InpStreamBuff::~InpStreamBuff() {}

WeakStr InpStreamBuff::FileName() { return SharedStr(); }

CharPtr InpStreamBuff::GetDataBegin()
{
	throwIllegalAbstract(MG_POS, "InpStreamBuff::GetDataBegin()");
}

CharPtr InpStreamBuff::GetDataEnd()
{
	throwIllegalAbstract(MG_POS, "InpStreamBuff::GetDataEnd()");
}

void InpStreamBuff::SetCurrPos(streamsize_t pos)
{
	throwIllegalAbstract(MG_POS, "InpStreamBuff::SetCurrPos(streamsize_t pos)");
}

/********** OutStreamBuff CODE **********/

OutStreamBuff:: OutStreamBuff() {}
OutStreamBuff::~OutStreamBuff() {}
WeakStr OutStreamBuff::FileName() { return SharedStr(); }

void OutStreamBuff::WriteBytes(CharPtr cstr)
{
	WriteBytes(cstr, StrLen(cstr)); 
}



// ==== AsString ====

#include "ser/AsString.h"
#include "vt/iterrange.h"
#include "ptr/SharedStr.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"
#include "ser/StringStream.h"
#include "ser/SequenceArrayStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/StrFormat.h"
#include "utl/Quotes.h"


//----------------------------------------------------------------------
// helper funcs
//----------------------------------------------------------------------

void StringRef_resize_uninitialized(StringRef& res, SizeT n)
{
	res.resize_uninitialized(n MG_DEBUG_ALLOCATOR_SRC("StringRef_resize_uninitialized"));
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
			res.assign(charBuf, charBuf + n MG_DEBUG_ALLOCATOR_SRC("AsString"));
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
		? SharedStr(CharPtrRange(begin_ptr(v), end_ptr(v)))
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

