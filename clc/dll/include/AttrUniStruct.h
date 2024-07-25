// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_ATTRUNISTRUCT_H)
#define __CLC_ATTRUNISTRUCT_H

#include "Prototypes.h"
#include "UnitCreators.h"
#include "dms_transform.h"

#include "geo/IsNotUndef.h"

// *****************************************************************************
//						ELEMENTARY PREDICATE FUNCTORS
// *****************************************************************************

template <typename T>
struct is_defined_func : unary_func<Bool, T>
{
	Bool operator ()(typename unary_func<Bool, T>::arg1_cref t) const { return IsNotUndef(t); }

	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return boolean_unit_creator(gr, args); }
};

template <typename T>
struct is_undefined_func : unary_func<Bool, T>
{
	Bool operator ()(typename unary_func<Bool, T>::arg1_cref t) const { return !IsNotUndef(t); }

	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return boolean_unit_creator(gr, args); }
};

template <typename T>
struct is_zero_func : unary_func<Bool, T>
{
	Bool operator ()(typename unary_func<Bool, T>::arg1_cref t) const { return t == T(); }

	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return boolean_unit_creator(gr, args); }
};

template <typename T>
struct is_positive_func : unary_func<Bool, T>
{
	Bool operator ()(typename unary_func<Bool, T>::arg1_cref t) const { return IsDefined(t) && t > T(); }

	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return boolean_unit_creator(gr, args); }
};

template <typename T>
struct is_negative_func : unary_func<Bool, T>
{
	Bool operator ()(typename unary_func<Bool, T>::arg1_cref t) const { return IsNegative(t) && IsDefined(t); } // && t < T(); }

	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return boolean_unit_creator(gr, args); }
};

template <typename T>
struct is_nonzero: unary_func<Bool, T>
{
	Bool operator ()(typename is_nonzero::arg1_cref t) const { return t != T(); }

	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return boolean_unit_creator(gr, args); }
};

// *****************************************************************************
//								do unary and binary operators
// *****************************************************************************

template<typename ResSpan, typename ArgSpan, typename AttrAssigner, typename OrgAttrAssigner>
void do_unary_assign(ResSpan resData, ArgSpan argData, const AttrAssigner& oper, bool hasUndefinedValues, const OrgAttrAssigner* orgOperPtr)
{
	assert(argData.size() == resData.size());

	OrgAttrAssigner::PrepareTile(resData, argData);
	using assignee_ref = vref_t<typename ResSpan::value_type>;
	using arg1_cref = cref_t<typename ArgSpan::value_type>;

	if (hasUndefinedValues)
		transform_assign(
			resData.begin(), 
			argData.begin(), 
			argData.end(), 
			[oper](assignee_ref res, arg1_cref arg) -> void
			{
				if (IsDefined(arg))
					oper(res, arg);
				else
					MakeUndefined(res); 
			}
		);
	else
		transform_assign(
			resData.begin(), 
			argData.begin(), 
			argData.end(), 
			oper
		);
}

// specialization for bit_sequence, use the fact that no missing data elements exist for bit_values

template<typename ResSpan, typename Block, int N, typename AttrAssigner>
void do_unary_assign(ResSpan resData, bit_sequence<N, Block> argData, const AttrAssigner& oper, bool hasUndefinedValues, const AttrAssigner*)
{
	assert(!hasUndefinedValues);
	assert(argData.size() == resData.size());

	transform_assign(
		resData.begin(),
		argData.begin(),
		argData.end(),
		oper
	);
}

template<typename ResSequence, typename ArgContainer, typename UnaryOper>
void do_unary_func(
	      ResSequence resData,
	const ArgContainer& argData,
	const UnaryOper&      oper,
	bool hasUndefinedValues)
{
	typedef typename ResSequence::value_type                      res_value_type;
	typedef typename cref<typename ArgContainer::value_type>::type arg_cref_type;

	constexpr bool mustCheckUndefined = !is_safe_for_undefines<UnaryOper>::value && has_undefines_v<typename ArgContainer::value_type>;

	assert(argData.size() == resData.size());
	if (!resData.size())
		return;
	if constexpr (mustCheckUndefined)
	{
		if (hasUndefinedValues)
		{
			dms_transform(
				argData.begin(),
				argData.end(),
				resData.begin(),
				[oper](arg_cref_type t) -> res_value_type
				{
					if (!IsDefined(t))
						return UNDEFINED_OR_ZERO(res_value_type);
					return oper(t);
				}
			);
			return;
		}
	}
	dms_transform(
		argData.begin(), 
		argData.end(), 
		resData.begin(), 
		oper
	);
}

#endif //!defined(__CLC_ATTRUNISTRUCT_H)