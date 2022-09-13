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

#if !defined(__RTC_SER_FORMATTEDSTREAM_H)
#define __RTC_SER_FORMATTEDSTREAM_H

#include "ser/BaseStreamBuff.h"
#include "geo/IterRange.h"
#include "geo/StringBounds.h"
#include "ptr/StaticPtr.h"
#include "utl/instantiate.h"

//----------------------------------------------------------------------
// FormattingFlags
//----------------------------------------------------------------------

enum class FormattingFlags {
	None = 0,
	ThousandSeparator = 1
};

inline bool HasThousandSeparator(FormattingFlags ff) { return UInt32(ff) & UInt32(FormattingFlags::ThousandSeparator);  }


// *****************************************************************************
// Section:     FormattedOutStream
// *****************************************************************************

struct FormattedOutStream : boost::noncopyable
{ 
	RTC_CALL FormattedOutStream(OutStreamBuff* out, FormattingFlags ff);

	OutStreamBuff& Buffer() { return *m_OutStreamBuff; }
	FormattingFlags GetFormattingFlags() const { return m_FormattingFlags; }

  private:
	OutStreamBuff*  m_OutStreamBuff;
	FormattingFlags m_FormattingFlags;
};

// *****************************************************************************
// Section:     FormattedInpStream
// *****************************************************************************

typedef unsigned char UChar;

struct FormattedInpStream : std::iterator<std::input_iterator_tag, char>
{ 
	enum class reader_flags : UInt32
	{
		commaAsDecimalSeparator = 1,
		noNegatives       =  2,
		noScientific      =  4,
		stringsWithTabs   =  8,
		stringsWithSpaces = 16
	};
	friend bool operator & (reader_flags lhs, reader_flags rhs) { return UInt32(lhs) & UInt32(rhs); }

	enum class TokenType { 
		None, 
		Word, 
		SingleQuote, 
		DoubleQuote, 
		Number, 
		EOF_, 
		Unknown, 
		Punctuation 
	};

	FormattedInpStream();
	RTC_CALL FormattedInpStream(InpStreamBuff* inp, reader_flags rf = reader_flags());

	InpStreamBuff& Buffer() { return *m_InpStreamBuff; }

	   const char& NextChar() const { return m_NextChar; }
	RTC_CALL char  ReadChar();

//	input iterator semantics
//	const char& operator * () const { return NextChar(); }
//	void       operator ++() { ReadChar(); }

	RTC_CALL std::pair<FormattedInpStream::TokenType, CharPtrRange> NextToken();
	RTC_CALL CharPtrRange NextWord ();

	streamsize_t CurrPos() const { return m_InpStreamBuff->CurrPos() - (AtEnd() ? 0 : 1); }
	RTC_CALL void SetCurrPos(streamsize_t pos); 

	UInt32 GetLineNr() const { return m_LineNr; }
	UInt32 GetColNr()  const { return UInt32(1+ CurrPos() - m_LineStartPos); }
	bool AtEnd() const { return m_AtEnd; }

//	reader_flags SetReaderFlags(reader_flags rf) { std::swap(rf, m_Flags);  return rf; }
	bool IsCommaDecimalSeparator() const { return m_Flags & reader_flags::commaAsDecimalSeparator; }
	RTC_CALL bool IsFieldSeparator(char ch);

  private:
	bool ReadComment();
	bool ReadSQuote();
	bool ReadDQuote();

	InpStreamBuff* m_InpStreamBuff;
	char           m_NextChar;
	std::vector<char> m_NextToken;
	UInt32         m_LineNr;
	streamsize_t   m_LineStartPos;
	bool           m_AtEnd;
	reader_flags   m_Flags;
	MG_DEBUGCODE( streamsize_t md_LastPos; )
};

#define INSTANTIATE(T) \
RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, T value); \
RTC_CALL FormattedInpStream& operator >>(FormattedInpStream& str, T& value); \
RTC_CALL void AssignValueFromCharPtr(T& value, CharPtr data); \
RTC_CALL void AssignValueFromCharPtrs(T& value, CharPtr begin, CharPtr end); \
RTC_CALL void AssignValueFromCharPtrs_Checked(T& value, CharPtr begin, CharPtr end); \
RTC_CALL bool AsCharArray(T value, char* buffer, UInt32 bufLen); \


INSTANTIATE_NUM_ELEM

INSTANTIATE_BOOL
INSTANTIATE_UINT2
INSTANTIATE_UINT4

#undef INSTANTIATE

//DEFINE_FORMATTED_STREAMABLE(char)
inline FormattedOutStream& operator <<(FormattedOutStream& str, char value) 
{
	str.Buffer().WriteBytes(&value, 1);
	return str;
}

inline FormattedInpStream& operator >>(FormattedInpStream& str, char& value) 
{
	value = str.NextChar();
	str.ReadChar();
	return str;
}

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, const Undefined&);

//DEFINE_FORMATTED_STREAMABLE(Void)
inline FormattedOutStream& operator <<(FormattedOutStream& str, const Void&) { return str; }
inline FormattedInpStream& operator >>(FormattedInpStream& str,       Void&) { return str; }

//INSTANTIATE(CharPtr)
inline FormattedOutStream& operator <<(FormattedOutStream& str, CharPtr value)
{
	dms_assert(value);
	str.Buffer().WriteBytes(value, StrLen(value));
	return str;
}


RTC_CALL FormattedInpStream& operator >>(FormattedInpStream& str, CharPtr value);

//----------------------------------------------------------------------
// StreamableDataTime
//----------------------------------------------------------------------

struct StreamableDataTime // Display operating system-style date and time. 
{
	char dateBuff[128], timeBuff[128];

	StreamableDataTime();
};

FormattedOutStream& operator <<(FormattedOutStream& fos, const StreamableDataTime& self);


#endif // __RTC_SER_FORMATTEDSTREAM_H
