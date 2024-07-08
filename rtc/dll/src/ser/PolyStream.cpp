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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "RtcInterface.h"

#include "ser/PolyStream.h"
#include "mci/Object.h"
#include "mci/Class.h"
#include "mci/PropDef.h"
#include "ptr/PersistentSharedObj.h"
#include "ser/StringStream.h"
#include "utl/mySPrintF.h"

#include "RtcVersion.h"

// *****************************************************************************
// Section:     Persistent Streaming Interface
// *****************************************************************************

const UInt32 UInt32_ClsNrLimit   = 0x80000000;
const UInt32 UInt32_ObjNrLimit   = 0x80000000;
const UInt32 UInt32_TokenIdLimit = 0x80000000;

// *****************************************************************************
// Class:     PolymorphOutStream
// *****************************************************************************

PolymorphOutStream::PolymorphOutStream(OutStreamBuff* buff)
	:	BinaryOutStream(buff)
{
	(*this) 
		<<	UInt16(DMS_VERSION_MAJOR)
		<<	UInt16(DMS_VERSION_MINOR)
		<<	UInt8 (DMS_CURR_POLYSTREAM_FORMAT)
		<<	UInt16(DMS_VERSION_MAJOR_BACKWARD)
		<<	UInt16(DMS_VERSION_MINOR_BACKWARD);
}

PolymorphInpStream::PolymorphInpStream(InpStreamBuff* inp)
	:	BinaryInpStream(inp)
	,	m_StrnBufSize(0)
{
	UInt16 versionMajor, versionMajorBW;
	UInt16 versionMinor, versionMinorBW;
	(*this) >> versionMajor >> versionMinor >> m_FormatID;

	bool tooOld = (versionMajor < DMS_VERSION_MAJOR_COMPATIBLE) || (versionMajor == DMS_VERSION_MAJOR_COMPATIBLE && versionMinor < DMS_VERSION_MINOR_COMPATIBLE) || (m_FormatID < DMS_MINR_POLYSTREAM_FORMAT);
	bool tooNew = false;
	if (m_FormatID < 3)
	{
		versionMajorBW = versionMajor;
		versionMinorBW = versionMinor;
	}
	else
	{
		(*this) >> versionMajorBW >> versionMinorBW;
	}
	tooNew = (versionMajorBW > DMS_VERSION_MAJOR)  || (versionMajorBW == DMS_VERSION_MAJOR && versionMinorBW > DMS_VERSION_MINOR) || (m_FormatID > DMS_CURR_POLYSTREAM_FORMAT);
	constexpr bool majorBackwardReadSupport = (DMS_VERSION_MAJOR_COMPATIBLE < DMS_VERSION_MAJOR);
	constexpr bool minorBackwardReadSupport = (DMS_VERSION_MINOR_COMPATIBLE < DMS_VERSION_MINOR);
	constexpr bool hasBackwardReadSupport  = majorBackwardReadSupport || minorBackwardReadSupport;

	constexpr bool majorBackwardWriteSupport = (DMS_VERSION_MAJOR_BACKWARD < DMS_VERSION_MAJOR);
	constexpr bool minorBackwardWriteSupport = (DMS_VERSION_MINOR_BACKWARD < DMS_VERSION_MINOR);
	constexpr bool hasBackwardWriteSupport = majorBackwardWriteSupport || minorBackwardWriteSupport;

	if (tooOld || tooNew)
	{
		CharPtr strProblem
			=	tooOld
					?	"internal data format has changed"
					:	"this software cannot determine if the data from the newer version is compatible";
		CharPtr strSolution
			=	tooOld 
					?	"Remove all binary data files (such as *.CFS files and the CalcCache contents)\n"
						"that were created with the old DMS version and recalculate the CalcCache contents"
					:	"Try to locate and start the newer version of the GeoDMS with which these binary data files\n"
						"have been created or remove the CalcCache contents";

		throwErrorF("Serialization", 
			"Attempt to read binary configuration data from PolymorphStream %s that was written with GeoDMS version %d.%03d%s\n"
			"Now running: %s which can read binary configuration data written from version %d.%03d %s%s\n"
			"Problem: %s\n"
			"Solution: %s\n",

			inp->FileName().c_str(),  int(versionMajor), int(versionMinor), 
			(versionMajor != versionMajorBW || versionMinor != versionMinorBW)
				?	mySSPrintF(" which can write for version %d.%03d and later", int(versionMajorBW), int(versionMinorBW)).c_str() 
				:	"",
			DMS_GetVersion(), int(DMS_VERSION_MAJOR_COMPATIBLE), int(DMS_VERSION_MINOR_COMPATIBLE), 
			hasBackwardReadSupport
				?	"(and later versions)" 
				:	"",
			hasBackwardWriteSupport
				?	mySSPrintF(" and can write for version %d.%03d and later", int(DMS_VERSION_MAJOR_BACKWARD), int(DMS_VERSION_MINOR_BACKWARD)).c_str()
				:	"",
			strProblem,
			strSolution
		);
	}
	dms_assert(m_FormatID >= DMS_MINR_POLYSTREAM_FORMAT); // since older formats have obsolete versions(4.66 -> 2)
	dms_assert(m_FormatID <= DMS_CURR_POLYSTREAM_FORMAT); // since newer formats have newer versions   (5.23 -> 2)
}

