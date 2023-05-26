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

#if !defined(__RTC_SER_BINARYSTREAM)
#define __RTC_SER_BINARYSTREAM

#include "geo/BitValue.h"
#include "ser/BaseStreamBuff.h"
#include "utl/instantiate.h"

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
