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
#include "geo/Color.h"
#include "geo/Conversions.h"
#include "geo/SequenceArray.h"
#include "ser/BinaryStream.h"
#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "ser/StringStream.h"
#include "ser/AsString.h"
#include "set/Token.h"
#include "utl/mySPrintF.h"

#include "PropDefInterface.h"

//----------------------------------------------------------------------
// declarations from StringBounds.h
//----------------------------------------------------------------------

SizeT StrLen(CharPtr str)
{
	assert(str != nullptr);
	return strlen(str);
}

//----------------------------------------------------------------------
// declarations from AsString.h
//----------------------------------------------------------------------

SharedStr AsRgbStr(UInt32 v) 
{ 
	return mySSPrintF("rgb(%d,%d,%d)"
		,	GetRed  (v)
		,	GetGreen(v)
		,	GetBlue (v)
		);
}


//----------------------------------------------------------------------
// Section      : IString, used for returning string-handles to ClientAppl
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

namespace {

	typedef std::set<CharPtr> IStringSet;
	static_ptr<IStringSet> s_AllocatedStrings;
	UInt32                 s_nrIStringComponentLocks = 0;
	void IStringCheck(const IString* p)
	{
		dms_assert(s_AllocatedStrings && s_AllocatedStrings->find(p->c_str()) != s_AllocatedStrings->end());
	}

}	//	anonymous namespace

IStringComponentLock::IStringComponentLock()
{
	if (!s_nrIStringComponentLocks++)
		s_AllocatedStrings.assign( new IStringSet );
}

IStringComponentLock::~IStringComponentLock()
{
	dms_assert(s_AllocatedStrings);
	if (--s_nrIStringComponentLocks)
		return;

	std::size_t n = s_AllocatedStrings->size();
	if (n)
	{
		reportF(SeverityTypeID::ST_MajorTrace, "Memory Leak of %I64u IStrings", (UInt64)n);

		IStringSet::iterator i = s_AllocatedStrings->begin();
		IStringSet::iterator e = s_AllocatedStrings->end();
		while (i!=e)
			reportD(SeverityTypeID::ST_MajorTrace, "MemoLeak: IString ", (*i));
	}
	s_AllocatedStrings.reset();
}

#endif


//----------------------------------------------------------------------
// Section      : IString, used for returning string-handles to ClientAppl
//----------------------------------------------------------------------

static_assert(sizeof(IStringHandle) == sizeof(char*));
static_assert(sizeof(IString) == sizeof(char*));


const IString* IString::Create(CharPtr strVal) { return new IString(strVal); }
const IString* IString::Create(CharPtr strBegin, CharPtr strEnd) { return new IString(strBegin, strEnd); }

const IString* IString::Create(WeakStr strVal) { return new IString(strVal); }
const IString* IString::Create(TokenID strVal) { return new IString(strVal); }

IString::IString(CharPtr strVal)
	:	SharedStr(strVal) 
{
	Init();
}

IString::IString(CharPtr strBegin, CharPtr strEnd)
	:	SharedStr(strBegin, strEnd) 
{
	Init();
}

IString::IString(WeakStr strVal)
	:	SharedStr(strVal) 
{
	Init();
}

IString::IString(TokenID strVal)
	:	SharedStr(strVal) 
{
	Init();
}

IString::~IString()
{ 
	MG_DEBUGCODE( 
		IStringCheck(this);
		s_AllocatedStrings->erase(c_str()); 
	) 
}

void IString::Release(Handle self)
{
	dms_check_not_debugonly; 
	MG_DEBUGCODE( IStringCheck(self); )
	delete self;
}

CharPtr IString::AsCharPtr(Handle self)
{
	MG_DEBUGCODE( IStringCheck(self); )
	return self->c_str();
}

void IString::Init()
{
	MG_DEBUGCODE( 
		s_AllocatedStrings->insert(c_str());
	)
}


MG_DEBUGCODE(SharedStr sd_strCopy1; )
MG_DEBUGCODE(SharedStr sd_strCopy2; )

RTC_CALL CharPtr DMS_CONV DMS_IString_AsCharPtr(IStringHandle ptrString)
{
	return IString::AsCharPtr(ptrString);
}
 
RTC_CALL void    DMS_CONV DMS_IString_Release(IStringHandle ptrString)
{
	DMS_CALL_BEGIN

		dms_assert(ptrString);
		IString::Release(ptrString);

	DMS_CALL_END
}


//----------------------------------------------------------------------
// Section      : Binary serialization of strings
//----------------------------------------------------------------------

BinaryOutStream& operator <<(BinaryOutStream& ar, WeakStr str) 
{
	UInt32 len = str.ssize();
	ar << len;
	if (len)
		ar.Buffer().WriteBytes(str.begin(), len);
	return ar;
}

