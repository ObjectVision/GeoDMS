// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_SER_PAIRSTREAM_H
#define __RTC_SER_PAIRSTREAM_H

#include "geo/Pair.h"
#include "ser/FormattedStream.h"

//----------------------------------------------------------------------
// Section      : Serialization support for Tuples
//----------------------------------------------------------------------

template <class T1, class T2> inline
BinaryOutStream& operator <<(BinaryOutStream& ar, const std::pair<T1, T2>& t)
{
	ar << t.first << t.second;
	return ar;
}

template <class T1, class T2> inline
BinaryInpStream& operator >>(BinaryInpStream& ar, std::pair<T1, T2>& t)
{
	ar >> t.first >> t.second;
	return ar;
}

template <class T1, class T2> inline
FormattedOutStream& operator << (FormattedOutStream& os, const std::pair<T1, T2>& p)
{
	os << "{" << p.first << ", " << p.second << "}";
	return os;
}

template <class T1, class T2> inline
FormattedInpStream& operator >> (FormattedInpStream& is, std::pair<T1, T2>& p)
{
	is >> "{" >> p.first >> ", " >> p.second >> "}";
	return is;
}

#endif // __RTC_SER_PAIRSTREAM_H
