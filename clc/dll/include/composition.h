// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_COMPOSITION_H)
#define __CLC_COMPOSITION_H

#include "Prototypes.h"

// *****************************************************************************
//						FUNCTOR COMPOSITIONS
// *****************************************************************************

struct no_block_func 
{
	no_block_func(auto bf, auto v)
	{}
};

// ====================== composition_2_v_p(arg1) = TBinFunc(value1, arg1)

template<typename TBinaryFunc>
struct composition_2_v_p_base
	: unary_func<
	typename TBinaryFunc::res_type,
	typename TBinaryFunc::arg2_type
	>
{
	composition_2_v_p_base(TBinaryFunc bf, typename TBinaryFunc::arg1_cref v)
		: m_BinFunc(bf)
		, m_Value(v)
	{}

	typename composition_2_v_p_base::res_type operator()(typename composition_2_v_p_base::arg1_cref arg) const
	{
		return m_BinFunc(m_Value, arg);
	}

	TBinaryFunc            m_BinFunc;
	typename TBinaryFunc::arg1_cref m_Value;
};

template<typename TBinaryFunc>
struct c2vp_block_func_provider {
	using block_func = composition_2_v_p_base<typename TBinaryFunc::block_func>;
	block_func m_BlockFunc;

	c2vp_block_func_provider(TBinaryFunc& bf, typename TBinaryFunc::arg2_cref v)
		: m_BlockFunc(bf.m_BlockFunc, v * unit_block< nrbits_of_v<typename TBinaryFunc::arg1_type> >::value)
	{}
};

template<typename TBinaryFunc>
struct composition_2_v_p
	:	composition_2_v_p_base<TBinaryFunc> 
	,	std::conditional_t< has_block_func_v < TBinaryFunc>, c2vp_block_func_provider<TBinaryFunc>, no_block_func>
{
	composition_2_v_p(TBinaryFunc bf, typename TBinaryFunc::arg1_cref v)
		: composition_2_v_p_base<TBinaryFunc>(std::move(bf), v)
		, std::conditional_t< has_block_func_v < TBinaryFunc>, c2vp_block_func_provider<TBinaryFunc>, no_block_func>(bf, v)
	{}
};

template <typename TBinaryFunc>
struct is_safe_for_undefines<composition_2_v_p<TBinaryFunc> > : is_safe_for_undefines<TBinaryFunc> {};

// ====================== composition_2_p_v(arg1) = TBinFunc(arg1, value1)

template<typename TBinaryFunc>
struct composition_2_p_v_base 
	:	unary_func<
			typename TBinaryFunc::res_type, 
			typename TBinaryFunc::arg1_type
		> 
{
	composition_2_p_v_base(TBinaryFunc bf, typename TBinaryFunc::arg2_cref v)
		:	m_BinFunc(bf)
		,	m_Value(v) 
	{}

	typename composition_2_p_v_base::res_type operator()(typename composition_2_p_v_base::arg1_cref arg) const
	{
		return m_BinFunc(arg, m_Value);
	}

	         TBinaryFunc            m_BinFunc;
	typename TBinaryFunc::arg2_cref m_Value;
};

template<typename TBinaryFunc>
struct c2pv_block_func_provider 
{
	using block_func = composition_2_p_v_base<typename TBinaryFunc::block_func>;
	block_func m_BlockFunc;

	c2pv_block_func_provider(TBinaryFunc& bf, typename TBinaryFunc::arg2_cref v)
		: m_BlockFunc(bf.m_BlockFunc, v* unit_block< nrbits_of_v<typename TBinaryFunc::arg2_type> >::value)
	{}
};

template<typename TBinaryFunc>
struct composition_2_p_v
	:	composition_2_p_v_base<TBinaryFunc>
	,	std::conditional_t<has_block_func_v<TBinaryFunc>, c2pv_block_func_provider<TBinaryFunc>, no_block_func>
{
	composition_2_p_v(TBinaryFunc bf, typename TBinaryFunc::arg2_cref v)
		: composition_2_p_v_base<TBinaryFunc>(std::move(bf), v)
		, std::conditional_t<has_block_func_v<TBinaryFunc>, c2pv_block_func_provider<TBinaryFunc>, no_block_func>(bf, v)
	{}
};

template <typename TBinaryFunc>
struct is_safe_for_undefines<composition_2_p_v<TBinaryFunc> > : is_safe_for_undefines<TBinaryFunc> {};

#endif // !defined(__CLC_COMPOSITION_H)