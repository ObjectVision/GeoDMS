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
#pragma hdrstop

#include <stdio.h>
#include <ctype.h>

#include "geo/Conversions.h"
#include "geo/IterRange.h"
#include "geo/StringBounds.h"
#include "geo/Undefined.h"
#include "ser/FormattedStream.h"
#include "ser/ReadValue.h"
#include "ser/StringStream.h"
#include "set/RangeFuncs.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"


// *****************************************************************************
// Section:     FormattedOutStream
// *****************************************************************************

FormattedOutStream::FormattedOutStream(OutStreamBuff* out, FormattingFlags ff)
	: m_OutStreamBuff(out)
	, m_FormattingFlags(ff)
{
	if (HasThousandSeparator(m_FormattingFlags))
		if (!ShowThousandSeparator())
			reinterpret_cast<UInt32&>(m_FormattingFlags) &= ~UInt32(FormattingFlags::ThousandSeparator);
	MG_PRECONDITION(out);
}

FormattedOutStream& operator <<(FormattedOutStream& str, const Undefined&) 
{ 
	str.Buffer().WriteBytes(UNDEFINED_VALUE_STRING, UNDEFINED_VALUE_STRING_LEN);
	return str;
}

// *****************************************************************************
// Section:     FormattedInpStream
// *****************************************************************************

FormattedInpStream::FormattedInpStream()
	:	m_InpStreamBuff(0) 
	,	m_NextChar(0)
	,	m_LineNr(0)
	,	m_LineStartPos(0)
	,	m_AtEnd(true)
	,	m_Flags()
#if defined(MG_DEBUG)
	,	md_LastPos()
#endif
{}

FormattedInpStream::FormattedInpStream(InpStreamBuff* inp, reader_flags rf) 
	:	m_InpStreamBuff(inp)
	,	m_NextChar(0)
	,	m_LineNr(1)
	,	m_LineStartPos(inp->CurrPos())
	,	m_AtEnd(false)
	,	m_Flags(rf)
{
	MG_PRECONDITION(inp);
	ReadChar();
}

char FormattedInpStream::ReadChar() 
{
	if (!m_AtEnd)
	{
		assert(m_InpStreamBuff);
		assert(md_LastPos == m_InpStreamBuff->CurrPos());

		if (m_NextChar == '\n')
		{
			++m_LineNr;
			m_LineStartPos = m_InpStreamBuff->CurrPos();
		}
		if (m_InpStreamBuff->AtEnd())
		{
			m_AtEnd = true;
			m_NextChar = 0;
		}
		else
		{
			m_InpStreamBuff->ReadBytes(&m_NextChar, 1); 
			MG_DEBUGCODE(md_LastPos = m_InpStreamBuff->CurrPos());
		}

	}
	return m_NextChar; 
}

bool operator ==(const FormattedInpStream& a, const FormattedInpStream& b)
{
	if (a.AtEnd())
		return b.AtEnd();
	if (b.AtEnd())
		return false;
	dms_assert(&(const_cast<FormattedInpStream&>(a).Buffer()) == &(const_cast<FormattedInpStream&>(b).Buffer())); // only compare compatible forward iterators
	return const_cast<FormattedInpStream&>(a).Buffer().CurrPos() == const_cast<FormattedInpStream&>(b).Buffer().CurrPos();
}

void FormattedInpStream::SetCurrPos(streamsize_t pos)
{
	Buffer().SetCurrPos(pos); 
	MG_DEBUGCODE( md_LastPos = pos; )
	ReadChar();
}



#include <vector>

bool FormattedInpStream::IsFieldSeparator(char ch)
{
	if (ch == ';')
		return true;
	if (ch == '\t')
		return !(m_Flags & reader_flags::stringsWithTabs);
	if (ch == ',')
		return !(m_Flags & reader_flags::commaAsDecimalSeparator);

	return (ch == '\r' || ch == '\n');
}

bool FormattedInpStream::ReadComment()
{
	dms_assert(NextChar() == '/');
	unsigned char nextChar = ReadChar(); // nextChar is one after '/'
	if (nextChar=='*')
	{
		nextChar = ReadChar(); // nextChar is one after '/*'
		while (!AtEnd())
		{
			if (nextChar=='*')
			{
				nextChar = ReadChar(); // nextChar is one after '*'
				if (nextChar=='/')
				{
					ReadChar(); // NextChar is one after '*/'
					break;
				}
			}
			else
				nextChar = ReadChar();
		}
		return true;;
	} else if (nextChar=='/')
	{
		nextChar = ReadChar(); // nextChar is one after '//'
		while (!AtEnd())
		{
			if (nextChar == '\n')
				break;
			nextChar = ReadChar();
		}
		return true;
	}
	return false;
}

