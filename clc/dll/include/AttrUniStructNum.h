// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_ATTRUNISTRUCTNUM_H)
#define __CLC_ATTRUNISTRUCTNUM_H

#include "AttrUniStruct.h"

// *****************************************************************************
//							UNARY NUMERIC FUNCTORS
// *****************************************************************************

template<typename T>	
struct sin_func: unary_func<div_type_t<T>, T >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<div_type_t<T>>(); }

	typename sin_func::res_type operator()(typename sin_func::arg1_cref arg) const { return std::sin(arg); }
};

template<typename T>	
struct cos_func: unary_func<div_type_t<T>, T >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<div_type_t<T>>(); }

	typename cos_func::res_type operator()(typename cos_func::arg1_cref arg) const { return std::cos(arg); }
};

template<typename T>	
struct tan_func: unary_func<div_type_t<T>, T >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<div_type_t<T>>(); }

	typename tan_func::res_type operator()(typename tan_func::arg1_cref arg) const { return std::tan(arg); }
};

template<typename T>	
struct atan_func: unary_func<div_type_t<T>, T >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<div_type_t<T>>(); }

	typename atan_func::res_type operator()(typename atan_func::arg1_cref arg) const { return std::atan(arg); }
};

template <typename T>
struct sqrx_func: unary_func<acc_type_t<T>, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return square_unit_creator(gr, args); }

	typename sqrx_func::res_type operator()(typename sqrx_func::arg1_cref arg) const { return sqrx_func::res_type(arg) * sqrx_func::res_type(arg); }
};

template<typename T>
struct neg_func_unchecked : unary_func<signed_type_t<T>, T> 
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return cast_unit_creator<typename neg_func_unchecked::res_type>(args); }

	auto operator()(cref_t<T> x) const { return -signed_type_t<T>(x); }
};

// *****************************************************************************
//							CHECKED ELEMENTARY UNARY FUNCTORS
// *****************************************************************************

template <typename T> 
struct exp_func_checked: unary_func<product_type_t<T>, T>
{
	typename exp_func_checked::res_type operator()(typename exp_func_checked::arg1_cref arg) const
	{ 
		static const T s_MaxExp = log(MAX_VALUE(exp_func_checked::res_type));
		if (arg >= s_MaxExp || arg == UNDEFINED_VALUE(T))
			return UNDEFINED_VALUE(typename exp_func_checked::res_type);
		return std::exp(typename exp_func_checked::res_type(arg));
	}

	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
	{
		return default_unit_creator_and_check_input< typename product_type<T>::type >(gr, args);
	}
};

template <typename T> 
struct log_func_checked: unary_func<typename product_type<T>::type, T>
{
	typename log_func_checked::res_type operator()(typename log_func_checked::arg1_cref arg) const
	{ 
		return (arg > T() && 
		        arg != UNDEFINED_VALUE(T))
			?	std::log(arg)
			:	UNDEFINED_VALUE(typename log_func_checked::res_type);
	}

	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) 
	{ 
		return default_unit_creator_and_check_input< typename product_type<T>::type >(gr, args);
	}
};

template <typename res_type, typename V> 
res_type sqrt_func_checked_f(const V& arg)
{
	return (IsDefined(arg) && !IsNegative(arg) )
		?	std::sqrt( res_type(arg) )
		:	UNDEFINED_VALUE(res_type);
}

template <typename res_type, typename T> 
res_type sqrt_func_checked_f(const Point<T>& arg)
{
	return res_type(
		sqrt_func_checked_f<typename scalar_of<res_type>::type>(arg.first), 
		sqrt_func_checked_f<typename scalar_of<res_type>::type>(arg.second)
	);
}

template <typename T> 
struct sqrt_func_checked: unary_func<div_type_t<T>, T>
{
	auto operator()(cref_t<T> arg) const
	{ 
		return sqrt_func_checked_f<div_type_t<T>>(arg);
	}
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return operated_unit_creator(gr, args); }
};


#endif //!defined(__CLC_ATTRUNISTRUCTNUM_H)