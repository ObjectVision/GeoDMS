// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  ConvertResultFunctor: the unary functor form of Convert, for use in
 *  transformations.
 */

#if !defined(__VT_CONVERTFUNCTOR_H)
#define __VT_CONVERTFUNCTOR_H

#include "vt/Conversions.h"
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


#endif // __VT_CONVERTFUNCTOR_H
