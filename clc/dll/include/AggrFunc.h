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

#if !defined(__CLC_AGGRFUNC_H)
#define __CLC_AGGRFUNC_H

#include "Prototypes.h"
#include "PartitionTypes.h"
#include "IndexGetterCreator.h"

// *****************************************************************************
//                      aggregate_total function
// *****************************************************************************

template <typename assignee_ref, typename TAssignUniFunc, typename CIV> 
void aggr1_total(assignee_ref output, CIV valuesFirst, CIV valuesLast, TAssignUniFunc assignFunc = TAssignUniFunc())
{ 
	for (; valuesFirst != valuesLast; ++valuesFirst)
		assignFunc(output, *valuesFirst);
}

template <typename TAssignFunc, typename CIV> 
typename std::enable_if<has_undefines_v<typename std::iterator_traits<CIV>::value_type> >::type
aggr1_total_best(typename TAssignFunc::assignee_ref output, CIV valuesFirst, CIV valuesLast, bool hasUndefinedValues, TAssignFunc assignFunc = TAssignFunc() )
{ 
	typedef typename TAssignFunc::assignee_ref assignee_ref;
	typedef typename TAssignFunc::arg1_cref arg1_cref;

	if (!hasUndefinedValues)
		aggr1_total<assignee_ref>(output, valuesFirst, valuesLast, assignFunc);
	else
		aggr1_total<assignee_ref>(output, valuesFirst, valuesLast,  
			[assignFunc](assignee_ref res, arg1_cref arg)
			{
				if (IsDefined(arg))
					assignFunc(res, arg);
			}
		);
}

template <typename TAssignFunc, typename CIV> 
typename boost::disable_if<has_undefines<typename std::iterator_traits<CIV>::value_type> >::type
aggr1_total_best(typename TAssignFunc::assignee_ref output, CIV valuesFirst, CIV valuesLast, bool hasUndefinedValues, TAssignFunc assignFunc = TAssignFunc() )
{ 
	dms_assert(!hasUndefinedValues);
	aggr1_total<typename TAssignFunc::assignee_ref>(output, valuesFirst, valuesLast, assignFunc);
}

template <typename assignee_ref, typename TAssignBinFunc, typename CIV> 
void aggr2_total(assignee_ref output, CIV values1First, CIV values1Last, CIV values2First, const TAssignBinFunc& assignFunc = TAssignBinFunc())
{ 
	for (; values1First != values1Last; ++values2First, ++values1First)
		assignFunc(output, *values1First, *values2First);
}

template <typename TAssignFunc, typename CIV> 
typename std::enable_if<has_undefines_v<typename std::iterator_traits<CIV>::value_type> >::type
aggr2_total_best(typename TAssignFunc::assignee_ref output, CIV values1First, CIV values1Last, CIV values2First, bool hasUndefinedValues, TAssignFunc assignFunc = TAssignFunc() )
{ 
//	typedef binary_assign_nonnullonly<TAssignFunc> TCheckedFunc;
	if (!hasUndefinedValues)
		aggr2_total<typename TAssignFunc::assignee_ref>(output, values1First, values1Last, values2First, assignFunc);
	else
		aggr2_total<typename TAssignFunc::assignee_ref>(output, values1First, values1Last, values2First,
			[assignFunc](typename TAssignFunc::assignee_ref res, typename TAssignFunc::arg1_cref arg1, typename TAssignFunc::arg2_cref arg2)
			{
				if (IsDefined(arg1) && IsDefined(arg2))
					assignFunc(res, arg1, arg2);
			}
		);
}

template <typename TAssignFunc, typename CIV> 
typename std::enable_if<!has_undefines_v<typename std::iterator_traits<CIV>::value_type> >::type
aggr2_total_best(typename TAssignFunc::assignee_ref output, CIV values1First, CIV values1Last, CIV values2First, bool hasUndefinedValues, TAssignFunc assignFunc = TAssignFunc() )
{ 
	dms_assert(!hasUndefinedValues);
	aggr2_total<TAssignFunc>(output, values1First, values1Last, values2First, assignFunc);
}

// ***************************************************************************** Dispatching of different partition types

// Specialization for partition indices without possible <null> or out-of-range
template<typename TAssignFunc, typename CIV, typename OIA>
void aggr_fw_partial(OIA outFirst, CIV valuesFirst, CIV valuesLast, const IndexGetter* indices, TAssignFunc assignFunc = TAssignFunc() )
{
	SizeT i=0;
	if (indices->AllDefined())
		for (; valuesFirst != valuesLast; ++i, ++valuesFirst)
			assignFunc(outFirst[indices->Get(i)], *valuesFirst);
	else
		for (; valuesFirst != valuesLast; ++i, ++valuesFirst)
		{
			SizeT index = indices->Get(i);
			if (IsDefined(index))
				assignFunc(outFirst[index], *valuesFirst);
		}
}