BinaryInpStream& operator >>(BinaryInpStream& ar, SharedStr& str) 
{
	UInt32 len;
	ar >> len;
	if (len)
	{
		SharedCharArray* sca =SharedCharArray::CreateUninitialized(SizeT(len)+1); sca->back() = char(0);
		str.assign(sca);
		ar.Buffer().ReadBytes(sca->begin(), len);
	}
	else
		str.clear();
	return ar;
}

//----------------------------------------------------------------------
// Section      : Serialization support for SharedStr
//----------------------------------------------------------------------

#include "geo/IterRange.h"

FormattedInpStream& operator >> (FormattedInpStream& is, SharedStr& str)
{
	CharPtrRange result = is.NextWord();
	str.assign (SharedCharArray_Create(result.begin(), result.end()));
	return is;
}

//----------------------------------------------------------------------
// Section      : Serialization support for TokenID
//----------------------------------------------------------------------

FormattedOutStream& operator <<(FormattedOutStream& str, TokenID value)
{
	if (IsDefined(value))
		str << value.GetStr().c_str();
	else
		str << Undefined();
	return str;
}

FormattedInpStream& operator >>(FormattedInpStream& str, TokenID& value)
{
	auto result = str.NextToken();
	value = GetTokenID<mt_tag>(result.second);
	return str;
}

//----------------------------------------------------------------------
// Section      : Various conversions
//----------------------------------------------------------------------

Float64 AsFloat64(WeakStr x )
{
	if (!x.IsDefined())
		return UNDEFINED_VALUE(Float64);
	MemoInpStreamBuff mis(x.begin(), x.send());
	FormattedInpStream fin(&mis);
	Float64 r; 
	fin >> r; // DEBUG: step in to see if end of InpStream is taken care of (for an empty string)
	return r; 
}

Float64 AsFloat64(CharPtr x )
{ 
	Float64 r; 
	MemoInpStreamBuff mis(x);
	FormattedInpStream fin(&mis);
	fin >> r;
	return r; 
}

Float64 AsFloat64(TokenID x )
{ 
	Float64 r; 
	MemoInpStreamBuff mis(x.GetStr().c_str(), x.GetStrEnd().c_str());
	FormattedInpStream fin(&mis);
	fin >> r;
	return r; 
}

//----------------------------------------------------------------------
// Section      : various StringRef functions
//----------------------------------------------------------------------

RTC_CALL SharedStr AsDataStr(const StringCRef& v)
{
	SharedStr result;
	SingleQuote(
		result,
		begin_ptr( v ), 
		end_ptr  ( v)
	);
	return result;
}

RTC_CALL void WriteDataString(FormattedOutStream& out, const StringCRef& v)
{
	SingleQuote(out, begin_ptr( v ), end_ptr( v ) ); 
}

char NibbleAsHex(unsigned char v)
{
	dms_assert(v < 16);
	return (v<10) ? ('0'+v) : (('A'-10)+v);
}

RTC_CALL void AsHex(StringRef& res, UInt4 v) 
{ 
	res.resize_uninitialized(1);
	res[0] = NibbleAsHex(v);
}

RTC_CALL void AsHex(StringRef& res, UInt8 v)
{
	res.resize_uninitialized(2);
	res[1] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[0] = NibbleAsHex(v);
}

RTC_CALL void AsHex(StringRef& res, UInt16 v)
{
	res.resize_uninitialized(4);
	res[3] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[2] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[1] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[0] = NibbleAsHex(v);
}

RTC_CALL void AsHex(StringRef& res, UInt32 v)
{
	res.resize_uninitialized(8);
	res[7] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[6] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[5] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[4] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[3] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[2] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[1] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[0] = NibbleAsHex(v);
}

RTC_CALL void AsHex(StringRef& res, UInt64 v)
{
	res.resize_uninitialized(16);
	res[15] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[14] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[13] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[12] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[11] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[10] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 9] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 8] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 7] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 6] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 5] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 4] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 3] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 2] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 1] = NibbleAsHex(v & 0x000F); v >>= 4;
	res[ 0] = NibbleAsHex(v);
}

RTC_CALL void AsHex(StringRef& res, StringCRef v)
{ 
	StringCRef::size_type vSize = v.size();
	res.reserve(vSize * 2);
	res.resize_uninitialized(0);
	for (StringCRef::size_type p=0; p!=vSize; ++p)
	{
		unsigned char ch = v[p];
		res.push_back(NibbleAsHex( ch >>   4 ) );
		res.push_back(NibbleAsHex( ch & 0x0F ) );
	}
}
