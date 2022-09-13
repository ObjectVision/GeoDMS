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

	void operator()(typename unary_assign_inc::assignee_ref assignee, typename unary_assign_inc::arg1_cref arg) const
	{ 
		assignee++; 
	}
};

template<typename R, typename T> void SafeAccumulate(R& assignee, const T& arg)
{
	assignee += arg;
}

template<typename T, typename U> void SafeAccumulate(Point<T>& assignee, const Point<U>& arg)
{
	assignee.first  += arg.first;
	assignee.second += arg.second;
}

template <typename R, typename T>
struct unary_assign_add : unary_assign<R, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return cast_unit_creator<R>(args); }

	void operator()(typename unary_assign_add::assignee_ref assignee, typename unary_assign_add::arg1_cref arg) const
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
		MakeUpperBound(assignee, arg);
	}
};

template <typename OR, typename T>
struct unary_assign_once : unary_assign<OR, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return cast_unit_creator<R>(args); }

	void operator()(typename unary_assign_once::assignee_ref assignee, typename unary_assign_once::arg1_cref arg) const
	{ 
//		dms_assert(IsDefined(arg)); // caller should check if T is not a bitvalue
		if ((!IsDefined(assignee)))
			assignee = arg;
	}
};

template <typename T>
struct unary_assign_overwrite: unary_assign<T, T>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }

	void operator()(typename unary_assign_overwrite::assignee_ref assignee, typename unary_assign_overwrite::arg1_cref arg) const
	{ 
//		dms_assert(IsDefined(arg)); // caller should check if T is not a bitvalue
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
void count_best_total(I& output, const V* valuesFirst, const V* valuesLast, bool hasUndefinedValues)
{
	if (hasUndefinedValues)
		aggr1_total_best<unary_assign_inc<V, I> >(output, valuesFirst, valuesLast, hasUndefinedValues);
	else
		output += (valuesLast - valuesFirst);
}

template <bit_size_t N, typename Block, typename I> 
void count_best_total(I& output, bit_iterator<N, Block> valuesFirst, bit_iterator<N, Block> valuesLast, bool hasUndefinedValues)
{
	dms_assert(!hasUndefinedValues);
	output += (valuesLast - valuesFirst);
}

template<typename CIV, typename OIA>
void count_best_partial_best(OIA outFirst, CIV valuesFirst, CIV valuesLast, const IndexGetter* indices, bool hasUndefinedValues)
{
	typedef typename std::iterator_traits<CIV>::value_type V;
	aggr_fw_best_partial<unary_assign_inc<V, typename value_type_of_iterator<OIA>::type>  >(outFirst, valuesFirst, valuesLast, indices, hasUndefinedValues);
}

#endif // !defined(__CLC_AGGRFUNCNUM_H)


