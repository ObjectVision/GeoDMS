// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__CLC_ATTRBINSTRUCT_H)
#define __CLC_ATTRBINSTRUCT_H

#include <cmath>
#include <functional>

#include "mci/ValueClass.h"
#include "utl/StrFormat.h"
#include "utl/StringFunc.h"

#include "Prototypes.h"
#include "UnitCreators.h"
#include "composition.h"
#include "AttrUniStruct.h"
#include "dms_transform.h"
#include "mci/ValueWrap.h"

// *****************************************************************************
//								CHECKED FUNCTORS
// *****************************************************************************

template <typename TBinFunc>
struct binary_func_checked_base: binary_func<typename TBinFunc::res_type, typename TBinFunc::arg1_type, typename TBinFunc::arg2_type>
{
	binary_func_checked_base(const TBinFunc& f = TBinFunc()) : m_BinFunc(f) {}

	typename binary_func_checked_base::res_type operator()(typename binary_func_checked_base::arg1_cref arg1, typename binary_func_checked_base::arg2_cref arg2) const
	{ 
		return (IsDefined(arg1) && IsDefined(arg2))
			?	m_BinFunc(arg1, arg2)
			:	UNDEFINED_OR_ZERO(typename binary_func_checked_base::res_type);
	}
private:
	TBinFunc m_BinFunc;
};

template <typename TBinFunc>
struct binary_func_checked: binary_func_checked_base<TBinFunc>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return TBinFunc::unit_creator(gr, args); }

	binary_func_checked(const TBinFunc& f = TBinFunc()) : binary_func_checked_base<TBinFunc>(f) {}
};

template <typename Func> struct is_safe_for_undefines< binary_func_checked<Func>> : std::true_type {};


// *****************************************************************************
//								do binary operators
// *****************************************************************************

template<typename ResSequence, typename Arg1Sequence, typename Arg2Sequence, typename BinaryOper>
void do_binary_func(
	ResSequence  resData,
	Arg1Sequence arg1Data,
	Arg2Sequence arg2Data,
	const BinaryOper&      oper,
	bool e1IsVoid, bool e2IsVoid,
	bool arg1HasMissingData,
	bool arg2HasMissingData)
{
	typedef typename cref<typename Arg1Sequence::value_type>::type arg1_cref;
	typedef typename cref<typename Arg2Sequence::value_type>::type arg2_cref;

	constexpr bool mustCheckUndefined = !is_safe_for_undefines<BinaryOper>::value 
		&& (has_undefines_v<typename Arg1Sequence::value_type> || has_undefines_v<typename Arg2Sequence::value_type>);

	if (e1IsVoid)
	{
		assert(arg1Data.size() == 1);

		if constexpr (mustCheckUndefined)
		{
			if (arg1HasMissingData && !IsDefined(arg1Data[0]))
			{
				fast_undefine(resData.begin(), resData.end());
				return;
			}
		}
		arg1_cref arg1Value = arg1Data[0];
		do_unary_func(
			resData
		,	arg2Data
		,	composition_2_v_p<BinaryOper>(oper, arg1Data[0]) // composition allows for optimizing SIMD
		,	arg2HasMissingData
		);
		return;
	}

	assert(arg1Data.size() == resData.size());

	if (e2IsVoid)
	{
		assert(arg2Data.size() == 1);

		if constexpr (mustCheckUndefined)
		{
			if (arg2HasMissingData && !IsDefined(arg2Data[0]))
			{
				fast_fill(resData.begin(), resData.end(), UNDEFINED_OR_ZERO(typename ResSequence::value_type));
				return;
			}
		}
		arg2_cref arg2Value = arg2Data[0];
		do_unary_func(
			resData
		,	arg1Data
		,	composition_2_p_v<BinaryOper>(oper, arg2Data[0])
		,	arg1HasMissingData
		);
		return;
	}

	assert(arg2Data.size() == resData.size());

	if constexpr (mustCheckUndefined)
	{
		if (arg1HasMissingData || arg2HasMissingData)
		{
			dms_transform(
				arg1Data.begin(), arg1Data.end(),
				arg2Data.begin(), resData.begin(),
				binary_func_checked_base<BinaryOper>(oper)
			);
			return;
		}
	}
	dms_transform(
		arg1Data.begin(), arg1Data.end(), 
		arg2Data.begin(), resData.begin(), 
		oper
	);
}

