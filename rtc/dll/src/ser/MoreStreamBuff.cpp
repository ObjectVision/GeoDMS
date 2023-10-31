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
#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "dbg/DmsCatch.h"
#include "geo/StringBounds.h"
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
