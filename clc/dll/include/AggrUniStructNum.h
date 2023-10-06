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

#if !defined(__CLC_AGGRUNISTRUCTNUM_H)
#define __CLC_AGGRUNISTRUCTNUM_H

#include "AttrUniStructNum.h"
#include "AggrUniStruct.h"
#include "AggrFuncNum.h"
#include "UnitCreators.h"
#include "composition.h"

#include "geo/Undefined.h"
#include <boost/mpl/identity.hpp>

// MOVE TO RTC

extern CommonOperGroup cog_Min;
extern CommonOperGroup cog_Max;
extern CommonOperGroup cog_First;
extern CommonOperGroup cog_Last;

template <typename T>
struct null_wrap : private std::pair<T, bool>
{
	null_wrap()
	{
		assert(!IsDefined());
	}
	bool IsDefined() const { return this->second; };
	operator typename param_type<T>::type () const { return this->first; }

	void operator =(typename param_type<T>::type rhs)
	{
		this->first  = rhs;
		this->second = true;
	}
};

template <typename T> struct DataArrayBase<null_wrap<T> > { typename T::dont_instantiate_this x; }; // DEBUG

template <typename T>
null_wrap<T> UndefinedValue(const null_wrap<T>*)
{
	return null_wrap<T>();
}

template <typename T>
inline bool IsDefined(const null_wrap<T>& v)
{
	return v.IsDefined();
}

template <typename T>
using nullable_t = std::conditional_t<has_undefines_v<T>, T, null_wrap<T>>;

// END MOVE

/*****************************************************************************/
//											COUNT
/*****************************************************************************/

// don't use unary_assign_total_accumulation since count_best_total can to better when hasUndefinedValues is false.

template <typename T, typename I> 
struct count_total_best
	:	initializer< assign_default<I>  >
	,	unary_total_accumulation<I, T>
	,	assign_output_direct<I>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(count_unit_creator(args)); }

	void operator()(typename count_total_best::assignee_ref output, typename count_total_best::value_cseq1 input) const
	{ 
		count_best_total(output, input.begin(), input.end());
	}
};

template <typename T> struct count_total_best_UInt32 : count_total_best<T, UInt32> {};

template <typename T, typename I> 
struct count_partial_best: unary_assign_partial_accumulation<unary_assign_inc<T, I> >
{};

template <typename T> struct count_partial_best_UInt32 : count_partial_best<T, UInt32> {};

/*****************************************************************************/
//											SUM data
/*****************************************************************************/

template <typename T> struct sum_elem_type : std::conditional<std::is_floating_point_v<T>, Float64, T> {};

template <typename T> struct sum_type : sum_elem_type<T> {};
template <typename T> struct sum_type<Range<T> > { using type = Range<typename sum_type<T>::type>; };
template <typename T> struct sum_type<Point<T> > { using type = Point<typename sum_type<T>::type>; };
//template <typename T> struct sum_type<const T> {}; // illegal use

template <typename T> using sum_type_t = typename sum_type<T>::type;


template <typename Res, typename Acc, typename T>
struct sum_total_in
	: unary_assign_total_accumulation<unary_assign_add<Acc, T> >
	, assign_output_convert<Res, Acc>
{};


template <typename Res, typename Acc, typename T>
struct sum_partial_in : unary_assign_partial_accumulation<unary_assign_add<Acc, T> >, assign_partial_output_from_buffer< assign_output_direct<Res> >
{};

template <typename Res, typename Acc>
struct sum_func_generator
{
	template <typename T> struct total : sum_total_in<Res, Acc, T> {};
	template <typename T> struct partial : sum_partial_in<Res, Acc, T> 
	{
		static_assert(std::is_same_v<typename partial::result_type, Acc>);
	};
};

template <typename Res>
struct specified_sum_func_generator : sum_func_generator<Res, Res> 
{};

template <typename T> using sum_total_best = typename sum_func_generator<T, sum_type_t<T>>::template total<T>;
template <typename T> using sum_partial_best = typename sum_func_generator<T, sum_type_t<T>>::template partial<T>;

/*****************************************************************************/
//									MAX
/*****************************************************************************/

template<typename T>
struct max_total_best : unary_assign_total_accumulation<unary_assign_max<T>, assign_min_value<T> >, assign_output_direct<T>
{};

