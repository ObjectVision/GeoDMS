// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_SER_POINTSTREAM_H
#define __RTC_SER_POINTSTREAM_H

#include <cctype>

#include "dbg/Diagnostics.h"
#include "ser/FormattedStream.h"
#include "geom/Point.h"
#include "geom/PointOrder.h"

//----------------------------------------------------------------------
// Section      : Serialization support for Points
//----------------------------------------------------------------------

template <class T> inline
BinaryOutStream& operator <<(BinaryOutStream& os, const Point<T>& p)
{
	os << p.Row() << p.Col();
	return os;
}

template <class T> inline
BinaryInpStream& operator >>(BinaryInpStream& is, Point<T>& p)
{
	is >> p.Row() >> p.Col();
	return is;
}

//----------------------------------------------------------------------
// Section      : textual point syntax: xy(x; y)
//
// A bare pair of numbers cannot say which coordinate comes first, and the two
// halves of the product disagreed about it: the range property rendered {row, col}
// while the detail page rendered {col, row}. Data blocks settled this long ago by
// requiring the xy(..) / yx(..) tags (see DataBlockParse.h and the diagnostic in
// PointOrder.h); textual points now use the same tags. Output is always xy(x; y);
// input also accepts yx(y; x), either separator, and the legacy untagged {row, col}
// that existing configurations and .mmd dictionaries are full of.
//----------------------------------------------------------------------

namespace point_stream {

inline void SkipSpace(FormattedInpStream& is)
{
	while (is.NextChar() && isspace(UChar(is.NextChar())))
		is.ReadChar();
}

inline void ReadChar(FormattedInpStream& is, char expected)
{
	SkipSpace(is);
	char ch = is.NextChar();
	if (ch != expected)
		throwErrorF("PointStream", "expected '{}' but got '{}'", expected, ch ? ch : ' ');
	is.ReadChar();
}

// ',' and ';' are interchangeable: ';' is what a rendering with thousand separators
// must use to stay unambiguous, ',' is what everything written by hand tends to use.
inline void ReadSeparator(FormattedInpStream& is)
{
	SkipSpace(is);
	char ch = is.NextChar();
	if (ch != ',' && ch != ';')
		throwErrorF("PointStream", "expected ',' or ';' between two coordinates but got '{}'", ch ? ch : ' ');
	is.ReadChar();
}

// the tag has already been peeked at; consume it case-insensitively
inline void ReadTag(FormattedInpStream& is, CharPtr tag)
{
	for (; *tag; ++tag)
	{
		char ch = is.NextChar();
		if (tolower(UChar(ch)) != *tag)
			throwErrorF("PointStream", "expected point tag 'xy' or 'yx' but got '{}'", ch ? ch : ' ');
		is.ReadChar();
	}
}

} // namespace point_stream

template <typename T> inline
FormattedOutStream& operator << (FormattedOutStream& os, const Point<T>& p)
{
	os << "xy(" << p.X() << "; " << p.Y() << ")";
	return os;
}

template <typename T> inline
FormattedInpStream& operator >> (FormattedInpStream& is, Point<T>& p)
{
	using namespace point_stream;

	SkipSpace(is);
	switch (tolower(UChar(is.NextChar())))
	{
	case 'x':
		ReadTag(is, "xy"); ReadChar(is, '(');
		is >> p.X(); ReadSeparator(is); is >> p.Y();
		ReadChar(is, ')');
		break;

	case 'y':
		ReadTag(is, "yx"); ReadChar(is, '(');
		is >> p.Y(); ReadSeparator(is); is >> p.X();
		ReadChar(is, ')');
		break;

	case '{': // legacy untagged form, always meant {row, col}
		ReadChar(is, '{');
		is >> p.Row(); ReadSeparator(is); is >> p.Col();
		ReadChar(is, '}');
		break;

	default:
		throwErrorF("PointStream", "expected a point as xy(x; y), yx(y; x) or {{row; col}} but got '{}'"
			, is.NextChar() ? is.NextChar() : ' ');
	}
	return is;
}

#endif // __RTC_SER_POINTSTREAM_H
