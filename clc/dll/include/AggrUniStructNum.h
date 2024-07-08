// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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


inline void Assign(FPoint& lhs, const DPoint& rhs)
{
	lhs.first = rhs.first;
	lhs.second = rhs.second;
}

template <typename T>
struct null_wrap : private std::pair<T, bool>
{
	null_wrap()
	{
		assert(!IsDefined());
	}
	null_wrap(null_wrap<T>&& rhs) noexcept
	{
		Assign(this->first, std::move(rhs.first));
		this->second = rhs.second;
	}
	null_wrap(const null_wrap<T>& rhs)
	{
		Assign(this->first, rhs.first);
		this->second = rhs.second;
	}
	bool IsDefined() const { return this->second; };

	auto value() const -> const T&
	{ 
		assert(IsDefined());
		return this->first; 
	}

	void operator =(const null_wrap<T>& rhs)
	{
		Assign(this->first, rhs.first);
		this->second = rhs.second;
	}

	void operator =(null_wrap<T>&& rhs) noexcept
	{
		Assign(this->first, std::move(rhs.first));
		this->second = rhs.second;
	}
	void AssignValue(const T& rhs)
	{
		Assign(this->first, rhs);
		this->second = true;
	}
	void AssignValue(T&& rhs)
	{
		Assign(this->first, rhs);
		this->second = true;
	}
	template <typename T>
	void operator =(const T&& rhs)
	{
		AssignValue(std::forward<T>(rhs));
	}
	      T* operator ->()       { assert(IsDefined()); return &this->first; }
	const T* operator ->() const { assert(IsDefined()); return &this->first; }
};

template <typename T>
inline constexpr bool IsDefined(const null_wrap<T>& v) { return v.IsDefined(); }

template <typename T> struct DataArrayBase<null_wrap<T> > { typename T::dont_instantiate_this x; }; // DEBUG

template <typename T>
null_wrap<T> UndefinedValue(const null_wrap<T>*)
{
	return null_wrap<T>();
}

template<typename T>
void MakeUndefined(null_wrap<T>& output)
{
	output = null_wrap<T>();
}

template<typename T>
inline void Assign(SA_Reference<T> lhs, const null_wrap<std::vector<T>>& rhs)
{
	if (rhs.IsDefined())
		lhs.assign(begin_ptr(rhs.value()), end_ptr(rhs.value()));
	else
		lhs.assign(Undefined());
}

inline void Assign(StringRef lhs, const null_wrap<SharedStr>& rhs)
{
	if (rhs.IsDefined())
		lhs.assign(rhs->begin(), rhs->send());
	else
		lhs.assign(Undefined());
}

template <bit_size_t N, typename Block, typename BV>
inline void Assign(bit_reference<N, Block> lhs, const null_wrap<BV>& rhs)
{
	if (rhs.IsDefined())
		lhs = rhs.value();
	else
		lhs = 0;
}

template<typename T>
void Assign(null_wrap<T>& output, const null_wrap<T>& rhs)
{
	output = rhs;
}

template<typename T>
void Assign(null_wrap<T>& output, null_wrap<T>&& rhs)
{
	output = std::move(rhs);
}

template<typename T, typename U>
void Assign(null_wrap<T>& output, U&& rhs) requires (!is_null_wrap_v<std::remove_cvref_t<U>>)
{
	if constexpr (can_be_undefined_v<std::remove_cvref_t<U>>)
	{
		if (!IsDefined(rhs))
		{
			MakeUndefined(output);
			return;
		}
	}
	output.AssignValue(rhs);
}

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
	void CombineValues(I& a, I rhs) const
	{
		::SafeAccumulate(a, rhs);
	}
	void CombineRefs(sequence_traits<I>::reference a, sequence_traits<I>::const_reference rhs) const
	{
		::SafeAccumulate(a, rhs);
	}
};

template <typename T, typename I> 
struct count_partial_best: unary_assign_partial_accumulation<unary_assign_inc<T, I> >
{};

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

	using accumulator_type = nullable_t<T>;
	accumulator_type InitialValue() const
	{
		return UNDEFINED_VALUE(accumulator_type);
	}

	void operator()(auto&& output, typename first_total_best::value_cseq1 input) const
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
				Assign(output, *i);
				break;
			}
		}	
	}
	void CombineValues(nullable_t<T>& accumulator, const nullable_t<T>& rhs) const
	{
		if (IsDefined(accumulator))
			return;
		if (!IsDefined(rhs))
			return;
		Assign(accumulator, rhs);
	}
};

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

	using accumulator_type = nullable_t<T>;
	accumulator_type InitialValue() const
	{
		return UNDEFINED_VALUE(accumulator_type);
	}

	void operator()(auto&& output, typename base_type::value_cseq1 input) const
	{ 
		auto
			i = input.end(),
			b = input.begin();
		while  (i!=b)
		{
			if (IsDefined(*--i))
			{
				Assign(output, *i);
				break;
			}
		}
	}
	template <typename T>
	void CombineValues(T& accumulator, T rhs) const
	{
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(rhs))
				return;
		accumulator = rhs;
	}
};

template <typename OR, typename T>
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


template <typename OR, typename T>
struct last_partial_best_direct_base : unary_assign_partial_accumulation<unary_assign_overwrite<OR, T>, assign_null_value<OR> >
{};

template <typename T>
struct last_partial_best_buffered_base
	: last_partial_best_direct_base<null_wrap<T>, T>
	, assign_partial_output_from_buffer< assign_output_direct<T> >
{};

template <typename T>
struct last_partial_best : std::conditional_t<has_undefines_v<T>, last_partial_best_direct_base<T, T>, last_partial_best_buffered_base<T> >
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
	using accu_type = expectation_accumulation_type<typename TUniFunc::res_type>;
	void CombineValues(accu_type& a, const accu_type& rhs) const
	{
		SafeAccumulate(a.n, rhs.n);
		SafeAccumulate(a.total, rhs.total);
	}
	void CombineRefs(accu_type& a, const accu_type& rhs) const
	{
		CombineValues(a, rhs);
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
	void CombineValues(var_accumulation_type<T>& a, const var_accumulation_type<T>& rhs) const
	{
		SafeAccumulate(a.n, rhs.n);
		SafeAccumulate(a.x, rhs.x);
		SafeAccumulate(a.xx, rhs.xx);
	}
	void CombineRefs(var_accumulation_type<T>& a, const var_accumulation_type<T>& rhs) const
	{
		CombineValues(a, rhs);
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
	using dms_result_type = T;
	using var_type = var_accumulation_type<T>::var_type;

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
};

struct all_partial: unary_assign_partial_accumulation<unary_assign_all, nullary_set_true > 
{
//	typedef Bool dms_result_type;
};

/*****************************************************************************/


#endif //!defined(__CLC_AGGRUNISTRUCTNUM_H)
