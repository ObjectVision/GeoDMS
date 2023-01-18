#pragma once

#ifndef __RTC_GEO_CONVERTFUNCTOR_H
#define __RTC_GEO_CONVERTFUNCTOR_H

#include "geo/Conversions.h"
#include <functional>

template<typename Dst, typename Src, typename Func>
struct ConvertResultFunctor
{
	typedef Src argument_type;
	typedef Dst result_type;

	ConvertResultFunctor(Func func = Func()) : m_Func(func) {}

	Dst operator ()(const Src& src) const
	{
		return Convert<Dst>( m_Func( src ) );
	}

	Func m_Func;
};


#endif // __RTC_GEO_CONVERTFUNCTOR_H
