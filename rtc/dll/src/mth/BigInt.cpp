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

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "mth/BigInt.h"
#include "ptr/IterCast.h"
#include "set/BitVector.h"

#include "geo/MinMax.h"

//----------------------------------------------------------------------
// Implementation
//----------------------------------------------------------------------

namespace Big {

UInt::UInt(radix_type v)
{
	push_back(v);
}

void UInt::add(const UInt& rhs, size_type shift)
{
	UInt64 accumulator = 0;

	MG_PRECONDITION(rhs.size() + shift >= shift); // overflow check

	if (size() < shift)
		resize(shift, 0);

	size_type n1 = Min<size_type>(size(), rhs.size()+shift);
	dms_assert(n1 >= shift); // internal consistency

	iterator thisI = begin()+shift, thisE =  thisI + (n1-shift); 
	const_iterator thatI = rhs.begin(), thatE = rhs.end();
	for (; thisI!=thisE; ++thatI, ++thisI)
	{
		accumulator += *thisI;
		accumulator += *thatI; 
		*thisI = (accumulator & 0xFFFFFFFF);
		accumulator >>= 32;
	}
	thisE = end();
	for (; thisI!=thisE; ++thisI)
	{
		dms_assert(thatI == thatE); // internal consistency
		if (!(accumulator & 0xFFFFFFFF))
			break;
		accumulator += *thisI;
		*thisI = (accumulator & 0xFFFFFFFF);
		accumulator >>= 32;
	}
	for (; thatI!=thatE; ++thatI)
	{
		if (!(accumulator & 0xFFFFFFFF))
		{
			insert(end(), thatI, thatE);
			return;
		}
		accumulator += *thatI; 
		push_back(accumulator & 0xFFFFFFFF);
		accumulator >>= 32;
	}
	if (accumulator & 0xFFFFFFFF)
		push_back(accumulator & 0xFFFFFFFF);
}

void UInt::operator +=(const UInt& rhs)
{
	add(rhs, 0);
}

void UInt::operator *=(const UInt& rhs)
{
	UInt result;
	const_iterator
		rB = rhs.begin()
	,	rI = rhs.end();

	while (rI != rB)
	{
		UInt self = *this; self *= *--rI;
		result.add(self, rI - rB);
	}
	this->swap(result);
}

void UInt::operator /=(const UInt& rhs)
{
	bool isImplemented = false;
	dms_assert(isImplemented);
}

void UInt::operator *=(UInt32 factor)
{
	UInt64 accumulator = 0;

	iterator thisI = begin(), thisE =  end(); 
	for (; thisI!=thisE; ++thisI)
	{
		accumulator += RTC_UInt32xUInt32toUInt64(*thisI, factor);
		*thisI = (accumulator & 0xFFFFFFFF);
		accumulator >>= 32;
	}
	if (accumulator & 0xFFFFFFFF)
		push_back(accumulator & 0xFFFFFFFF);
}

void UInt::operator >>=(size_type shift)
{
	if (empty())
		return;
	erase(begin(), begin() + Min<size_type>((shift / 32), size()));
	UInt32 shiftRem = shift % 32;
	if (empty() || !shiftRem)
		return;
	UInt32 rshift = 32 - shiftRem;
	(*this)[0] >>= shiftRem;
	for (iterator i = begin()+1, e = end(); i != e; ++i)
	{
		i[-1] |= ((*i) << rshift);
		(*i) >>= shiftRem;
	}
	if (!back())
		pop_back();
}

void UInt::operator ++()
{
	for (iterator i = begin(), e = end(); i != e; ++i)
		if (++*i)
			return;
	push_back(1);
}


void UInt::StripLSB(UInt32 p)
{
	UInt32 s = (p+31)/32;
	if (s > size())
		return;
	erase(begin()+s, end());
	p %= 32;
	back() &= ~(( 1 << p)-1);
}

UInt::size_type UInt::ShiftToOdd()
{
	SizeT result = bit_sequence<1, radix_type>(begin_ptr(*this), size()*sizeof(radix_type)*8).FindLowestNonZeroPos();
	(*this) >>= result;
	return result;
}

/******************************************************************************/
//                          UInt operations
/******************************************************************************/

UInt operator + (const UInt& a, const UInt& b)
{
	UInt result = a; result+=b; return result;
}

UInt operator * (const UInt& a, const UInt& b)
{
	// following is the brute force O(n^2) covolution algorith
	// a more sofisticated O(n*log(n)) algorithm could be implemented by using numerical FFT but then cumulation of overflows become tricky
	UInt result = a; result*=b; return result;
}

UInt operator / (const UInt& a, const UInt& b)
{
	UInt result = a; result/=b; return result;
}

UInt operator >> (const UInt& a, UInt32 shift)
{
	UInt result = a; result>>=shift; return result;
}

} // namespace Big
/******************************************************************************/
//                         (U)Int64 functions
/******************************************************************************/

#include <windows.h>

RTC_CALL UInt64 RTC_UInt32xUInt32toUInt64(UInt32 a, UInt32 b) // calls UInt32x32To64
{
	return UInt32x32To64(a, b);
}

RTC_CALL Int64 RTC_Int32xInt32toInt64(Int32 a, Int32 b) // calls Int32x32To64
{
	return Int32x32To64(a, b);
}
