// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__CLC_AGGRFUNCNUM_H)
#define __CLC_AGGRFUNCNUM_H

#include <vector>

#include "geo/BaseBounds.h"

#include "AggrFunc.h"
#include "composition.h"
#include "Prototypes.h"
#include "UnitCreators.h"
#include "AttrBinStruct.h"


// *****************************************************************************
//								ELEMENTARY ASSIGNMENT FUNCTORS 
// *****************************************************************************

template<typename R> void SafeIncrement(R& assignee) // see the similarity with safe_plus
{
	R orgAssignee = assignee; // may-be used in check
	if constexpr (has_undefines_v<R>)
	{
		if (!IsDefined(assignee))
			return;
	}

	if constexpr (!std::is_floating_point_v<R>)
	{
		if constexpr (is_signed_v<R>)
		{
			if (assignee == MAX_VALUE(R))
				throwErrorF("SafeIncrement", "non-representable numerical overflow when incrementing %s", assignee);
		}
	}

	++assignee;

	if constexpr (!std::is_floating_point_v<R>)
	{
		if constexpr (!has_undefines_v<R>)
		{
			if (assignee == R())
				throwErrorF("SafeIncrement", "non-representable numerical overflow of sub-byte value", assignee);
		}
		else
		{
			if (!IsDefined(assignee))
				throwErrorF("SafeIncrement", "non-representable numerical overflow", assignee);
		}
	}
}

template <typename T, typename I>
struct unary_assign_inc : unary_assign<I, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<R>(); }

	void operator()(vref_t<I> assignee, cref_t<T> arg) const
	{ 
		static_assert(!std::is_floating_point_v<I>);
		if constexpr (has_undefines_v<T>)
		{
			if (!IsDefined(arg))
				return;
		}
		SafeIncrement<I>(assignee);
	}
	void CombineRefs(vref_t<I> assignee, cref_t<I> rhs) const
	{
		if constexpr (has_undefines_v<I>)
		{
			assert(IsDefined(assignee));
			assert(IsDefined(rhs));
		}
		SafeAccumulate<I>(assignee, rhs);
	}
};

template<typename R, typename T> void SafeAccumulate(R& assignee, T arg) // see the similarity with safe_plus
{
	if constexpr (has_undefines_v<R>)
		if (!IsDefined(assignee))
			return;
	if constexpr (has_undefines_v<T>)
		if (!IsDefined(arg))
			return;

	R convertedArg = ThrowingConvert<R>(arg);
	if constexpr (is_signed_v<R> && !std::is_floating_point_v<R>)
	{
		static_assert(UNDEFINED_VALUE(R) == std::numeric_limits<R>::min());
		if constexpr (is_signed_v<T>)
		{
			if (convertedArg >= 0)
			{
				if (assignee > std::numeric_limits<R>::max() - convertedArg)
					throwDmsErrF("non-representable numerical overflow in accumulation of %s with %s", assignee, arg);
			}
			else
			{
				if (assignee <= std::numeric_limits<R>::min() - convertedArg)
					throwDmsErrF("non-representable numerical overflow in accumulation of %s with %s", assignee, arg);
			}

		}
		else
		{
			if (assignee > std::numeric_limits<R>::max() - convertedArg)
				throwDmsErrF("non-representable numerical overflow in accumulation of %s with %s", assignee, arg);
		}
	}

	R result = assignee + convertedArg;

	if constexpr (!is_signed_v<R>)
	{
		static_assert(!std::is_floating_point_v<R>);
		bool hasOverflow = false;
		if constexpr (is_signed_v<T>)
			hasOverflow = (convertedArg >= 0 ? result < assignee : result >= assignee);
		else
		{
			assert(convertedArg >= 0);
			hasOverflow = (result < convertedArg);
		}

		if (hasOverflow || !IsDefined(result))
			throwDmsErrF("non-representable numerical overflow in accumulation of %s with %s", assignee, arg);
	}
	assignee = result;
}

template<typename R, typename T> void SafeAccumulate(Point<R>& assignee, const Point<T>& arg)
{
	SafeAccumulate(assignee.first, arg.first);
	SafeAccumulate(assignee.second, arg.second);
}

template <typename R, typename T>
struct unary_assign_add : unary_assign<R, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return cast_unit_creator<R>(args); }

	void operator()(typename unary_assign_add::assignee_ref assignee, cref_t<T> arg) const
	{ 	
		SafeAccumulate(assignee, arg);
	}
	void CombineValues(R& a, R rhs) const
	{
		::SafeAccumulate(a, rhs);
	}
	void CombineRefs(typename unary_assign_add::assignee_ref assignee, cref_t<typename unary_assign_add::assignee_type> rhs) const
	{
		if constexpr (has_undefines_v<R>)
		{
			assert(IsDefined(assignee));
			assert(IsDefined(rhs));
		}
		::SafeAccumulate(assignee, rhs);
	}
};

