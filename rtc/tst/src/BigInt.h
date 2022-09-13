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
#include "ptr/RefObject.h"
#include "ptr/RefPtr.h"
#include "mth/mathlib.h"

struct BigUInt
{
private:
	typedef RefObjectArray<UInt32> DataType;
public:
	typedef DataType::const_iterator const_iterator;
	typedef DataType::iterator       iterator;

	BigUInt() {}
	BigUInt(UInt32 val,UInt32 count=1): m_Array(DataType::Create(count, val)) {}
	BigUInt(const_iterator first, const_iterator last) : m_Array(DataType::Create(first, last)) {}

	void Cleanup() const;
	bool IsClean() const { return !m_Array || !m_Array->size() || m_Array->back(); }
	UInt32 size() const { return m_Array ? m_Array->size() : 0; }

private:
	RefPtr<DataType> m_Array;
friend BigUInt operator *(const BigUInt& a, const BigUInt& b);
friend BigUInt operator +(const BigUInt& a, const BigUInt& b);
friend bool  operator <(const BigUInt& a, const BigUInt& b);
};

void BigUInt::Cleanup() const
{
	if (m_Array)
	{
		const_iterator b = m_Array->begin(), e = m_Array->end(), f = e;
		while (e!=b && *e == 0) --e;
		if (e!=f)
			m_Array->Shrink(e-b);
	}
}


void _UnshieldedAdd(BigUInt::iterator buffer, BigUInt::const_iterator first, BigUInt::const_iterator last)
{
	UInt32 count = last - first;
	if (!count) return;
	_asm {
		mov edi, buffer
		mov esi, first
		mov ecx, count
		clc

_loop:
		lodsd 
		adc eax, DWORD PTR [edi]
		stosd
		loop _loop
		jnc _end;
		inc DWORD PTR [edi]
_end:
	}
}

void _UnshieldedAddQW(BigUInt::iterator buffer, UInt64* _qWord)
{
	_asm {
		mov edi, buffer
		mov esi, _qWord
		// add least significant dw
		lodsd
		add eax, DWORD PTR [edi]
		stosd
		// add most  significant dw
		lodsd 
		adc eax, DWORD PTR [edi]
		stosd

		jnc _end;
		inc DWORD PTR [edi]
_end:
	}
}

bool  operator <(const BigUInt& a, const BigUInt& b) 
{
	dms_assert(a.IsClean());
	dms_assert(b.IsClean());
	if (a.size() < b.size()) return true;  // less
	if (a.size() > b.size()) return false; // greater
	if (a.size() == 0)       return false; // equal

	BigUInt::const_iterator ba = a.m_Array->begin(), ea = a.m_Array->end(), bb = b.m_Array->begin(), eb = b.m_Array->end();
	dms_assert(ba != ea && ea-ba == eb-bb);
	while(ea !=ba)
	{
		if (*--ea < *--eb) return true;  // less
		if (*ea   > *eb)   return false; // greater
	}
	return false;                       // equal
}
BigUInt operator *(const BigUInt& a, const BigUInt& b) 
{
	dms_assert(a.IsClean());
	dms_assert(b.IsClean());
	if (a.size() == 0 || b.size() == 0)
		return BigUInt();
	BigUInt result(0, a.size() + b.size());
	BigUInt::const_iterator ia = a.m_Array->begin(), ea = a.m_Array->end(), bb = b.m_Array->begin(), eb = b.m_Array->end();
	BigUInt::iterator ir = result.m_Array->begin();
	while (ia != ea)
	{
		BigUInt::const_iterator ib = bb;
		BigUInt::iterator ijr = ir;
		UInt64 acc;
		while (ib != eb)
		{
			acc = RTC_UInt32xUInt32toUInt64(*ia, *ib++);
			_UnshieldedAddQW(ijr++, &acc);
		}
		++ia;
		++ir;
	}
	result.Cleanup();
	return result;
}

BigUInt operator + (const BigUInt& a, const BigUInt& b)
{
	dms_assert(a.IsClean());
	dms_assert(b.IsClean());
	if (a.size() == 0 && b.size() == 0)
		return BigUInt();
	BigUInt result(0, Max(a.size(), b.size())+1);
	if (a.m_Array)
		std::copy(a.m_Array->begin(), a.m_Array->end(), result.m_Array->begin());
	if (b.m_Array)
		_UnshieldedAdd(result.m_Array->begin(), b.m_Array->begin(), b.m_Array->end());
	result.Cleanup();
	return result;
}