template <typename V >
struct safe_plus
{
	V operator ()(V a, V b) const
	{
		V result = a + b;
		if constexpr (!std::is_floating_point_v<V>)
		{
			if (!IsDefined(a))
				return UNDEFINED_VALUE(V);
			if (!IsDefined(b))
				return UNDEFINED_VALUE(V);
			if constexpr (!is_signed_v<V>)
			{
				if (result < a)
					throwOverflow("adding", a, "and", b, true, "add_or_null", NextAddIntegral<V>());
			}
			else
			{
				auto aNonnegative = (a >= 0);
				auto bNonnegative = (b >= 0);

				if (aNonnegative == bNonnegative)
				{
					auto resultNonnegative = (result >= 0);
					if (aNonnegative !=resultNonnegative)
						throwOverflow("adding", a, "and", b, true, "add_or_null", NextAddIntegral<V>());
				}
			}
		}
		return result;
	}
};

template <typename V>
struct safe_plus < Point<V> >
{
	Point<V> operator ()(Point<V> a, Point<V> b) const
	{
		return Point<V>( scalar_op(a.first, b.first), scalar_op(a.second, b.second) );
	}
	safe_plus<V> scalar_op;
};

template <typename T>
struct safe_minus
{
	T operator ()(T a, T b) const
	{
		T result = a - b;
		if constexpr (!std::is_floating_point_v<T>)
		{
			if (!IsDefined(a))
				return UNDEFINED_VALUE(T);
			if (!IsDefined(b))
				return UNDEFINED_VALUE(T);
			if constexpr (!is_signed_v<T>)
			{
				if (a < b)
					throwOverflow("subtracting", b, "from", a, true, "sub_or_null", NextSubIntegral<T>());
			}
			else
			{
				auto aNonnegative = (a >= 0);
				auto bNonnegative = (b >= 0);

				if (aNonnegative != bNonnegative)
				{
					auto resultNonnegative = (result >= 0);
					if (aNonnegative != resultNonnegative)
						throwOverflow("subtracting", b, "from", a, true, "sub_or_null", NextSubIntegral<T>());
				}
			}
		}
		return result;
	}
};

template <typename T>
struct safe_minus < Point<T> >
{
	Point<T> operator ()(Point<T> a, Point<T> b) const
	{
		return Point<T>( scalar_op(a.first, b.first), scalar_op(a.second, b.second));
	}
	safe_minus<T> scalar_op;
};

template <typename T>
struct safe_plus_or_null
{
	T operator ()(T a, T b) const
	{
		T result = a + b;
		if constexpr (!std::is_floating_point_v<T>)
		{
			if (!IsDefined(a))
				return UNDEFINED_VALUE(T);
			if (!IsDefined(b))
				return UNDEFINED_VALUE(T);
			if constexpr (!is_signed_v<T>)
			{
				if (result < a)
					return UNDEFINED_VALUE(T);
			}
			else
			{
				auto aNonnegative = (a >= 0);
				auto bNonnegative = (b >= 0);

				if (aNonnegative == bNonnegative)
				{
					auto resultNonnegative = (result >= 0);
					if (aNonnegative != resultNonnegative)
						return UNDEFINED_VALUE(T);
				}
			}
		}
		return result;
	}
};

template <typename T>
struct safe_plus_or_null < Point<T> >
{
	Point<T> operator ()(Point<T> a, Point<T> b) const
	{
		return Point<T>(scalar_op(a.first, b.first), scalar_op(a.second, b.second));
	}
	safe_plus_or_null<T> scalar_op;
};