std::pair<FormattedInpStream::TokenType, CharPtrRange> 
FormattedInpStream::NextToken()
{
	TokenType tokenType = TokenType::Unknown;

	m_NextToken.clear();

redo:
	SkipSpace(*this);
	UChar nextChar = NextChar(); 

	if (nextChar == 0)
		tokenType = TokenType::EOF_;
	else if (nextChar=='/')
	{
		if (ReadComment())
			goto redo;
		tokenType = TokenType::Punctuation;
		m_NextToken.push_back('/'); // single char token
	}
	else if (isdigit(nextChar) || (nextChar == '-' && !(m_Flags & reader_flags::noNegatives) ))
	{
		tokenType = TokenType::Number;
		if (nextChar == '-')
		{
			m_NextToken.push_back(nextChar);
			nextChar = ReadChar();
		}
		while (isdigit(nextChar)) 
		{
			m_NextToken.push_back(nextChar);
			nextChar = ReadChar();
		}
		if (nextChar == '.' && !(m_Flags & reader_flags::commaAsDecimalSeparator)
			|| (nextChar == ',' && (m_Flags & reader_flags::commaAsDecimalSeparator)))
		{
			m_NextToken.push_back(nextChar);
			nextChar = ReadChar();
			while (isdigit(nextChar))
			{
				m_NextToken.push_back(nextChar);
				nextChar = ReadChar();
			}
		}
		if ((nextChar == 'E' || nextChar == 'e') && !(m_Flags & reader_flags::noScientific))
		{
			m_NextToken.push_back(nextChar);
			nextChar = ReadChar();
			if (nextChar == '+' || nextChar == '-')
			{
				m_NextToken.push_back(nextChar);
				nextChar = ReadChar();
			}
			while (isdigit(nextChar))
			{
				m_NextToken.push_back(nextChar);
				nextChar = ReadChar();
			}
		}

		// ensure token terminations for sscanf stuff
		m_NextToken.push_back(0);
		m_NextToken.pop_back();
	}
	else if (isalpha(nextChar) ||  nextChar=='_')
	{
		tokenType = TokenType::Word;
		while (isalnum(nextChar)||nextChar=='_')
		{
			m_NextToken.push_back(nextChar);
			nextChar = ReadChar();
		}
	}
	else if (nextChar == '\"')
	{
		tokenType = TokenType::DoubleQuote;
		if (!ReadDQuote())
			goto labelEOF;
	}
	else if (nextChar == '\'')
	{
		tokenType = TokenType::SingleQuote;
		if (!ReadSQuote())
			goto labelEOF;
	}
	else if (nextChar == EOF)
	{
labelEOF:
		tokenType = TokenType::EOF_;
		m_NextToken.clear();
		m_NextToken.push_back(EOF); // single char token
	}
	else if (nextChar != 0)
	{
		tokenType = TokenType::Punctuation;
		m_NextToken.push_back(nextChar); // single char token
		ReadChar(); // Read past token;
	}

	dms_assert(tokenType != TokenType::Unknown);
	if (m_NextToken.empty())
		return { tokenType, CharPtrRange("") };
	return { tokenType, CharPtrRange(begin_ptr(m_NextToken), end_ptr(m_NextToken)) };
}

bool FormattedInpStream::ReadDQuote()
{
	while (true) 
	{
		char nextChar = ReadChar();              // Read past opening delimiter or next char
		if(nextChar == '\"') break;
		if(nextChar == EOF) 
			return false;
		if(nextChar == 0)   break;
		if(nextChar == '\\') 
		{
			nextChar = ReadChar();               // skip escape character
			if(nextChar == EOF) 
				return false;
			if(nextChar == 0)   break;
			else if(nextChar == 'n') nextChar = '\n';
			else if(nextChar == 't') nextChar = '\t';
			else if(nextChar == 'r') nextChar = '\r';
			else if(nextChar == '0') nextChar = '\0';
		}
		m_NextToken.push_back(nextChar);
	}
	dms_assert(NextChar() == '\"' || NextChar() == 0);
	ReadChar();        // Read past closing delimiter
	return true;
}

bool FormattedInpStream::ReadSQuote()
{
	while (true)
	{
		unsigned char nextChar = ReadChar();      // Read past opening delimiter
		if(nextChar == EOF)
			return false;
		if(nextChar == 0) 
		{
			ReadChar();  // Read past closing delimiter
			break;
		}
		if(nextChar == '\'')    
		{
			nextChar = ReadChar();  // Read past closing delimiter
			if (nextChar != '\'')   // if double-delimiter in string then insert & continue
				break;              // else exit after having read the closing delimiter  
		}
		m_NextToken.push_back(nextChar);
	}
	return true;
}