template <typename T>
struct unary_assign_min : unary_assign<T, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }

	void operator()(typename unary_assign_min::assignee_ref assignee, typename unary_assign_min::arg1_cref arg) const
	{ 
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(arg))
				return;
		MakeLowerBound(assignee, arg);
	}
	void CombineRefs(typename unary_assign_min::assignee_ref assignee, typename unary_assign_min::arg1_cref rhs) const
	{
		(*this)(assignee, rhs);
	}
	void CombineValues(T& assignee, T arg) const
	{
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(arg))
				return;
		MakeLowerBound(assignee, arg);
	}
};


template <typename T, typename U=T>
struct unary_assign_max : unary_assign<T, U>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }

	void operator()(typename unary_assign_max::assignee_ref assignee, typename unary_assign_max::arg1_cref arg) const
	{ 
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(arg))
				return;
		MakeUpperBound(assignee, arg);
	}
	void CombineRefs(typename unary_assign_max::assignee_ref assignee, typename unary_assign_max::arg1_cref rhs) const
	{
		(*this)(assignee, rhs);
	}
	void CombineValues(T& assignee, T arg) const
	{
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(arg))
				return;
		MakeUpperBound(assignee, arg);
	}
};

template <typename OR, typename T>
struct unary_assign_once : unary_assign<OR, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return cast_unit_creator<R>(args); }

	void operator()(vref_t<OR> assignee, cref_t<T> arg) const
	{ 
		if (IsDefined(assignee))
			return;
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(arg))
				return;
		Assign(assignee, arg);
	}

	void CombineRefs(vref_t<OR> assignee, cref_t<OR> rhs) const
	{
		if (IsDefined(assignee))
			return;
		if (!IsDefined(rhs))
			return;
		Assign(assignee, rhs);
	}
	void CombineValues(OR& assignee, OR rhs) const
	{
		if (IsDefined(assignee))
			return;
		if (!IsDefined(rhs))
			return;
		Assign(assignee, rhs);
	}
};

template <typename OR, typename T>
struct unary_assign_overwrite: unary_assign<OR, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }

	void operator()(vref_t<OR> assignee, cref_t<T> arg) const
	{
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(arg))
				return;
		Assign(assignee, arg);
	}
	void CombineRefs(vref_t<OR> assignee, cref_t<OR> rhs) const
	{
		if (!IsDefined(rhs))
			return;
		Assign(assignee, rhs);
	}
	void CombineValues(OR& assignee, OR rhs) const
	{
		if (!IsDefined(rhs))
			return;
		Assign(assignee, rhs);
	}
};

struct nullary_set_true: nullary_assign<Bool>
{
	void operator()(typename nullary_assign::assignee_ref assignee) const
	{ 
		assignee = true;
	}
};

struct nullary_set_false: nullary_assign<Bool>
{
	void operator()(typename nullary_assign::assignee_ref assignee) const
	{ 
		assignee = false;
	}
};

struct unary_assign_any : unary_assign<Bool, Bool>
{
	template <typename R = Bool>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator()(typename unary_assign_any::assignee_ref assignee, typename unary_assign_any::arg1_cref arg) const
	{ 
		if (arg)
			assignee = true;
	}
	void CombineValues(Bool& assignee, Bool rhs) const
	{
		if (rhs)
			assignee = true;
	}
	void CombineRefs(typename unary_assign_any::assignee_ref assignee, typename unary_assign_any::arg1_cref rhs) const
	{
		if (rhs)
			assignee = true;
	}
};

struct unary_assign_all : unary_assign<Bool, Bool>
{
	template <typename R = Bool>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }

	void operator()(typename unary_assign_all::assignee_ref assignee, typename unary_assign_all::arg1_cref arg) const
	{ 
		if (!arg)
			assignee = false;
	}
	void CombineValues(Bool& assignee, Bool rhs) const
	{
		if (!rhs)
			assignee = false;
	}
	void CombineRefs(typename unary_assign_all::assignee_ref assignee, typename unary_assign_all::arg1_cref rhs) const
	{
		if (!rhs)
			assignee = false;
	}
};

// *****************************************************************************
//                      count functions
// *****************************************************************************

template <typename V, typename I> 
void count_best_total(I& output, const V* valuesFirst, const V* valuesLast)
{
	aggr1_total<unary_assign_inc<V, I> >(output, valuesFirst, valuesLast);
}

template <bit_size_t N, typename Block, typename I> 
void count_best_total(I& output, bit_iterator<N, Block> valuesFirst, bit_iterator<N, Block> valuesLast)
{
	output += (valuesLast - valuesFirst);
}

template<typename CIV, typename OIA>
void count_best_partial_best(OIA outFirst, CIV valuesFirst, CIV valuesLast, const IndexGetter* indices)
{
	typedef typename std::iterator_traits<CIV>::value_type V;
	aggr_fw_partial<unary_assign_inc<V, typename value_type_of_iterator<OIA>::type>  >(outFirst, valuesFirst, valuesLast, indices);
}

#endif // !defined(__CLC_AGGRFUNCNUM_H)