template <typename T>
struct safe_minus_or_null
{
	T operator ()(T a, T b) const
	{
		T result = a - b;
		if constexpr (!std::is_floating_point_v<T>)
		{
			if (!IsDefined(a))
				return UNDEFINED_VALUE(T);
			if (!IsDefined(b))
				return UNDEFINED_VALUE(T);
			if constexpr (!is_signed_v<T>)
			{
				if (a < b)
					return UNDEFINED_VALUE(T);
			}
			else
			{
				auto aNonnegative = (a >= 0);
				auto bNonnegative = (b >= 0);

				if (aNonnegative != bNonnegative)
				{
					auto resultNonnegative = (result >= 0);
					if (aNonnegative != resultNonnegative)
						return UNDEFINED_VALUE(T);
				}
			}
		}
		return result;
	}
};

template <typename T>
struct safe_minus_or_null < Point<T> >
{
	Point<T> operator ()(Point<T> a, Point<T> b) const
	{
		return Point<T>(scalar_op(a.first, b.first), scalar_op(a.second, b.second));
	}
	safe_minus_or_null<T> scalar_op;
};


template <FixedSizeElement T> struct plus_func : std_binary_func< safe_plus<T>, T, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return  compatible_values_unit_creator_func(0, gr, args, false); }
};

template <FixedSizeElement T> struct minus_func: std_binary_func< safe_minus<T>, T, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return  compatible_values_unit_creator_func(0, gr, args, false); }
};

template <FixedSizeElement T> struct plus_or_null_func : std_binary_func< safe_plus_or_null<T>, T, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return  compatible_values_unit_creator_func(0, gr, args, false); }
};

template <FixedSizeElement T> struct minus_or_null_func : std_binary_func< safe_minus_or_null<T>, T, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return  compatible_values_unit_creator_func(0, gr, args, false); }
};

template <typename V> 
struct mul_func_impl  : binary_func<V, V, V>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mul2_unit_creator(gr, args); }

	V operator()(cref_t<V> a, cref_t<V> b) const
	{
		if constexpr (!std::is_floating_point_v<V> && has_undefines_v<V>)
		{
			if (!IsDefined(a) || !IsDefined(b))
				return UNDEFINED_VALUE(V);
		}

		if constexpr (std::is_floating_point_v<V>)
			return a * b;
		else
			return CheckedMul<V>(a, b, true);
	}
};

template <typename T> struct mul_func_impl<Point<T>> : binary_func<Point<T>, Point<T>, Point<T>>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mul2_unit_creator(gr, args); }

	Point<T> operator ()(Point<T> a, Point<T> b) const
	{
		return Point<T>(scalar_op(a.first, b.first), scalar_op(a.second, b.second));
	}
	mul_func_impl<T> scalar_op;
};


template <typename T> struct mul_func : mul_func_impl<T> {};

template <typename T> struct mul_or_null_func : binary_func<T, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mul2_unit_creator(gr, args); }

	T operator()(cref_t<T> a, cref_t<T> b) const
	{
		if constexpr (!std::is_floating_point_v<T> && has_undefines_v<T>)
		{
			if (!IsDefined(a) || !IsDefined(b))
				return UNDEFINED_VALUE(T);
		}

		T result = a * b;

		if constexpr (!std::is_floating_point_v<T>)
		{
			if (a && b && b != result / a)
				return UNDEFINED_VALUE(T);
		}

		return result;
	}
};

template <typename T> struct mul_or_null_func< Point<T>> : binary_func<Point<T>, Point<T>, Point<T>>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mul2_unit_creator(gr, args); }

	Point<T> operator ()(Point<T> a, Point<T> b) const
	{
		return Point<T>(scalar_op(a.first, b.first), scalar_op(a.second, b.second));
	}
	mul_or_null_func<T> scalar_op;
};

template <FixedSizeElement T>
struct mulx_func : binary_func<typename acc_type<T>::type, T, T>
{
	typename mulx_func::res_type operator()(typename mulx_func::arg1_cref a1, typename mulx_func::arg2_cref a2) const 
	{ 
		return typename mulx_func::res_type(a1) * typename mulx_func::res_type(a2); 
	}
};

template <>
struct mulx_func<UInt64> : binary_func<UInt64, UInt64, UInt64>
{
	typename mulx_func::res_type operator()(UInt64 a1, UInt64 a2) const
	{
		return CheckedMul(a1, a2, false);
	}
};