CharPtrRange FormattedInpStream::NextWord()
{
	m_NextToken.clear();

	SkipSpace(*this);
	char nextChar = NextChar(); 

	if (nextChar != 0)
	{
		if (nextChar == '\"')
		{
			if (!ReadDQuote()) 
				goto clearAndReturnEOF;
		}
		else if (nextChar == '\'')
		{
			if (!ReadSQuote()) 
				goto clearAndReturnEOF;
		}
		else if (nextChar == EOF)
		{
			dms_assert(m_NextToken.empty());
			goto returnEOF; 
		}
		else
			
			while (nextChar 
				&& (nextChar != ' ' || (m_Flags & reader_flags::stringsWithSpaces))
				&& !IsFieldSeparator(nextChar))
			{
				m_NextToken.push_back(nextChar);
				nextChar = ReadChar();
			}
	}
	if (m_NextToken.empty())
		return CharPtrRange("");
returnNonEmptyWord:
	return CharPtrRange(begin_ptr(m_NextToken), end_ptr(m_NextToken) );
clearAndReturnEOF:
	m_NextToken.clear();
returnEOF:
	m_NextToken.push_back(EOF); // single char token
	goto returnNonEmptyWord;
}

/********** FormattedOutStream Interface **********/
#include <boost/locale.hpp>

template<typename U>
int mysnprintf(char* charBuf, UInt32 bufLen, CharPtr formatOut, U value, FormattingFlags ff)
{
	auto charCount = _snprintf(charBuf, bufLen, formatOut, value);
	dms_assert(UInt32(charCount) <= bufLen);
	return charCount;
}

int process_uint(char* charBuf, char* decPtr, UInt32 bufLen, int numCount)
{
	auto decPos = decPtr - charBuf;
	auto nrSeparators = (decPos - 1) / 3;
	if (nrSeparators) {
		dms_assert(numCount + nrSeparators <= bufLen);
		auto ptrSrc = decPtr - 3;
		auto ptrBeg = decPtr - 3 * nrSeparators; dms_assert(ptrBeg > charBuf);
		auto ptrDst = std::copy_backward(ptrSrc, charBuf + numCount, charBuf + numCount + nrSeparators);
		dms_assert(ptrDst >= ptrBeg); *--ptrDst = ',';
		while (ptrSrc != ptrBeg) {
			dms_assert(ptrDst > ptrSrc); dms_assert(ptrSrc > ptrBeg); *--ptrDst = *--ptrSrc;
			dms_assert(ptrDst > ptrSrc); dms_assert(ptrSrc > ptrBeg); *--ptrDst = *--ptrSrc;
			dms_assert(ptrDst > ptrSrc); dms_assert(ptrSrc > ptrBeg); *--ptrDst = *--ptrSrc;
			dms_assert(ptrDst > ptrSrc); dms_assert(ptrSrc >= ptrBeg); *--ptrDst = ',';
		}
		dms_assert(ptrDst == ptrSrc);
	}
	return nrSeparators;
}

int process_int(char* charBuf, UInt32 bufLen, int charCount)
{
	if (*charBuf == '-')
	{
		++charBuf; --charCount; --bufLen;
	}
	return process_uint(charBuf, charBuf + charCount, bufLen, charCount);
}

int process_frac(char* charBuf, UInt32 bufLen, int charCount)
{
	if (*charBuf == '-')
	{
		++charBuf; --charCount; --bufLen;
	}
	auto decPtr = charBuf, e = charBuf + charCount;
	
	while (decPtr != e)
	{
		if (*decPtr == '.' || *decPtr == 'E')
			break;
		++decPtr;
	}
	dms_assert(decPtr == e || *decPtr == '.' || *decPtr == 'E');
	return process_uint(charBuf, decPtr, bufLen, charCount);
}

template<>
int mysnprintf<double>(char* charBuf, UInt32 bufLen, CharPtr formatOut, double value, FormattingFlags ff)
{
	auto charCount = _snprintf(charBuf, bufLen, formatOut, value);
	dms_assert(UInt32(charCount )<= bufLen);
	if (HasThousandSeparator(ff))
		charCount += process_frac(charBuf, bufLen, charCount);
	return charCount;
}

