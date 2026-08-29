// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

/*
 *  Description : Determine for each range type whether it is a binary streamable type
 *  Definition  : a "binary streamable type" is a type that contains no pointers
 */

#ifndef __SER_RANGEUTIL_H
#define __SER_RANGEUTIL_H

// Range
#include "dbg/Diagnostics.h"
#include "geom/Range.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"

//----------------------------------------------------------------------
// Section      : range streaming operators
//----------------------------------------------------------------------


template <class T> inline
BinaryOutStream& operator <<(BinaryOutStream& os, const Range<T>& r)
{
//	os << r.first << r.second;
	os.Buffer().WriteBytes((const char*)&r, sizeof(Range<T>));
	return os;
}

template <class T> inline
BinaryInpStream& operator >>(BinaryInpStream& is, Range<T>& r)
{
//	is >> r.first >> r.second;
	is.Buffer().ReadBytes((char*)&r, sizeof(Range<T>));
	return is;
}

template <class T>
FormattedOutStream& operator << (FormattedOutStream& os, const Range<T>& r)
{
	os << "[" << r.first << ", " << r.second << ") ";
	return os;
}

// Both bounds and the separator between them are read leniently: '[0,34)', '[0, 34) ' and
// '[xy(0; 300000); xy(280000; 625000))' all parse. For 2d ranges the bounds carry their own
// xy()/yx() tag, so only the pair order is fixed here: lower bound first, then upper bound.
template <class T>
FormattedInpStream& operator >> (FormattedInpStream& is, Range<T>& r)
{
	point_stream::ReadChar(is, '[');
	point_stream::SkipSpace(is);
	// A '{' can only start the legacy untagged {row, col} spelling of a point bound. Deprecated
	// for the coordinate-order reasons of #1165 -- since 20.14.0 output is always xy(x; y) -- but
	// still parsed, as existing configurations and .mmd dictionaries carry it. This is the range
	// parse only: the high-volume point readers (sequences, data blocks) do not pass through here.
	bool legacyPointNotation = (is.NextChar() == '{');
	is >> r.first;
	point_stream::ReadSeparator(is);
	point_stream::SkipSpace(is);
	legacyPointNotation |= (is.NextChar() == '{');
	is >> r.second;
	point_stream::ReadChar(is, ')');
	if (legacyPointNotation)
		reportF(SeverityTypeID::ST_Warning
			, "Depreciated point notation {{row, col}} in a range; write it as \"[xy(x1; y1), xy(x2; y2))\", see wiki topic XY-order");
	return is;
}


#endif // __SER_RANGEUTIL_H
