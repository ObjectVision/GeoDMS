// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  bit_value<N>: the N-bit packed value type (Bool, UInt2, UInt4) with its
 *  minmax and undefined traits - the element type of bit-packed sequences.
 */

#if !defined(__VT_BITVALUE_H)
#define __VT_BITVALUE_H

#include "RtcBase.h"

#include "vt/mpf.h"
#include "vt/ElemTraits.h"
#include "vt/Undefined.h"

#include "dbg/Diagnostics.h"

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

void dont_link_this();

template <bit_size_t N> inline bool IsDefined(bit_value<N>)
{
	dont_link_this(); // intentionally undefined: a bit_value is never undefined, so any real use must fail to link
//	struct dont_instantiate_this; return dont_instantiate_this();
	return false;     // unreachable; present only to satisfy -Wreturn-type on GCC
}

template <typename T>
inline bool IsBitValueOrDefined(const T& v) 
{ 
	if constexpr (has_undefines_v<T>)
		return IsDefined(v);
	else
		return true;
}

#endif // !defined(__VT_BITVALUE_H)