template<>
int mysnprintf<unsigned int>(char* charBuf, UInt32 bufLen, CharPtr formatOut, unsigned int value, FormattingFlags ff)
{
	auto charCount = _snprintf(charBuf, bufLen, formatOut, value);
	dms_assert(UInt32(charCount) <= bufLen);
	if (HasThousandSeparator(ff))
		charCount += process_uint(charBuf, charBuf + charCount, bufLen, charCount);
	return charCount;
}

template<>
int mysnprintf<int>(char* charBuf, UInt32 bufLen, CharPtr formatOut, int value, FormattingFlags ff)
{
	auto charCount = _snprintf(charBuf, bufLen, formatOut, value);
	dms_assert(UInt32(charCount) <= bufLen);
	if (HasThousandSeparator(ff))
		charCount += process_int(charBuf, bufLen, charCount);
	return charCount;
}

template<>
int mysnprintf<unsigned __int64>(char* charBuf, UInt32 bufLen, CharPtr formatOut, unsigned __int64 value, FormattingFlags ff)
{
	auto charCount = _snprintf(charBuf, bufLen, formatOut, value);
	dms_assert(UInt32(charCount) <= bufLen);
	if (HasThousandSeparator(ff))
		charCount += process_uint(charBuf, charBuf + charCount, bufLen, charCount);
	return charCount;
}

template<>
int mysnprintf<__int64>(char* charBuf, UInt32 bufLen, CharPtr formatOut, __int64 value, FormattingFlags ff)
{
	auto charCount = _snprintf(charBuf, bufLen, formatOut, value);
	dms_assert(UInt32(charCount) <= bufLen);
	if (HasThousandSeparator(ff))
		charCount += process_int(charBuf, bufLen, charCount);
	return charCount;
}


#define ENABLE_UNDEF_HANDLING(X) X
#define DISABLE_UNDEF_HANDLING(X)

#define DEFINE_FORMATTED_STREAMABLE(T, U, C, FOUT, UNDEF_HANDLER) \
RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, T value) \
{   UNDEF_HANDLER( \
 	if (!IsDefined(value)) \
		str << Undefined(); \
	else ) \
	{ \
		char charBuf[C+1]; \
		int actualSize = mysnprintf<U>(charBuf, C+1, FOUT, U(value), str.GetFormattingFlags()); \
		dms_assert(actualSize > 0 && actualSize <= C+1); \
		str.Buffer().WriteBytes(charBuf, actualSize); \
	} \
	return str; \
} \
RTC_CALL FormattedInpStream& operator >>(FormattedInpStream& str, T& value) \
{ \
	value = Convert<T>(ReadValueOrNullAfterSpace<U>(str)); \
	return str; \
} \
RTC_CALL void AssignValueFromCharPtr(T& value, CharPtr data) \
{ \
	value = Convert<T>(ReadValueAfterSpace<U>(data)); \
} \
RTC_CALL void AssignValueFromCharPtrs(T& value, CharPtr begin, CharPtr end) \
{ \
	value = Convert<T>(ReadValueAfterSpace<U>(begin, end)); \
} \
RTC_CALL void AssignValueFromCharPtrs_Checked(T& value, CharPtr begin, CharPtr end) \
{ \
	value = Convert<T>(ReadValueAfterSpace<U>(begin, end)); \
} \
RTC_CALL bool AsCharArray(T value, char* buffer, UInt32 bufLen) \
{ \
	UNDEF_HANDLER( \
		if (!IsDefined(value)) \
		{ \
			CharPtr undef = UNDEFINED_VALUE_STRING; \
			fast_copy(undef, undef+Min<UInt32>(sizeof(UNDEFINED_VALUE_STRING), bufLen), buffer); \
			return sizeof(UNDEFINED_VALUE_STRING) <= bufLen; \
		} \
	) \
	int count = _snprintf(buffer, bufLen, FOUT, U(value)); \
	return UInt32(count) < bufLen; \
} \
			
