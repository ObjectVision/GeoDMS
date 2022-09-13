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
 *  Name        : ser/MoreStreamBuff.h
 *  Description : provides streaming, rtti, serialisation
 */

#ifndef __SER_MORESTREAMBUFF_H
#define __SER_MORESTREAMBUFF_H

#include "dbg/Check.h"
#include "ptr/IterCast.h"
#include "ser/BaseStreamBuff.h"

/********** MemoInpStreamBuff Interface **********/

class MemoInpStreamBuff : public InpStreamBuff
{
public:
	RTC_CALL MemoInpStreamBuff(const Byte* begin, const Byte* end = 0);
	RTC_CALL void ReadBytes (Byte* data, streamsize_t size) const override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool   AtEnd  () const override { dms_assert(m_Begin <= m_Curr && m_Curr <= m_End); return m_Curr == m_End; }

	RTC_CALL virtual CharPtr GetDataBegin() override;
	RTC_CALL virtual CharPtr GetDataEnd()   override;
	RTC_CALL virtual void    SetCurrPos(streamsize_t pos) override;

private:
	const Byte* m_Begin;
	const Byte* m_End;
	// non const access required for read
	mutable const Byte* m_Curr; 
};


/********** MemoOutStreamBuff Interface **********/

class MemoOutStreamBuff : public OutStreamBuff
{
public:
	RTC_CALL MemoOutStreamBuff(Byte* begin, Byte* end);

	// override OutStreamBuff interface
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool AtEnd() const override { dms_assert(m_Begin <= m_Curr && m_Curr <= m_End); return m_Curr == m_End; }

	Byte* m_Begin;
	Byte* m_Curr;
	Byte* m_End;
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

/********** ExternalVectorOutStreamBuff Interface **********/

#include <vector>

class ExternalVectorOutStreamBuff : public OutStreamBuff
{
public:
	typedef std::vector<Byte> VectorType;

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
	using VectorType = std::vector<Byte>;

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