void PolymorphOutStream::WriteCls(const Class* cls)
{
	dms_assert(cls != NULL);
	UInt32& clsId = m_ClsIds[cls];

	dms_assert(clsId < UInt32_ClsNrLimit);

	WriteInt32(~clsId);

	if (clsId == 0)
	{
		clsId = ++m_ClsCount;
		WriteToken(cls->GetID());
	}
	dms_assert(m_ClsIds.size() == m_ClsCount);
}

void PolymorphOutStream::WriteObj(const Object* obj)
{
	if (!obj)
		WriteInt32(0); // NIL indicator
	else
	{
		UInt32& objId = m_ObjIds[obj];
		if (objId)
		{
			dms_assert(objId < UInt32_ObjNrLimit);
			WriteInt32(objId);
		}
		else
		{
			objId = ++ m_ObjCount;
			WriteCls(obj->GetDynamicClass());
			obj->WriteObj(*this);
		}
		dms_assert(m_ObjIds.size() == m_ObjCount);// size is not officially a const-time member of std::map
	}
}

Object* Class::CreateObj(PolymorphInpStream* istr) const
{
	Object* obj = CreateObj();
	istr->m_ObjReg.push_back(obj);

	obj->ReadObj(*istr);

	return obj;
}

Object* PolymorphInpStream::ReadObj()
{
	UInt32 objId = ReadInt32();
	if (!objId) 
		return nullptr;

	if (objId < UInt32_ObjNrLimit) // real objId
	{
		dms_assert(objId <= m_ObjReg.size());
		return m_ObjReg[ objId-1 ];
	}
	
	// class id is negated version of a negiative objId.
	dms_assert((~objId) < UInt32_ClsNrLimit); 
	const Class* cls = ReadCls(~objId);
	dms_assert(cls);

	#if defined(MG_DEBUG)

		SizeT n = m_ObjReg.size();
		Object* obj = cls->CreateObj(this);
		dms_assert(m_ObjReg.size() > n && m_ObjReg[n] == obj);
		return obj;

	#else

		return cls->CreateObj(this);

	#endif
}

void PolymorphOutStream::WriteToken(TokenID ID)
{
	UInt32& tknId = m_TknIds[ID];
	if (tknId)
		WriteInt32(tknId);
	else
	{
		tknId = ++ m_TknCount;
		UInt32 strLen = GetTokenStrLen(ID);

		TokenStr prevTokenPtr = GetTokenStr(m_PrevTokenID);
		TokenStr thisTokenStr = GetTokenStr(ID);
		CharPtr  thisTokenPtr = thisTokenStr.c_str();

		while (prevTokenPtr.c_str()[0] && prevTokenPtr.c_str()[0] == *thisTokenPtr)
		{
			dms_assert(*thisTokenPtr);
			++thisTokenPtr;
			++prevTokenPtr;
		}
		UInt32 nrSame = thisTokenPtr - thisTokenStr.c_str();
		strLen -= nrSame;

		WriteInt32(~strLen);
		WriteUInt32(nrSame);
		Buffer().WriteBytes(thisTokenPtr, strLen);
		m_PrevTokenID = ID;
	}
	dms_assert(m_TknIds.size() == m_TknCount);
}