DEFINE_FORMATTED_STREAMABLE(UInt32,unsigned int, 13, "%u", ENABLE_UNDEF_HANDLING)
DEFINE_FORMATTED_STREAMABLE(Int32, int, 14, "%d", ENABLE_UNDEF_HANDLING)
DEFINE_FORMATTED_STREAMABLE(UInt16,unsigned int,  6, "%u", ENABLE_UNDEF_HANDLING)
DEFINE_FORMATTED_STREAMABLE(Int16, int,  7, "%d", ENABLE_UNDEF_HANDLING)
DEFINE_FORMATTED_STREAMABLE(UInt8, unsigned int,  3, "%u", ENABLE_UNDEF_HANDLING)
DEFINE_FORMATTED_STREAMABLE(Int8,  int,  4, "%d", ENABLE_UNDEF_HANDLING)
DEFINE_FORMATTED_STREAMABLE(UInt4, unsigned int,  2, "%u", DISABLE_UNDEF_HANDLING)
#if defined(DMS_TM_HAS_UINT2)
DEFINE_FORMATTED_STREAMABLE(UInt2, unsigned int,  1, "%u", DISABLE_UNDEF_HANDLING)
#endif
DEFINE_FORMATTED_STREAMABLE(Float32, double, 20, "%G", ENABLE_UNDEF_HANDLING)
DEFINE_FORMATTED_STREAMABLE(Float64, double, 35, "%.9G", ENABLE_UNDEF_HANDLING)
#if defined(DMS_TM_HAS_FLOAT80)
DEFINE_FORMATTED_STREAMABLE(Float80, long double, 40, "%.9lG", ENABLE_UNDEF_HANDLING)
#endif
#if defined(DMS_TM_HAS_INT64)
DEFINE_FORMATTED_STREAMABLE(UInt64,unsigned __int64, 30, "%I64u", ENABLE_UNDEF_HANDLING)
DEFINE_FORMATTED_STREAMABLE(Int64, __int64, 30, "%I64d", ENABLE_UNDEF_HANDLING)
#endif

#include "ser/StreamException.h"

RTC_CALL FormattedInpStream& operator >>(FormattedInpStream& str, CharPtr value) 
{
	dms_assert(value);

	char inCh;
	str >> inCh;
	while (isspace(UChar(inCh)))
		str >> inCh;
	while (true)
	{
		char valCh = *value;
		dms_assert(valCh);
		if (valCh !=inCh)
		{
			if (inCh)
				throwErrorF("FormattedInpStream","expected '%c' but got '%c'", valCh, inCh);
			else
				throwErrorF("FormattedInpStream","expected '%c' at end of string", valCh);
		}
		valCh = *++value;
		if (!valCh) 
			return str;
		str >> inCh;
	}
	return str;
}

//DEFINE_FORMATTED_STREAMABLE(Bool)
FormattedOutStream& operator <<(FormattedOutStream& str, Bool value) 
{
	str << CharPtr(value ? "True" : "False");
	return str;
}

FormattedOutStream& operator <<(FormattedOutStream& str, const ErrMsg& err)
{
	str << err.GetAsText();

	return str;
}

FormattedInpStream& operator >>(FormattedInpStream& str, Bool& value) 
{
	auto result = str.NextToken();
	value = (result.second.size() && result.second[0]=='T');

	return str;
}

void AssignValueFromCharPtr(Bool& value, CharPtr data) 
{ 
	value = (*data=='T') || (*data == 't') || (*data == '1');
} 

void AssignValueFromCharPtrs(Bool& value, CharPtr begin, CharPtr end)
{
	if (begin == end)
		value = false;
	else
		AssignValueFromCharPtr(value, begin);
}

bool AssignBoolFromCharPtrs_Checked(CharPtr begin, CharPtr end)
{
	UInt32 size = end - begin;

	switch (size) {
	case 1:
		if (*begin == '1') return true;
		if (*begin == '0') return false;
		break;
	case 4:
		if (strnicmp(begin, "TRUE", size) == 0) return true;
		break;
	case 5:
		if (strnicmp(begin, "FALSE", size) == 0) return false;
		break;
	}
	throwErrorD("AssignBoolFromCharPtrs_Checked", "'true' or 'false' expected");
}

void AssignValueFromCharPtrs_Checked(Bool& value, CharPtr begin, CharPtr end)
{
	value = AssignBoolFromCharPtrs_Checked(begin, end);
}

RTC_CALL bool AsCharArray(Bool value, char* buffer, UInt32 bufLen)
{ 
	CharPtr str = value
		?	"True"
		:	"False";

	int count = _snprintf(buffer, bufLen, "%s", str);
	return UInt32(count) < bufLen;
}

//----------------------------------------------------------------------
// StreamableDataTime
//----------------------------------------------------------------------

StreamableDateTime::StreamableDateTime() 
{
	time(&m_time);
}

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& fos, const StreamableDateTime& self)
{
	char timeBuff[30];
	auto errCode = ctime_s(timeBuff, sizeof(timeBuff), &self.m_time);
	if (errCode == 0)
	{
		timeBuff[24] = 0;
		fos << timeBuff;
	}
	return fos;
}

extern "C" RTC_CALL CharPtr DMS_CONV RTC_MsgData_GetMsgAsCStr(MsgData * msgData)
{
	assert(msgData);
	return msgData->m_Txt.c_str();
}
