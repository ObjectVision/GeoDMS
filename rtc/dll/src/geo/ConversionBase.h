// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_GEO_CONVERSIONBASE_H
#define __RTC_GEO_CONVERSIONBASE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"

//----------------------------------------------------------------------
// Convert function wrapper
//----------------------------------------------------------------------

struct undefined_or_zero_func
{
	template <typename U, typename T>
	U apply(const T& /* v */) const
	{
		return UNDEFINED_OR_ZERO(U);
	}
};

struct throw_func
{
	template <typename U, typename T>
	[[noreturn]] U apply(const T& v) const
	{
		throwErrorF("Convert", "Failed to convert %s to %s", v, typeid(U).name());
	}
};

template<typename Dst, typename Src> inline
Dst Convert(const Src& src)
{
	using ConvertFunc = DefaultConvertFunc<Dst> ;
	return Convert4(src, TYPEID(Dst), TYPEID(undefined_or_zero_func), TYPEID(ConvertFunc));
}

template<typename Dst, typename Src> inline
Dst SignedIntGridConvert(const Src& src)
{
	using ConvertFunc = IntRoundDnFunc<Dst>;
	return Convert4(src, TYPEID(Dst), TYPEID(undefined_or_zero_func), TYPEID(ConvertFunc));
}

template<typename Dst, typename Src> inline
Dst ThrowingConvert(const Src& src)
{
	typedef DefaultConvertFunc<Dst> ConvertFunc;
	return Convert4(src, TYPEID(Dst), TYPEID(throw_func), TYPEID(ConvertFunc));
}

template<typename Dst, typename Src> inline
Dst ThrowingConvertNonNull(const Src& src)
{
	typedef DefaultConvertFunc<Dst> ConvertFunc;
	return ConvertNonNull4(src, TYPEID(Dst), TYPEID(throw_func), TYPEID(ConvertFunc));
}


#endif // __RTC_GEO_CONVERSIONBASE_H
