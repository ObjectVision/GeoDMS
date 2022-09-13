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

#if !defined(__CLC_ATTRBINSTRUCT_H)
#define __CLC_ATTRBINSTRUCT_H

#include "utl/StringFunc.h"

#include "Prototypes.h"
#include "UnitCreators.h"
#include "composition.h"
#include "AttrUniStruct.h"
#include "dms_transform.h"

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

	if (e1IsVoid)
	{
		dms_assert(arg1Data.size() == 1);

		if (arg1HasMissingData && !IsDefined(arg1Data[0]))
			fast_undefine(resData.begin(), resData.end());
		else
		{
			arg1_cref arg1Value = arg1Data[0];
			do_unary_func(
				resData
			,	arg2Data
			,	composition_2_v_p<BinaryOper>(oper, arg1Data[0]) // composition allows for optimizing SIMD
//			,	[arg1Value, oper](arg2_cref arg2) { return oper(arg1Value, arg2); }
			,	arg2HasMissingData
			);
		}
		return;
	}

	dms_assert(arg1Data.size() == resData.size());

	if (e2IsVoid)
	{
		dms_assert(arg2Data.size() == 1);

		if (arg2HasMissingData && !IsDefined(arg2Data[0]))
			fast_fill(resData.begin(), resData.end(), UNDEFINED_OR_ZERO(typename ResSequence::value_type) );
		else
		{
			arg2_cref arg2Value = arg2Data[0];
			do_unary_func(
				resData
			,	arg1Data
			,	composition_2_p_v<BinaryOper>(oper, arg2Data[0])
//			,	[arg2Value, oper](arg1_cref arg1) { return oper(arg1, arg2Value); }
			,	arg1HasMissingData
			);
		}
		return;
	}

	dms_assert(arg2Data.size() == resData.size());

	if (arg1HasMissingData || arg2HasMissingData)
	{
		dms_transform(
			arg1Data.begin(), arg1Data.end(), 
			arg2Data.begin(), resData.begin(), 
			binary_func_checked_base<BinaryOper>(oper)
		);
	}
	else
	{
		dms_transform(
			arg1Data.begin(), arg1Data.end(), 
			arg2Data.begin(), resData.begin(), 
			oper
		);
	}
}

// specialization for bit_sequence, use the fact that no missing data elements exist for bit_values

template<typename ResSequence, typename Block, int N, typename BinaryOper>
void do_binary_func(
	ResSequence            resData,
	bit_sequence<N, Block> arg1Data,
	bit_sequence<N, Block> arg2Data,
	const BinaryOper& oper,
	bool e1IsVoid, bool e2IsVoid,
	bool arg1HasMissingData, 
	bool arg2HasMissingData)
{
	dms_assert(!arg1HasMissingData);
	dms_assert(!arg2HasMissingData);

	if (e1IsVoid)
	{
		dms_assert(arg1Data.size() == 1);

		do_unary_func(
			resData, 
			arg2Data, 
			composition_2_v_p<BinaryOper>(oper, arg1Data[0]),
			arg2HasMissingData
		);
		return;
	}

	dms_assert(arg1Data.size() == resData.size());

	if (e2IsVoid)
	{
		dms_assert(arg2Data.size() == 1);

		do_unary_func(
			resData, 
			arg1Data, 
			composition_2_p_v<BinaryOper>(oper, arg2Data[0]), 
			arg1HasMissingData
		);
		return;
	}

	dms_assert(arg2Data.size() == resData.size());

	dms_transform(
		arg1Data.begin(), arg1Data.end(), 
		arg2Data.begin(), resData.begin(), 
		oper
	);
}


// *****************************************************************************
//						ELEMENTARY BINARY FUNCTORS
// *****************************************************************************
#include <functional>

template <typename T> struct plus_func : std_binary_func< std::plus<T>, T, T, T>       
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return  compatible_values_unit_creator_func(0, gr, args); }
};