// ***************************************************************************** Dispatching of different values type

// Specialization for Values with <null>
template<typename TAssignFunc, typename CIV, typename OIA>
typename std::enable_if<has_undefines_v<typename std::iterator_traits<CIV>::value_type> >::type
aggr_fw_best_partial(OIA outFirst
	,	CIV valuesFirst,  CIV valuesLast
	,	const IndexGetter* indices
	,	bool hasUndefinedValues, const TAssignFunc& assignFunc = TAssignFunc()
	)
{
	typedef typename TAssignFunc::assignee_ref assignee_ref;
	typedef typename TAssignFunc::arg1_cref arg1_cref;

	if (!hasUndefinedValues)
		aggr_fw_partial(outFirst, valuesFirst, valuesLast, indices, assignFunc);
	else
		aggr_fw_partial(outFirst, valuesFirst, valuesLast, indices,
			[assignFunc](assignee_ref res, arg1_cref arg)
			{
				if (IsDefined(arg))
					assignFunc(res, arg);
			}
		);
}

// Specialization for Values without <null>
template<typename TAssignFunc, typename CIV, typename OIA>
typename std::enable_if<!has_undefines_v<typename std::iterator_traits<CIV>::value_type> >::type
aggr_fw_best_partial(OIA outFirst
	,	CIV valuesFirst,  CIV valuesLast
	,	const IndexGetter* indices
	,	bool hasUndefinedValues
	,	TAssignFunc assignFunc = TAssignFunc()
	)
{
	dms_assert(!hasUndefinedValues);
	aggr_fw_partial<TAssignFunc>(outFirst, valuesFirst, valuesLast, indices, assignFunc);
}

/* ============================ Binary accumulators ============== */

// *****************************************************************************
//                      aggregate_partial functions

// ***************************************************************************** Dispatching of different partition types

// Specialization for partition indices with possible <null> or out-of-range
template<typename TAssignFunc, typename CIV, typename OIA>
void aggr2_fw_partial(OIA outFirst
	,	CIV values1First, CIV values1Last,	CIV values2First
	,	const IndexGetter* indices
	,	const TAssignFunc& assignFunc = TAssignFunc()
	)
{
	SizeT i=0;
	if (indices->AllDefined())
		for (; values1First != values1Last; ++i, ++values2First, ++values1First)
			assignFunc(outFirst[indices->Get(i)], *values1First, *values2First);
	else
		for (; values1First != values1Last; ++i, ++values2First, ++values1First)
		{
			SizeT index = indices->Get(i);
			if (IsDefined(index))
				assignFunc(outFirst[index], *values1First, *values2First);
		}
}

// ***************************************************************************** Dispatching of different values type

// Specialization for Values with <null>
template<typename TAssignFunc, typename CIV, typename OIA>
typename std::enable_if<has_undefines_v<typename std::iterator_traits<CIV>::value_type> >::type
aggr2_fw_best_partial(OIA outFirst
	,	CIV values1First,  CIV values1Last,	CIV values2First
	,	const IndexGetter* indices
	,	bool hasUndefinedValues
	,	TAssignFunc assignFunc = TAssignFunc()
	)
{
	if (!hasUndefinedValues)
		aggr2_fw_partial(outFirst, values1First, values1Last, values2First, indices, assignFunc);
	else
		aggr2_fw_partial(outFirst, values1First, values1Last, values2First, indices,
			[assignFunc](typename TAssignFunc::assignee_ref res, typename TAssignFunc::arg1_cref arg1, typename TAssignFunc::arg2_cref arg2)
			{
				if (IsDefined(arg1) && IsDefined(arg2))
					assignFunc(res, arg1, arg2);
			} );
}

// Specialization for Values without <null>
template<typename TAssignFunc, typename CIV, typename OIA>
typename std::enable_if<!has_undefines_v<typename std::iterator_traits<CIV>::value_type> >::type
aggr2_fw_best_partial(OIA outFirst
	,	CIV values1First,  CIV values1Last, CIV values2First
	,	const IndexGetter* indices
	,	bool hasUndefinedValues
	,	const TAssignFunc& assignFunc = TAssignFunc()
	)
{
	dms_assert(!hasUndefinedValues);
	aggr2_fw_partial<TAssignFunc>(outFirst, values1First, values1Last, values2First, indices, assignFunc);
}

#endif // !defined(__CLC_AGGRFUNC_H)
