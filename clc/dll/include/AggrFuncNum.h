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

template <typename T, typename I>
struct unary_assign_inc : unary_assign<I, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<R>(); }

	void operator()(vref_t<I> assignee, cref_t<T> arg) const
	{ 
		if constexpr (has_undefines_v<I>)
		{
			if (!IsDefined(assignee))
				return;
		}
		if constexpr (has_undefines_v<T>)
		{
			if (!IsDefined(arg))
				return;
		}
		assignee++;
		if constexpr (!has_undefines_v<I>)
		{ 
			if (assignee == I())
				throwDmsErrD("non-representable numerical overflow of sub-byte value");
		}
		else
		{
			if (!IsDefined(assignee))
				throwDmsErrD("non-representable numerical overflow of sub-byte value");
		}
	}
};

template<typename R, typename T> void SafeAccumulate(R& assignee, T arg) // see the similarity with safe_plus
{
	R orgAssignee = assignee; // may-be used in check
	if constexpr (has_undefines_v<R>)
	{
		if constexpr (is_signed_v<R> && !std::is_floating_point_v<R>)
			if (!IsDefined(assignee))
				return;
	}
	if constexpr (has_undefines_v<T>)
		if (!IsDefined(arg))
			return;

	assignee += arg;

	if constexpr (!std::is_floating_point_v<T>)
	{
		if constexpr (!is_signed_v<R>)
		{
			bool hasOverflow = false;
			if constexpr (is_signed_v<T>)
				hasOverflow = (arg >= 0 ? assignee < orgAssignee : assignee >= orgAssignee);
			else
			{
				std::conditional_t<is_bitvalue_v<T>, UInt8, T> argCopy = arg;
				hasOverflow = (assignee < argCopy);
			}

			if (hasOverflow)
				throwDmsErrD("non-representable numerical overflow in aggregation");
		}
		else
		{
			bool aNonnegative = (orgAssignee >= 0);
			bool bNonnegative = (Convert<R>(arg) >= 0);

			if (aNonnegative == bNonnegative)
			{
				auto resultNonnegative = (assignee>= 0);
				if (aNonnegative != resultNonnegative)
					throwDmsErrD("non-representable numerical overflow in aggregation");
			}
		}
	}
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
		assignee = arg;
	}
};

template <typename T>
struct unary_assign_overwrite: unary_assign<T, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }

	void operator()(vref_t<T>  assignee, cref_t<T>  arg) const
	{ 
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(arg))
				return;
		assignee = arg;
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