template <typename T> struct minus_func: std_binary_func< std::minus<T>, T, T, T>      
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return  compatible_values_unit_creator_func(0, gr, args); }
};

template <typename T> struct mul_func  : std_binary_func< std::multiplies<T>, T, T, T> 
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mul2_unit_creator(gr, args); }
};

template <typename T>
struct mulx_func : binary_func<typename acc_type<T>::type, T, T>
{
	typename mulx_func::res_type operator()(typename mulx_func::arg1_cref a1, typename mulx_func::arg2_cref a2) const { return typename mulx_func::res_type(a1) * typename mulx_func::res_type(a2); }
};

template <typename R, typename T, typename U=T>
struct div_func_base: binary_func<R, T, U>
{
	typename div_func_base::res_type operator()(typename div_func_base::arg1_cref t1, typename div_func_base::arg2_cref t2) const
	{ 
		dms_assert(t1 != 0 || t1 == 0); // validates that T is a scalar
		return
				(	t2 != 0 // validates that U is a scalar 
				&&	t2 != UNDEFINED_VALUE(U)
				&&	t1 != UNDEFINED_VALUE(T)
				)	
			?	(Convert<typename div_func_base::res_type>(t1) / Convert<typename div_func_base::res_type>(t2))
			:	UNDEFINED_VALUE(typename div_func_base::res_type);
	}
};

template <typename T, typename U = T> struct div_func_best: div_func_base<typename div_type<T>::type, T, U> {};

template <typename T, typename U>
struct div_func_best<Point<T>, U>
	:	binary_func<Point<typename div_type<T>::type>, Point<T>, U >
{
	typedef typename div_type<T>::type      quotient_type;
	typedef typename cref<Point<T> >::type  point_ref_type;
	typedef div_func_base<quotient_type, U> base_func;

	Point<quotient_type> operator()(point_ref_type t1, typename base_func::arg2_cref t2) const
	{
		return Point<quotient_type>(m_BaseFunc(t1.first, t2), m_BaseFunc(t1.second, t2) );
	}

	base_func m_BaseFunc;
};

template <typename T, typename U>
struct div_func_best<Point<T>, Point<U> >
	: binary_func<Point<typename div_type<T>::type>, Point<T>, Point<U> >
{
	typedef typename div_type<T>::type      quotient_type;
	typedef typename cref<Point<T> >::type  point1_ref_type;
	typedef typename cref<Point<U> >::type  point2_ref_type;
	typedef div_func_base<quotient_type, T, U> base_func;

	Point<quotient_type> operator()(point1_ref_type t1, point2_ref_type t2) const
	{
		return Point<quotient_type>(m_BaseFunc(t1.first, t2.first), m_BaseFunc(t1.second, t2.second) );
	}

	base_func m_BaseFunc;
};

template <typename T>
struct div_func: binary_func<T, T, T >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return div_unit_creator(gr, args); }

	typename div_func::res_type operator()(typename div_func::arg1_cref t1, typename div_func::arg2_cref t2) const
	{ 
		return (t2 != T())	
			?	(t1 / t2)
			:	UNDEFINED_VALUE(typename div_func::res_type);
	}
};

template <typename T> struct qint_t          { typedef Int64 type; };
template <>           struct qint_t<Float32> { typedef Int32 type; };

template <typename T> 
typename std::enable_if<std::is_integral_v<T>, T>::type
mod_func_impl(const T& counter, const T& divider)
{
	return counter % divider;
}

template <typename T>
typename std::enable_if<std::is_floating_point_v <T>, T>::type
mod_func_impl(const T& counter, const T& divider)
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

	UInt32 operator ()(typename strcount_func::arg1_cref arg1, typename strcount_func::arg2_cref arg2) const
	{
		return StrCount(arg1, arg2);
	}
};


#endif //!defined(__CLC_ATTRBINSTRUCT_H)