template <>
struct mulx_func<Int64> : binary_func<Int64, Int64, Int64>
{
	typename mulx_func::res_type operator()(Int64 a1, Int64 a2) const
	{
		return CheckedMul(a1, a2, false);
	}
};

template <typename R, typename T, typename U=T>
struct div_func_base: binary_func<R, T, U>
{
	typename div_func_base::res_type operator()(typename div_func_base::arg1_cref t1, typename div_func_base::arg2_cref t2) const
	{ 
		assert(t1 != 0 || t1 == 0); // validates that T is a scalar
		if (t2 == 0) // validates that U is a scalar 
			return UNDEFINED_VALUE(typename div_func_base::res_type);
		if constexpr (!std::is_floating_point_v<T> && has_undefines_v<T>)
		{
			if (!IsDefined(t1))
				return UNDEFINED_VALUE(typename div_func_base::res_type);
		}
		if constexpr (!std::is_floating_point_v<U> && has_undefines_v<U>)
		{
			if (!IsDefined(t2))
				return UNDEFINED_VALUE(typename div_func_base::res_type);
		}
		return Convert<typename div_func_base::res_type>(t1) / Convert<typename div_func_base::res_type>(t2);
	}
};

template <typename R, typename T, typename U = T> struct div_func_best : div_func_base<R, T, U> {};

template <typename R, typename T, typename U>
struct div_func_best<R, Point<T>, U>
	:	binary_func<R, Point<T>, U >
{
	using quotient_type = div_type_t<T>;
	using point_ref_type = cref<Point<T> >::type;
	using base_func = div_func_best<quotient_type, U>;

	Point<quotient_type> operator()(point_ref_type t1, typename base_func::arg2_cref t2) const
	{
		return Point<quotient_type>(m_BaseFunc(t1.first, t2), m_BaseFunc(t1.second, t2) );
	}

	base_func m_BaseFunc;
};

template <typename R, typename T, typename U>
struct div_func_best<R, Point<T>, Point<U> >
	: binary_func<R, Point<T>, Point<U> >
{
	using quotient_type = scalar_of_t<R>;
	using point1_ref_type = typename cref<Point<T> >::type;
	using point2_ref_type = typename cref<Point<U> >::type;
	using base_func = div_func_base<quotient_type, T, U>;

	Point<quotient_type> operator()(point1_ref_type t1, point2_ref_type t2) const
	{
		return Point<quotient_type>(m_BaseFunc(t1.first, t2.first), m_BaseFunc(t1.second, t2.second) );
	}

	base_func m_BaseFunc;
};

template <typename T> struct div_func : div_func_best<T, T, T> 
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return div_unit_creator(gr, args); }
};

template<typename T> struct is_safe_for_undefines<plus_func<T>> : std::true_type {};
template<typename T> struct is_safe_for_undefines<minus_func<T>> : std::true_type {};
template<typename T> struct is_safe_for_undefines<mul_func<T>> : std::true_type {};
template<typename T> struct is_safe_for_undefines<div_func<T> > : std::true_type {};

template <typename T> struct qint_t          { typedef Int64 type; };
template <>           struct qint_t<Float32> { typedef Int32 type; };

template <typename T> 
T mod_func_impl(const T& counter, const T& divider)
	requires(std::is_integral_v<T>)
{
	return counter % divider;
}

template <typename T>
T mod_func_impl(const T& counter, const T& divider)
	requires(std::is_floating_point_v<T>)
{
	typename qint_t<T>::type quotient = counter / divider;
	return counter - quotient * divider;
}

template <typename T>
Point<T>
mod_func_impl(const Point<T>& counter, const Point<T>& divider)
{
	return Point<T>(
		mod_func_impl(counter.first , divider.first )
	,	mod_func_impl(counter.second, divider.second)
	);
}

template <typename T> struct mod_func: binary_func<T, T, T >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return div_unit_creator(gr, args); }

	typename mod_func::res_type operator()(typename mod_func::arg1_cref t1, typename mod_func::arg2_cref t2) const
	{
		return (t2 != T())
			?	mod_func_impl(t1, t2)
			:	UNDEFINED_VALUE(typename mod_func::res_type);
	}
};

