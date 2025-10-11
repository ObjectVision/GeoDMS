// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include <stdio.h>
#include <ctype.h>

#include "geo/Conversions.h"
#include "geo/iterrange.h"
#include "geo/StringBounds.h"
#include "geo/Undefined.h"
#include "ser/FormattedStream.h"
#include "ser/ReadValue.h"
#include "ser/StringStream.h"
#include "set/rangefuncs.h"
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

template<typename U>
auto mysnprintf(char* charBuf, UInt32 bufLen, U value, FormattingFlags ff) -> UInt32
{
	auto to_chars_result = std::to_chars(charBuf, charBuf + bufLen, value);
	if (to_chars_result.ec != std::errc()) return 0;
	auto charCount = static_cast<UInt32>(to_chars_result.ptr - charBuf);
	assert(UInt32(charCount) <= bufLen);
	if (HasThousandSeparator(ff))
	{
		if constexpr (std::is_floating_point_v<U>)
			charCount += process_frac(charBuf, bufLen, charCount);
		else if constexpr (std::is_signed_v<U>)
			charCount += process_int(charBuf, bufLen, charCount);
		else
			charCount += process_uint(charBuf, charBuf + charCount, bufLen, charCount);
	}
	return charCount;
}

template<typename T> struct mysnprintf_type { using type = T; };
template<> struct mysnprintf_type<Int8> { using type = Int32; };
template<> struct mysnprintf_type<UInt8> { using type = UInt32; };
template<> struct mysnprintf_type<UInt4> { using type = UInt32; };
template<> struct mysnprintf_type<UInt2> { using type = UInt32; };
template<typename T> using mysnprintf_type_t = typename mysnprintf_type<T>::type;


constexpr int NUMERIC_BUFFER_SIZE = 32;
constexpr int NUMERIC_BUFFER_SIZEZ = NUMERIC_BUFFER_SIZE + 1;

template<typename T>
UInt32 AsCharArrayBase(T value, char* buffer, UInt32 bufLen, FormattingFlags ff) 
{ 
	assert(bufLen < UInt32(-1)); 
	using U = mysnprintf_type_t<T>; 
	auto actualSize = mysnprintf<U>(buffer, bufLen, value, ff); 
	assert(actualSize > 0 && actualSize <= bufLen); 
	return actualSize; 
} 

template<typename T>
RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, T value) requires is_numeric_v<T>
{ 
	if constexpr (has_undefines_v<T>) 
	{
		if (!IsDefined(value))
		{
			str << Undefined();
			return str;
		}
	}
	char charBuf[NUMERIC_BUFFER_SIZEZ];
	auto actualSize = AsCharArrayBase(value, charBuf, NUMERIC_BUFFER_SIZEZ, str.GetFormattingFlags());
	if (actualSize > 0u && actualSize <= NUMERIC_BUFFER_SIZEZ)
		str.Buffer().WriteBytes(charBuf, actualSize); 
	return str; 
} 

template<typename T>
RTC_CALL FormattedInpStream& operator >>(FormattedInpStream& str, T& value) requires is_numeric_v<T>
{ 
	using U = mysnprintf_type_t<T>; 
	value = Convert<T>(ReadValueOrNullAfterSpace<U>(str)); 
	return str; 
} 

template<typename T>
RTC_CALL void AssignNumericValueFromCharPtr(T& value, CharPtr data) 
{ 
	using U = mysnprintf_type_t<T>;
	value = Convert<T>(ReadValueAfterSpace<U>(data));
} 

template<typename T>
RTC_CALL void AssignNumericValueFromCharPtrs(T& value, CharPtr begin, CharPtr end) 
{ 
	using U = mysnprintf_type_t<T>;
	value = Convert<T>(ReadValueAfterSpace<U>(begin, end));
} 

template<typename T>
RTC_CALL void AssignNumericValueFromCharPtrs_Checked(T& value, CharPtr begin, CharPtr end)
{ 
	using U = mysnprintf_type_t<T>;
	value = Convert<T>(ReadValueAfterSpace<U>(begin, end));
} 

template<typename T>
RTC_CALL bool AsCharArray(T value, char* buffer, UInt32 bufLen)
{ 
	if constexpr (has_undefines_v<T>) 
		if (!IsDefined(value)) 
		{ 
			CharPtr undef = UNDEFINED_VALUE_STRING; 
			fast_copy(undef, undef + Min<UInt32>(sizeof(UNDEFINED_VALUE_STRING), bufLen), buffer); 
			return sizeof(UNDEFINED_VALUE_STRING) <= bufLen; 
		} 
	auto actualSize = AsCharArrayBase(value, buffer, bufLen, FormattingFlags::None); 
	return actualSize > 0u && actualSize <= bufLen; 
} 


#define INSTANTIATE(T) \
template RTC_CALL FormattedOutStream& operator << <T> (FormattedOutStream& str, T value); \
template RTC_CALL FormattedInpStream& operator >> <T> (FormattedInpStream& str, T& value); \
template RTC_CALL void AssignNumericValueFromCharPtr<T>(T& value, CharPtr data); \
template RTC_CALL void AssignNumericValueFromCharPtrs<T>(T& value, CharPtr begin, CharPtr end); \
template RTC_CALL void AssignNumericValueFromCharPtrs_Checked<T>(T& value, CharPtr begin, CharPtr end); \
template RTC_CALL bool AsCharArray(T value, char* buffer, UInt32 bufLen); \

INSTANTIATE_NUM_ELEM

#undef INSTANTIATE

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

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, CharPtr value)
{
	assert(value);
	str.Buffer().WriteBytes(value, StrLen(value));
	return str;
}

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, CharPtrRange value)
{
	str.Buffer().WriteBytes(value.begin(), value.size());
	return str;
}

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, Bool value)
{
	str << CharPtr(value ? "True" : "False");
	return str;
}

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, const SharedStr& value)
{
	assert(value.has_ptr());
	str.Buffer().WriteBytes(value.c_str(), value.ssize());
	return str;
}


RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, const ErrMsg& err)
{
	str << err.GetAsText();

	return str;
}

RTC_CALL FormattedInpStream& operator >>(FormattedInpStream& str, Bool& value)
{
	auto result = str.NextToken();
	value = (result.second.size() && (result.second[0]=='T' || result.second[0] == 't'));

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

	int count = snprintf(buffer, bufLen, "%s", str);
	return UInt32(count) < bufLen;
}

//----------------------------------------------------------------------
// StreamableDataTime
//----------------------------------------------------------------------

StreamableDateTime::StreamableDateTime() 
{
	time(&m_time);
}

int write_time_str(char* buff, SizeT n, time_t t);

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& fos, const StreamableDateTime& self)
{
	char timeBuff[60];
	if (write_time_str(timeBuff, sizeof(timeBuff), self.m_time) > 0)
		fos << static_cast<CharPtr>(timeBuff);

	return fos;
}

extern "C" RTC_CALL CharPtr DMS_CONV RTC_MsgData_GetMsgAsCStr(MsgData * msgData)
{
	assert(msgData);
	return msgData->m_Txt.c_str();
}
