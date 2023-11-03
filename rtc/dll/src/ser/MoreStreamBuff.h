// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

/*
 *  Name        : ser/MoreStreamBuff.h
 *  Description : provides streaming, rtti, serialisation
 */

#ifndef __SER_MORESTREAMBUFF_H
#define __SER_MORESTREAMBUFF_H

#include "dbg/Check.h"
#include "ptr/IterCast.h"
#include "ser/BaseStreamBuff.h"
#include "geo/iterrange.h"

using ByteRange = IterRange<BytePtr>;
using CByteRange = IterRange<CBytePtr>;

/********** MemoInpStreamBuff Interface **********/

class MemoInpStreamBuff : public InpStreamBuff
{
public:
	RTC_CALL MemoInpStreamBuff(CBytePtr begin, CBytePtr end = nullptr);
	RTC_CALL void ReadBytes (Byte* data, streamsize_t size) const override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool   AtEnd  () const override { dms_assert(m_Data.begin() <= m_Curr && m_Curr <= m_Data.end()); return m_Curr == m_Data.end(); }

	RTC_CALL virtual CharPtr GetDataBegin() override;
	RTC_CALL virtual CharPtr GetDataEnd()   override;
	RTC_CALL virtual void    SetCurrPos(streamsize_t pos) override;

private:
	CByteRange m_Data;
	// non const access required for read
	mutable CBytePtr m_Curr; 
};


/********** MemoOutStreamBuff Interface **********/

class MemoOutStreamBuff : public OutStreamBuff
{
public:
	MemoOutStreamBuff(ByteRange data)
		: m_Data(data)
		, m_Curr(data.begin())
	{}

	// override OutStreamBuff interface
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool   AtEnd() const override { dms_assert(m_Data.begin() <= m_Curr && m_Curr <= m_Data.end()); return m_Curr == m_Data.end(); }

	ByteRange m_Data;
	BytePtr m_Curr;
};

struct ThrowingMemoOutStreamBuff : MemoOutStreamBuff
{
	using MemoOutStreamBuff::MemoOutStreamBuff;
	RTC_CALL void WriteBytes(const Byte* data, streamsize_t size) override;
};

struct SilentMemoOutStreamBuff : MemoOutStreamBuff
{
	using MemoOutStreamBuff::MemoOutStreamBuff;
	RTC_CALL void WriteBytes(const Byte* data, streamsize_t size) override;
};

/********** NullOutStreamBuff Interface **********/

class NullOutStreamBuff : public OutStreamBuff
{
public:
	RTC_CALL NullOutStreamBuff();

	RTC_CALL void  WriteBytes(const Byte* data, streamsize_t size) override;
	RTC_CALL streamsize_t CurrPos() const override;

	streamsize_t m_CurrPos;
};

struct InfiniteNullOutStreamBuff : public NullOutStreamBuff
{
	RTC_CALL bool AtEnd() const override { return false; }
};

struct FiniteNullOutStreamBuff : public NullOutStreamBuff
{
	FiniteNullOutStreamBuff(streamsize_t maxLen) : m_MaxLength(maxLen) {}
	RTC_CALL bool AtEnd() const override { return m_CurrPos >= m_MaxLength; }
	streamsize_t m_MaxLength;
};


/********** CheckEqualityOutStreamBuff Interface **********/


enum class match_status { different, partial, full, overfull };

class CheckEqualityOutStreamBuff : public OutStreamBuff
{
public:
	CheckEqualityOutStreamBuff(CByteRange sourceData)
		: m_SourceData(sourceData)
	{}

	RTC_CALL void  WriteBytes(const Byte* data, streamsize_t size) override;

	auto CurrPos() const -> streamsize_t override { return m_CurrPos;  }
	bool AtEnd  () const                 override { return m_CurrPos == m_SourceData.size(); }

	auto GetStatus() const -> match_status
	{
		if (m_Status == match_status::partial && AtEnd())
			return match_status::full;
		return m_Status;
	}

private:
	CByteRange m_SourceData;
	streamsize_t m_CurrPos = 0;
	match_status m_Status = match_status::partial;
};


/********** ExternalVectorOutStreamBuff Interface **********/

#include <vector>

class ExternalVectorOutStreamBuff : public OutStreamBuff
{
public:
	using VectorType = vos_buffer_type;

	RTC_CALL ExternalVectorOutStreamBuff(VectorType& data);

	const Byte* GetData   () const { return begin_ptr(m_DataRef); }
	const Byte* GetDataEnd() const { return end_ptr  (m_DataRef); }

	RTC_CALL void  WriteBytes(const Byte* data, streamsize_t size) override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool AtEnd() const override { return false; }

private:
	VectorType& m_DataRef;
};

/********** VectorOutStreamBuff Interface **********/

class VectorOutStreamBuff : public OutStreamBuff
{
public:
	using VectorType = vos_buffer_type;

	RTC_CALL VectorOutStreamBuff();

	const Byte* GetData   () const { return begin_ptr(m_Data); }
	const Byte* GetDataEnd() const { return end_ptr  (m_Data); }

	RTC_CALL void WriteBytes(const Byte* data, streamsize_t size) override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool AtEnd() const override { return false; }

protected:
	VectorType m_Data;
};

/********** CallbackOutStreamBuff Interface **********/

class CallbackOutStreamBuff : public OutStreamBuff
{
public:

	RTC_CALL CallbackOutStreamBuff(ClientHandle clientHandle, CallbackStreamFuncType func);

	RTC_CALL void  WriteBytes(const Byte* data, streamsize_t size) override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool AtEnd() const override { return false; }

private:
	ClientHandle           m_ClientHandle;
	CallbackStreamFuncType m_Func;
	streamsize_t           m_ByteCount;
};


#endif // __SER_MORESTREAMBUFF_H