template <typename T>
struct max_partial_best: unary_assign_partial_accumulation<unary_assign_max<T>, assign_min_value<T> > //, assign_partial_output_direct<T>
{};

/*****************************************************************************/
//									MIN
/*****************************************************************************/

template<typename T>
struct min_total_best : unary_assign_total_accumulation<unary_assign_min<T>, assign_max_value<T> >, assign_output_direct<T>
{};

template <typename T>
struct min_partial_best: unary_assign_partial_accumulation<unary_assign_min<T>, assign_max_value<T> > //, assign_partial_output_direct<T>
{};

/*****************************************************************************/
//									FIRST
/*****************************************************************************/

template<typename T>
struct first_total_best
	:	unary_total_accumulation<nullable_t<T>, T>
	,	assign_output_direct<T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }

	void Init(typename first_total_best::assignee_ref output) const
	{
		MakeUndefined(output);
	}

	void operator()(typename first_total_best::assignee_ref output, typename first_total_best::value_cseq1 input) const
	{ 
		if (IsDefined(output))
			return;
		auto
			i = input.begin(),
			e = input.end();
		for (; i != e; ++i)
		{
			if (IsDefined(*i))
			{
				output = *i;
				break;
			}
		}	
	}
};

template <typename OR,  typename T>
struct first_partial_best_direct_base : unary_assign_partial_accumulation<unary_assign_once<OR, T>, assign_null_value<OR> >
{};

template <typename T>
struct first_partial_best_buffered_base 
	: first_partial_best_direct_base<null_wrap<T>, T>
	, assign_partial_output_from_buffer< assign_output_direct<T> >
{};

template <typename T>
struct first_partial_best : std::conditional_t<has_undefines_v<T>, first_partial_best_direct_base<T, T>, first_partial_best_buffered_base<T> >
{};

/*****************************************************************************/
//									LAST
/*****************************************************************************/

template<typename T>
struct last_total_best
	:	unary_total_accumulation<T, T>
	,	assign_output_direct<T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }

	using base_type = unary_total_accumulation<T, T>;
	void Init(typename base_type::assignee_ref output) const
	{
		output = UNDEFINED_OR_ZERO(T);
	}

	void operator()(typename base_type::assignee_ref output, typename base_type::value_cseq1 input) const
	{ 
		auto
			i = input.end(),
			b = input.begin();
		while  (i!=b)
		{
			if (IsDefined(*--i))
			{
				output = *i;
				break;
			}
		}
	}
};

template <typename T>
struct last_partial_best
	:	unary_assign_partial_accumulation<unary_assign_overwrite<T>, assign_null_or_zero<T> >
{};

/*****************************************************************************/
//								EXPECTATION and MEAN
/*****************************************************************************/

template <typename T>
struct expectation_accumulation_type
{
	typedef typename SizeT              count_type;
	typedef typename acc_type<T>::type  sum_type;
	typedef typename aggr_type<T>::type exp_type;
	expectation_accumulation_type(): n(), total() {}

	operator exp_type() const
	{
		return div_func_best<div_type_t<T>, sum_type, count_type>()(total, n);
	}

	count_type n;
	sum_type   total;
};


template<typename TUniFunc>
struct unary_assign_exp: unary_assign<expectation_accumulation_type<typename TUniFunc::res_type>, typename TUniFunc::arg1_type>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return  TUniFunc::template unit_creator<R>(gr, args); }

	unary_assign_exp(TUniFunc&& f = TUniFunc()) : m_Func(std::forward<TUniFunc>(f)) {}

	void operator () (typename unary_assign_exp::assignee_ref a, typename unary_assign_exp::arg1_cref x) const
	{
		if constexpr (has_undefines_v<typename unary_assign_exp::arg1_type>)
			if (!IsDefined(x))
				return;

		++a.n;
		SafeAccumulate(a.total, m_Func(x));
	}

private:
	TUniFunc m_Func;
};

template<typename TUniFunc>
struct exp_total_best: unary_assign_total_accumulation<unary_assign_exp<TUniFunc> >
{
	exp_total_best(TUniFunc&& uniFunc = TUniFunc())
		:	unary_assign_total_accumulation<unary_assign_exp<TUniFunc> >( unary_assign_exp<TUniFunc>(std::forward<TUniFunc>(uniFunc) ) )
	{}
};

template<typename T>
struct mean_total_best : exp_total_best< ident_function<T> >, assign_output_convert<T, typename aggr_type<T>::type>
{};