TokenID PolymorphInpStream::GetRegToken(UInt32 extTknId) const
{
	MG_CHECK(extTknId <= m_TknReg.size());
	return m_TknReg[ extTknId-1 ];
}

TokenID PolymorphInpStream::ReadToken()
{
	const char* tokenStrBuf;

	UInt32 extTknId = ReadInt32();
	dms_assert(extTknId);
	if (extTknId < UInt32_TokenIdLimit)
		return GetRegToken(extTknId);

	UInt32 nrSame = ReadUInt32();
	UInt32 nrDiff = ~extTknId;
	UInt32 strLen = nrSame + nrDiff;

	m_TokenStrBuf.resize(strLen);
	if (strLen) {
		char* strBuf = &*m_TokenStrBuf.begin();
		Buffer().ReadBytes(strBuf + nrSame, nrDiff);
		tokenStrBuf = strBuf;
	}
	else
		tokenStrBuf = "";

	TokenID tknId = GetTokenID_mt(tokenStrBuf, tokenStrBuf + strLen);
	m_TknReg.push_back(tknId);
	return tknId;
}

namespace { 
	const Int8  Int8_2ByteCode      = -128; // 0x80
	const Int8  Int8_4ByteCode      = -127; // 0x81
	const Int8  Int8_First1ByteValue= -126; // 0x82
	const Int16 Int16_First2ByteValue= Int16(0x8000);
}

void PolymorphOutStream::WriteInt32 (Int32 v)
{
	if (v < 0)
	{
		if (v >= Int8_First1ByteValue )
			*this << Int8(v);
		else if (v >= Int16_First2ByteValue)
			*this << Int8_2ByteCode << Int16(v);
		else 
			*this << Int8_4ByteCode << Int32(v);
	}
	else
	{
		if (v < 0x80)
			*this << Int8(v);
		else if (v < 0x8000)
			*this << Int8_2ByteCode << Int16(v);
		else 
			*this << Int8_4ByteCode << Int32(v);
	}
}

Int32 PolymorphInpStream::ReadInt32()
{
	Int8 v;
	// use of integer compression
	*this >> v;

	switch(v) {
		case Int8_2ByteCode:
		{
			Int16 vv;
			*this >> vv;
			return vv;
		}
		case Int8_4ByteCode:
		{
			Int32 vvvv;
			*this >> vvvv;
			return vvvv;
		}
	}
	return v;
}


namespace { 
	const UInt8  UInt8_2ByteCode       = 0xFE;
	const UInt8  UInt8_4ByteCode       = 0xFF;
	const UInt8  UInt8_Last1ByteValue  = 0xFD;
	const UInt16 UInt16_Last2ByteValue = UInt16(0xFFFF);
}
void PolymorphOutStream::WriteUInt32(UInt32 v)
{
	if (v <= UInt8_Last1ByteValue)
		*this << UInt8(v);
	else if (v <= UInt16_Last2ByteValue)
		*this << UInt8_2ByteCode << UInt16(v);
	else 
		*this << UInt8_4ByteCode << UInt32(v);
}

UInt32 PolymorphInpStream::ReadUInt32()
{
	UInt8 v;
	// use of integer compression
	*this >> v;

	switch(v) {
		case UInt8_2ByteCode:
			{
				UInt16 vv;
				*this >> vv;
				return vv;
			}
		case UInt8_4ByteCode:
			{
				UInt32 vvvv;
				*this >> vvvv;
				return vvvv;
			}
	}
	return v;
}

// *****************************************************************************
// Class:     PolymorphInpStream
// *****************************************************************************

const Class*  PolymorphInpStream::ReadCls(UInt32 clsId)
{
	if (clsId > 0)
	{
		dms_assert(clsId <= m_ClsReg.size());
		return m_ClsReg[ clsId-1 ];
	}
	TokenID clsID = ReadToken();
	const Class* cls = Class::Find(clsID);
	if (!cls)
		throwErrorF("Serialization", "PolymorphInpStream::ReadCls(): Unknown class '%s' read", GetTokenStr(clsID) );
	m_ClsReg.push_back(cls);
	return cls;
}

