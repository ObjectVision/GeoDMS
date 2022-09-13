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
	dms_assert(argData.size() == resData.size());

	OrgAttrAssigner::PrepareTile(resData, argData);
	typedef typename v_ref<typename ResSpan::value_type>::type assignee_ref;
	typedef typename cref <typename ArgSpan::value_type>::type arg1_cref;

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
	dms_assert(!hasUndefinedValues);
	dms_assert(argData.size() == resData.size());

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

	dms_assert(argData.size() == resData.size());
	if (!resData.size())
		return;
	if (hasUndefinedValues)
		dms_transform(
			argData.begin(), 
			argData.end(), 
			resData.begin(), 
			[oper] (arg_cref_type t) -> res_value_type
			{ 
				if (!IsDefined(t))
					return UNDEFINED_OR_ZERO(res_value_type);
				return oper(t);
			}
		);
	else
		dms_transform(
			argData.begin(), 
			argData.end(), 
			resData.begin(), 
			oper
		);
}

// specialization for bit_sequence, use the fact that no missing data elements exist for bit_values

template<typename ResSequence, typename Block, int N, typename UnaryOper>
void do_unary_func(
	ResSequence resData,
	const bit_sequence<N, Block>& argData,
	const UnaryOper& oper,
	bool hasUndefinedValues)
{
	dms_assert(!hasUndefinedValues);
	dms_assert(argData.size() == resData.size());

	dms_transform(
		argData.begin(), 
		argData.end(), 
		resData.begin(), 
		oper
	);
}

#endif //!defined(__CLC_ATTRUNISTRUCT_H)