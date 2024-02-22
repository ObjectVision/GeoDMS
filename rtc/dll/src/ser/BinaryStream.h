// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_SER_BINARYSTREAM)
#define __RTC_SER_BINARYSTREAM

#include "geo/BitValue.h"
#include "ser/BaseStreamBuff.h"
#include "utl/Instantiate.h"

// *****************************************************************************
// Section:     BinaryOutStream & BinaryInpStream
// *****************************************************************************

struct BinaryOutStream
{ 
  public:
	BinaryOutStream(OutStreamBuff* out) : m_OutStreamBuff(out)
	{
		MG_PRECONDITION(out);
	}

	OutStreamBuff& Buffer() { return *m_OutStreamBuff; }

  private:
	OutStreamBuff* m_OutStreamBuff;
};

struct BinaryInpStream
{ public:
	BinaryInpStream(const InpStreamBuff* inp) : m_InpStreamBuff(inp) 
	{
		MG_PRECONDITION(inp);
	}

	const InpStreamBuff& Buffer() { return *m_InpStreamBuff; }

  private:
	const InpStreamBuff* m_InpStreamBuff;
};

/*
 *  Description : Define binary streaming for each basic type that is binary streamable
 *  Definition  : a "binary streamable type" is a type that contains no pointers
 */



template<typename T> concept BinaryStreableObject = is_numeric_v<T> && has_fixed_elem_size_v<T>;

template<BinaryStreableObject T>
BinaryOutStream& operator <<(BinaryOutStream& os, T obj)
{
	os.Buffer().WriteBytes((const char*)&obj, sizeof(T));
	return os;
}

template<BinaryStreableObject T>
BinaryInpStream& operator >>(BinaryInpStream& is, T& obj)
{
	is.Buffer().ReadBytes((char*)&obj, sizeof(T));
	return is;
}

// specific handling of UInt64 values to keep binary-stream compatibility between Win32 and x64 builds of the GeoDMS of sizes < MAX_VALUE(UInt32)
// and use a simple compression assuming most values will be < MAX_VALUE(UInt32).
// sizes > MAX_VALUE(UInt32) are not supported by Win32 builds

inline BinaryOutStream& operator <<(BinaryOutStream& os, UInt64 v)
{
	if (v < MAX_VALUE(UInt32))
		os << UInt32(v);
	else if (!IsDefined(v))
		os << UNDEFINED_VALUE(UInt32);
	else 
	{
		os << MAX_VALUE(UInt32);
		os.Buffer().WriteBytes((const char*)&v, sizeof(UInt64));
	}
	return os;
}

inline BinaryInpStream& operator >>(BinaryInpStream& is, UInt64& v)
{
	UInt32 v32;
	is >> v32;
	if (v32 <  MAX_VALUE(UInt32))
		v = v32;
	else if (!IsDefined(v32))
		MakeUndefined(v);
	else 
		is.Buffer().ReadBytes((char*)&v, sizeof(UInt64));
	return is;
}

inline SizeT NrStreamBytesOf(UInt32) { return sizeof(UInt32); }
inline SizeT NrStreamBytesOf(UInt64 s) { return s < MAX_VALUE(UInt32) ? sizeof(UInt32) : sizeof(UInt32) + sizeof(UInt64); }

#endif // __RTC_SER_BINARYSTREAM
