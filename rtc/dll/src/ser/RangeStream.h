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
#include "dbg/Check.h"
#include "geo/Range.h"
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

template <class T>
FormattedInpStream& operator >> (FormattedInpStream& is, Range<T>& r)
{
	is >> "[" >> r.first >> ", " >> r.second >> ")";
	return is;
}


#endif // __SER_RANGEUTIL_H