template<typename TUniFunc>
struct exp_partial_best: unary_assign_partial_accumulation<unary_assign_exp<TUniFunc> >
{
	exp_partial_best(TUniFunc&& uniFunc = TUniFunc())
		:	unary_assign_partial_accumulation<unary_assign_exp<TUniFunc> >( unary_assign_exp<TUniFunc>(std::forward<TUniFunc>(uniFunc) ) )
	{}
};

template <typename T>
struct mean_partial_best : exp_partial_best< ident_function<T> >, assign_partial_output_from_buffer< assign_output_convert<T, typename aggr_type<T>::type> >
{};

/*****************************************************************************/
//											VARIANCE
/*****************************************************************************/

#include "AttrUniStruct.h"

template <typename T> 
struct var_accumulation_type
{
	typedef typename SizeT                     count_type;
	typedef typename acc_type<T>::type         sum_type;
	typedef typename aggr_type<sum_type>::type var_type;
	typedef typename scalar_of<var_type>::type var_scalar_type;

	var_accumulation_type() {}
	var_accumulation_type(count_type n_, sum_type x_, sum_type xx_): n(n_), x(x_), xx(xx_) {}

	operator var_type() const
	{
		if (!n)
			return UNDEFINED_VALUE(var_type);

		var_type exp_x  =  x;
		var_type exp_xx = xx;
		exp_x /= var_scalar_type(n);
		exp_xx /= var_scalar_type(n);

		var_type res = exp_xx - exp_x * exp_x;
		MakeUpperBound(res, var_type());
		return res;
	}

	count_type  n = 0;
	sum_type    x = sum_type(), xx = sum_type();
};

template <typename T>
struct unary_assign_var: unary_assign<var_accumulation_type<T>, T>
{
	template<typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(square_unit_creator(gr, args)); }

	void operator () (typename unary_assign_var::assignee_ref a, typename unary_assign_var::arg1_cref x) const
	{
		if (IsDefined(x))
		{
			++a.n;
			SafeAccumulate(a.x, x);
			a.xx += m_SqrFunc(x);
		}
	}

private:
	sqrx_func<T> m_SqrFunc;
};

template <typename T> 
struct var_total_best
	:	unary_assign_total_accumulation<unary_assign_var<T> >
	,	assign_output_convert<T, typename var_accumulation_type<T>::var_type>
{};

template <typename T> 
struct var_partial_best
	:	unary_assign_partial_accumulation<unary_assign_var<T> >
	,	assign_partial_output_from_buffer< assign_output_convert<T, typename var_accumulation_type<T>::var_type> >
{};

/*****************************************************************************/
//											STANDARD_DEVIATION
/*****************************************************************************/

template <typename T>
struct assign_output_convert_std
{
	typedef T dms_result_type;
	typedef typename var_accumulation_type<T>::var_type var_type;

	void AssignOutput(T& res, const var_accumulation_type<T>& buf) const
	{
		res = Convert<T>(m_Sqrt(buf));
	}

	sqrt_func_checked<var_type>	m_Sqrt;
};


template <typename T> 
struct std_total_best : unary_assign_total_accumulation<unary_assign_var<T> >, assign_output_convert_std<T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mean_total_best<T>::template unit_creator<R>(gr, args); }
};

template <typename T> 
struct std_partial_best
	:	unary_assign_partial_accumulation<unary_assign_var<T> >
	,	assign_partial_output_from_buffer< assign_output_convert_std<T> >
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mean_total_best<T>::template unit_creator<R>(gr, args); }
};

/*****************************************************************************/
//    any / all boolean quantoren
/*****************************************************************************/

struct any_total
	:	unary_assign_total_accumulation<unary_assign_any, nullary_set_false> 
	,	assign_output_direct<Bool>
{};

struct all_total
	:	unary_assign_total_accumulation<unary_assign_all, nullary_set_true> 
	,	assign_output_direct<Bool>
{};

struct any_partial: unary_assign_partial_accumulation<unary_assign_any, nullary_set_false > 
{
//	typedef Bool dms_result_type;
};

struct all_partial: unary_assign_partial_accumulation<unary_assign_all, nullary_set_true > 
{
//	typedef Bool dms_result_type;
};

/*****************************************************************************/


#endif //!defined(__CLC_AGGRUNISTRUCTNUM_H)
