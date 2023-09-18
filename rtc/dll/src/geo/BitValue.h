
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

#if !defined(__RTC_GEO_BITVALUE_H)
#define __RTC_GEO_BITVALUE_H

#include "RtcBase.h"

#include "geo/MPF.h"
#include "geo/ElemTraits.h"
#include "geo/Undefined.h"

#include "dbg/check.h"

//======================== bit_valye

template <bit_size_t N>
struct bit_value
{
	using base_type = typename api_type<bit_value>::type ;

	static const bit_block_t nr_values = mpf::exp2<N>::value;
	static const bit_block_t mask      = nr_values -1;

	constexpr bit_value()                : m_Value(0) {}
	constexpr bit_value(base_type value) : m_Value(value)
	{
		static_assert(nr_values > 0);
		static_assert(sizeof(bit_value<N>) == 1);
		assert(UInt32(value) <= mask);
	}

	template <bit_size_t M>
	constexpr bit_value(const bit_value<M>& value): m_Value(value)
	{
		assert(UInt32(value) <= mask);
	}

	constexpr operator base_type() const { return m_Value; }
	constexpr base_type base_value() const { return m_Value;  }

	void operator =(base_type newValue)
	{ 
		assert(UInt32(newValue) <= mask);
		m_Value = newValue;  
	}
	template <bit_size_t M>
	void operator = (const bit_value<M>& newValue)
	{
		assert(UInt32(newValue) <= mask);
		m_Value = newValue;
	}

	constexpr bool operator <  (bit_value<N> oth) const { return m_Value <  oth.m_Value; }
	constexpr bool operator >  (bit_value<N> oth) const { return m_Value >  oth.m_Value; }
	constexpr bool operator == (bit_value<N> oth) const { return m_Value == oth.m_Value; }
	constexpr bool operator != (bit_value<N> oth) const { return m_Value != oth.m_Value; }
	constexpr bool operator >= (bit_value<N> oth) const { return m_Value >= oth.m_Value; }
	constexpr bool operator <= (bit_value<N> oth) const { return m_Value <= oth.m_Value; }
	constexpr bool operator !  ()                 const { return m_Value == base_type(); }

private:
	base_type m_Value; 
};

//----------------------------------------------------------------------
// Section      : MinMax
//----------------------------------------------------------------------

template <bit_size_t N>
struct minmax_traits<bit_value<N> >
{
	static bit_value<N> MinValue() { return bit_value<N>(); }
	static bit_value<N> MaxValue() { return bit_value<N>::mask; } 
};

//----------------------------------------------------------------------
// Section      : Undefined
//----------------------------------------------------------------------

template <bit_size_t N>
constexpr bit_value<N> UndefinedOrZero(const bit_value<N>* ) { return 0; }

template <bit_size_t N>
constexpr bit_value<N> UndefinedOrMax(const bit_value<N>* ) { return bit_value<N>::mask; }

template <bit_size_t N> inline bool IsDefined(bit_value<N>) 
{
	struct dont_instantiate_this; return dont_instantiate_this(); 
}

template <typename T>
inline bool IsBitValueOrDefined(const T& v) 
{ 
	if constexpr (has_undefines_v<T>)
		return IsDefined(v);
	else
		return true;
}

#endif // !defined(__RTC_GEO_BITVALUE_H)