// *****************************************************************************
//										pow (a^b) as a first-class operator (issue #839)
// *****************************************************************************
//
// pow(base, exponent) with floating-point base T and an exponent that is either
// the same float type or any integer type U. This replaces the old expression
// rewrite a^b -> exp(b*log(a)), which yielded Null for any non-positive base
// (e.g. 0f^2f -> Null) because log() is undefined there. Following eoudejans'
// rules in issue #839:
//   - integer-valued exponent: exact result by repeated multiplication, so the
//     sign of a negative base is honoured (odd power keeps the sign, even power
//     drops it) and small powers (^2..^6) stay exact;
//   - 0^positive = 0; 0^(<=0) = Null;
//   - negative base with a non-integer exponent = Null (was the log() trap);
//   - positive base with a non-integer exponent: std::pow (= exp(b*log(a))).

template <typename T>
inline T pow_int_exponent(T base, Int64 n)
{
	bool neg = (n < 0);
	UInt64 m = neg ? (UInt64(0) - UInt64(n)) : UInt64(n); // two's-complement abs, safe for INT64_MIN
	T result = T(1);
	T b = base;
	while (m)
	{
		if (m & UInt64(1))
			result *= b;
		m >>= 1;
		if (m)
			b *= b;
	}
	if (neg)
	{
		if (result == T(0))
			return UNDEFINED_VALUE(T); // 1/0 : a non-positive base raised to a negative power
		result = T(1) / result;
	}
	return std::isfinite(result) ? result : UNDEFINED_VALUE(T); // overflow -> Null
}

template <typename T>
inline T pow_float_exponent(T base, T expo)
{
	if (expo == std::floor(expo) && std::fabs(expo) < T(1e18))
		return pow_int_exponent<T>(base, Int64(expo)); // whole-number exponent: exact, sign-correct

	// genuine non-integer exponent
	if (base < T(0))
		return UNDEFINED_VALUE(T);                       // would be exp(b*log(neg)) -> Null
	if (base == T(0))
		return (expo > T(0)) ? T(0) : UNDEFINED_VALUE(T); // 0^positive = 0, else Null
	T r = std::pow(base, expo);
	return std::isfinite(r) ? r : UNDEFINED_VALUE(T);
}

// T: floating-point base type; U: exponent type (the same float type, or any integer type)
template <typename T, typename U>
struct pow_func : binary_func<T, T, U>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
	{
		return default_unit_creator_and_check_input<T>(gr, args); // base and exponent must be metric-free
	}

	T operator()(typename pow_func::arg1_cref base, typename pow_func::arg2_cref expo) const
	{
		// undefined args are filtered by do_binary_func before we get here
		if constexpr (std::is_integral_v<U>)
			return pow_int_exponent<T>(base, Int64(expo));
		else
			return pow_float_exponent<T>(base, expo);
	}
};

// *****************************************************************************
//										binary string funcs
// *****************************************************************************

struct strpos_func: binary_func<UInt32, SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<UInt32>(); }

	UInt32 operator ()(typename strpos_func::arg1_cref arg1, typename strpos_func::arg2_cref arg2) const
	{
		arg1_cref::const_iterator p = std::search(arg1.begin(), arg1.end(), arg2.begin(), arg2.end());
		return (p == arg1.end())
			? UNDEFINED_VALUE(UInt32)
			: p - arg1.begin();
	}
};

struct strrpos_func : binary_func<UInt32, SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<UInt32>(); }

	UInt32 operator ()(typename strrpos_func::arg1_cref arg1, typename strrpos_func::arg2_cref arg2) const
	{
		auto p = std::find_end(arg1.begin(), arg1.end(), arg2.begin(), arg2.end());
		return (p == arg1.end())
			? UNDEFINED_VALUE(UInt32)
			: p - arg1.begin();
	}
};

struct strcount_func: binary_func<UInt32, SharedStr, SharedStr>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<UInt32>(); }

	UInt32 operator ()(cref_t<SharedStr> arg1, cref_t<SharedStr> arg2) const
	{
		return StrCount(arg1, arg2);
	}
};


#endif //!defined(__CLC_ATTRBINSTRUCT_H)