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
	InpStreamBuff();
	RTC_CALL virtual ~InpStreamBuff(); // exported: stg/stx MemoInpStreamBuff dtor needs it in Debug links (/OPT:REF strips the reference in Release)
	RTC_CALL virtual void ReadBytes (BytePtr data, streamsize_t size) const=0;
	RTC_CALL virtual streamsize_t CurrPos() const=0;
	RTC_CALL virtual bool   AtEnd  () const=0;
	virtual WeakStr FileName();

	virtual CharPtr GetDataBegin(); 
	virtual CharPtr GetDataEnd(); 
	virtual void    SetCurrPos(streamsize_t pos); 
};

class OutStreamBuff
{
public:
	RTC_CALL OutStreamBuff();
	RTC_CALL virtual ~OutStreamBuff();
	// #1227: streamed into from under the DebugOutStream lock and from the error-reporting
	// ceiling (every Describe writes through here). An implementation may buffer, count, hash or
	// do foreign I/O, and it may THROW -- ThrowingMemoOutStreamBuff and the CompoundStorage buff
	// do, and building that DmsException reads names, registry-shared -- but it may take nothing
	// outer than the token registry.
	RTC_CALL virtual void WriteBytes(CBytePtr data, streamsize_t size) DMS_CALLEE_ENTERS(ord_level_type::IndexedString, dms_shared_v) =0;
	RTC_CALL virtual streamsize_t CurrPos() const=0;
	RTC_CALL virtual WeakStr FileName();
	RTC_CALL virtual bool    AtEnd() const = 0;
	void WriteByte(Byte data) { WriteBytes(&data, 1); }
	RTC_CALL void WriteBytes(CharPtr cstr);
};

#endif // __RTC_SER_BASESTREAMBUFF
