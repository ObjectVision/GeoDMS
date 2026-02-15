// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_SER_BASESTREAMBUFF)
#define __RTC_SER_BASESTREAMBUFF

using BytePtr = Byte*;
using CBytePtr = const Byte*;

// *****************************************************************************
// Section:     Basic streambuffer Interface
// *****************************************************************************

class InpStreamBuff
{
public:
	RTC_CALL InpStreamBuff();
	RTC_CALL virtual ~InpStreamBuff();
	RTC_CALL virtual void ReadBytes (BytePtr data, streamsize_t size) const=0;
	RTC_CALL virtual streamsize_t CurrPos() const=0;
	RTC_CALL virtual bool   AtEnd  () const=0;
	RTC_CALL virtual WeakStr FileName();

	RTC_CALL virtual CharPtr GetDataBegin(); 
	RTC_CALL virtual CharPtr GetDataEnd(); 
	RTC_CALL virtual void    SetCurrPos(streamsize_t pos); 
};

class OutStreamBuff
{
public:
	RTC_CALL OutStreamBuff();
	RTC_CALL virtual ~OutStreamBuff();
	RTC_CALL virtual void WriteBytes(CBytePtr data, streamsize_t size) =0;
	RTC_CALL virtual streamsize_t CurrPos() const=0;
	RTC_CALL virtual WeakStr FileName();
	RTC_CALL virtual bool    AtEnd() const = 0;
	void WriteByte(Byte data) { WriteBytes(&data, 1); }
	RTC_CALL void WriteBytes(CharPtr cstr);
};

#endif // __RTC_SER_BASESTREAMBUFF
