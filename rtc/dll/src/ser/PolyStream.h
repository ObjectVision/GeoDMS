// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SER_POLYSTREAM_H)
#define __SER_POLYSTREAM_H

#include "geo/BaseBounds.h"
#include "ptr/OwningPtrArray.h"
#include "ser/BinaryStream.h"
#include "set/QuickContainers.h"
#include "set/Token.h"

#include <vector>

class Class;
class Object;

#define DMS_CURR_POLYSTREAM_FORMAT 3
#define DMS_MINR_POLYSTREAM_FORMAT 2

// Format history
//
// FORMAT 0: DMS 4.60, (DMS 4.61 internally used Format 0)
// FORMAT 1: DMS 4.61 and later
// FORMAT 2: DMS 4.66 and later

// *****************************************************************************
// Class:     PolymorphOutStream
// *****************************************************************************

struct PolymorphOutStream : BinaryOutStream
{
	using object_id_t = UInt32;
	using class_id_t = UInt32;
	using token_id_t = TokenT;

	RTC_CALL PolymorphOutStream(OutStreamBuff* out);

	RTC_CALL void WriteObj(const Object* obj);
	RTC_CALL void WriteToken(TokenID id);
	RTC_CALL void WriteInt32 (Int32 v);
	RTC_CALL void WriteUInt32(UInt32 v);

private:
	void WriteCls(const Class* cls);

private:

	object_id_t                          m_ObjCount = 0;
	class_id_t                           m_ClsCount = 0;
	TokenT                               m_TknCount = 0;
	std::map<const Object*, object_id_t> m_ObjIds;
	std::map<const Class*,  class_id_t>  m_ClsIds;
	std::map<TokenID,       token_id_t>  m_TknIds;
	TokenID                              m_PrevTokenID = TokenID::GetEmptyID();
};

// *****************************************************************************
// Class:     PolymorphInpStream
// *****************************************************************************

struct PolymorphInpStream : BinaryInpStream
{
	RTC_CALL PolymorphInpStream(InpStreamBuff* inp);

	RTC_CALL Object* ReadObj();
	RTC_CALL TokenID           ReadToken();
	RTC_CALL Int32             ReadInt32 ();
	RTC_CALL UInt32            ReadUInt32();

private:
	const Class* ReadCls(UInt32 clsId);
	TokenID      GetRegToken(UInt32 extTknId) const;

public:
	std::unique_ptr<Byte>          m_StrnBuf;
	UInt32                         m_StrnBufSize;
	std::vector<Object*> m_ObjReg;
	UInt8                          m_FormatID;

private:
	std::vector<Byte>              m_TokenStrBuf;
	std::vector<const Class*>      m_ClsReg;
	std::vector<TokenID>           m_TknReg;
};

//----------------------------------------------------------------------
// Serialization and rtti
//----------------------------------------------------------------------


inline PolymorphOutStream& operator <<(PolymorphOutStream& os, const Object* obj)
{
	os.WriteObj(obj);
	return os;
}

template <class T>
inline PolymorphInpStream& operator >>(PolymorphInpStream& is, const T*& ptr)
{
	ptr = checked_cast<const T*>(is.ReadObj()); // throws an exception
	return is;
}

template <typename Ptr> inline
PolymorphInpStream& operator >>(PolymorphInpStream& ar, SharedPtrWrap<Ptr>& rPtr)
{
	rPtr.assign(checked_cast<typename Ptr::pointer>(ar.ReadObj())); // increments counter of referrred object when read of linked; throws exception when dynamic_cast fails
	return ar;
}


#endif // __SER_POLYSTREAM_H
