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
#if !defined(__CLC_PROTOTYPES_H)
#define __CLC_PROTOTYPES_H

namespace has_block_func_details {
	template< typename U> static char test(typename U::block_func* v);
	template< typename U> static int  test(...);
}

template< typename T>
struct has_block_func
{
	static constexpr bool value = sizeof(has_block_func_details::test<T>(nullptr)) == sizeof(char);
};

template< typename T>
constexpr bool has_block_func_v = has_block_func<T>::value;

#include "RtcTypeLists.h"
#include "geo/Conversions.h"
#include "UnitCreators.h"

class OutStreamBuff;
template <typename T> struct AbstrValueGetter;
typedef AbstrValueGetter<SizeT> IndexGetter;


// *****************************************************************************
//											DEFINES
// *****************************************************************************


//	The following OPER_TYPE_INSTANTIATION macros instantiate operators of
//		of template type a, with functor b1<TYPE> 
//		with name b2##TYPE(c=name, d=doesCheck)

#define	OPER_INSTANTIATION(OperCls, FuncTempl, T, Group) \
			static OperCls< FuncTempl<T> > FuncTempl##T(Group); \

#define	OPER_INT_INSTANTIATION(OperCls, FuncTempl, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, UInt32, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, Int32,  Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, UInt16, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, Int16,  Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, UInt8,  Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, Int8,   Group) \

#define	OPER_FLOAT_INSTANTIATION(OperCls, FuncTempl, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, Float32, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, Float64, Group) \

#define	OPER_BOOL_INSTANTIATION(OperCls, FuncTempl, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, Bool, Group)

#if defined(DMS_TM_HAS_UINT2)
#define	OPER_BINT_INSTANTIATION(OperCls, FuncTempl, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, UInt2, Group); \
			OPER_INSTANTIATION(OperCls, FuncTempl, UInt4, Group) 
#else
#define	OPER_BINT_INSTANTIATION(OperCls, FuncTempl, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, UInt4, Group) 
#endif
#define	OPER_BITVAL_INSTANTIATION(OperCls, FuncTempl, Group) \
			OPER_INSTANTIATION(OperCls, FuncTempl, Bool, Group); \
			OPER_BINT_INSTANTIATION(OperCls, FuncTempl, Group) 

#include "utl/TypeListOper.h"

#define	OPER2_BITVAL_INSTANTIATION(OperCls, FuncTempl, T, Group) \
			OperCls< FuncTempl<Bool,  T > > FuncTempl##T##Bool (Group); \
			OperCls< FuncTempl<UInt2, T > > FuncTempl##T##UInt2(Group); \
			OperCls< FuncTempl<UInt4, T > > FuncTempl##T##UInt4(Group)

//=======================================
// various ref types
//=======================================

// arguments   
template <typename T> struct cref : param_type<T> {};
template <typename E> struct cref<std::vector<E> > { typedef typename sequence_traits<std::vector<E> >::container_type::const_reference type; };
template <>           struct cref<SharedStr     > { typedef          sequence_traits<SharedStr     >::container_type::const_reference type; };

// func results
template <typename T> struct f_ref { using type = typename sequence_traits<T>::container_type::reference; };

// assign results
template<typename T> struct v_ref : f_ref<T> {};
template<>           struct v_ref<OutStreamBuff> { using type = OutStreamBuff&; };

template <typename T> T make_result(const T& output) { return output; }

// *****************************************************************************
//									FUNCTOR PROTOTYPES
// *****************************************************************************

template <typename _R>
struct nullary_func
{
	typedef _R                             res_type; 
	typedef typename f_ref<res_type>::type res_ref;
};

template<typename _R, typename _A1>
struct unary_func //: std::unary_function<_A1, _R>; NOTE THAT order of args has changed
	:	nullary_func<_R>
{
	typedef _A1                            arg1_type;
	typedef typename cref<arg1_type>::type arg1_cref;
};


template<typename _R, typename _A1, typename _A2>
struct binary_func //: std::binary_function<_A1, _A2, _R> // NOTE THAT order of args has changed
	:	unary_func<_R, _A1>
{
	typedef _A2                            arg2_type;
	typedef typename cref<arg2_type>::type arg2_cref;
};

template<typename _R, typename _A1, typename _A2, typename _A3>
struct ternary_func
:	binary_func<_R, _A1, _A2>
{
	typedef _A3                            arg3_type;
	typedef typename cref<arg3_type>::type arg3_cref;
};

// *****************************************************************************
// std::functions adapters
// *****************************************************************************


template <typename TUniFunc, typename R, typename A1> 
struct std_unary_func :	unary_func<R,A1>, TUniFunc
{
/* REMOVE
	typename std_unary_func::res_type operator()(typename std_unary_func::arg1_cref a1) const
	{ 
		return m_Func(a1); 
	}

private:
	TUniFunc m_Func;
*/
};

template <
	typename TBinFunc
,	typename R
,	typename A1
,	typename A2
>	
struct std_binary_func: binary_func<R, A1, A2>
{
	typename std_binary_func::res_type operator()(typename std_binary_func::arg1_cref a1, typename std_binary_func::arg2_cref a2) const
	{
		return 
			Convert<typename std_binary_func::res_type>(
				m_Func(
					Convert<A1>(a1)
				,	Convert<A2>(a2)
				)
			); 
	}

private:
	TBinFunc m_Func;
};

// *****************************************************************************
//								ASSIGNMENT FUNCTOR PROTOTYPES
// *****************************************************************************

template<typename TAssigneeType>
struct nullary_assign 
{
	typedef TAssigneeType                        assignee_type;
	typedef typename v_ref<assignee_type>::type  assignee_ref;
};

template<typename TAssigneeType, typename TArgumentType>
struct unary_assign : nullary_assign<TAssigneeType>
{
	typedef TArgumentType                  arg1_type;
	typedef typename cref<arg1_type>::type arg1_cref;

	static void PrepareTile(typename sequence_traits<TAssigneeType>::seq_t res, typename sequence_traits<TArgumentType>::cseq_t a1)
	{ /* NOP */ }
};

template<typename TAssigneeType, typename TArgument1Type, typename TArgument2Type>
struct binary_assign : unary_assign<TAssigneeType, TArgument1Type>
{
	typedef TArgument2Type                 arg2_type;
	typedef typename cref<arg2_type>::type arg2_cref;
};

template<typename TAssigneeType, typename TArgument1Type, typename TArgument2Type, typename TArgument3Type>
struct ternary_assign : binary_assign<TAssigneeType, TArgument1Type, TArgument2Type>
{
	typedef TArgument3Type                 arg3_type;
	typedef typename cref<arg3_type>::type arg3_cref;
};

// *****************************************************************************
//							ELEMENTARY UNARY FUNCTORS
// *****************************************************************************

template <typename T>
struct ident_assignment : unary_assign<T, T>
{
	template<typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }
	
	using unary_assign = typename ident_assignment::unary_assign;

	void operator()(typename unary_assign::assignee_ref res, typename unary_assign::arg1_cref arg) const 
	{ 
		res = arg; 
	}
};

template <typename T>
struct ident_function : unary_func<T, T>
{
	template<typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return CastUnit<R>(arg1_values_unit(args)); }

	using unary_func = typename ident_function::unary_func;

	typename unary_func::res_type operator()(typename unary_func::arg1_cref arg) const { return arg; }
};

template <typename TNullaryFunc>
struct nullary_assign_from_func
:	nullary_assign<
		typename TNullaryFunc::res_type
	>
{
	nullary_assign_from_func(TNullaryFunc f = TNullaryFunc())
		:	m_NulFunc(f) 
	{}

	void operator()(typename nullary_assign_from_func::assignee_ref res) const
	{
		res = m_NulFunc();
	}

	TNullaryFunc m_NulFunc;
};

// *****************************************************************************
//								ACCUMULATION FUNCTOR PROTOTYPES
// *****************************************************************************

template <typename TAccumulationType> 
struct nullary_total_accumulation : nullary_assign<TAccumulationType>
{
};

template <typename TAssigneeType, typename TValueType> 
struct unary_total_accumulation : unary_assign<TAssigneeType, typename sequence_traits<TValueType>::cseq_t >
{
	using value_cseq1 = typename unary_total_accumulation::arg1_type;
	using value_type1 = typename value_cseq1::value_type;

	static_assert( std::is_same_v<TValueType, value_type1> );
};

template <typename TAccumulationType, typename TValueType1, typename TValueType2> 
struct binary_total_accumulation
:	binary_assign<
		TAccumulationType,
		typename sequence_traits<TValueType1>::cseq_t,
		typename sequence_traits<TValueType2>::cseq_t
	>
{
	using accumulation_type = typename binary_total_accumulation::assignee_type;
	using accumulation_ref = typename binary_total_accumulation::assignee_ref;

	using value_cseq1 = typename binary_total_accumulation::arg1_type;
	using value_cseq2 = typename binary_total_accumulation::arg2_type;

	using value_type1 = typename value_cseq1::value_type;
	using value_type2 = typename value_cseq2::value_type;

	static_assert( std::is_same_v<TValueType1, value_type1> );
	static_assert( std::is_same_v<TValueType2, value_type2> );
};

template <typename TAccumulationType> 
struct nullary_partial_accumulation
	:	nullary_assign<
			typename sequence_traits<TAccumulationType>::seq_t
		>
{
	typedef	typename nullary_partial_accumulation::assignee_type accumulation_container;
	typedef	typename accumulation_container::value_type accumulation_type;
};

template <typename TAccumulationType, typename TValueType> 
struct unary_partial_accumulation
	:	unary_assign<
			typename sequence_traits<TAccumulationType>::seq_t,
			typename sequence_traits<TValueType  >::cseq_t
		>
{
	using this_type = unary_partial_accumulation;

	typedef typename TAccumulationType            result_type;       // ie. Int32,             bit_value<N>,                 SharedStr
	typedef typename this_type::assignee_type     accumulation_seq;  // ie. IterRange<Int32*>, bit_sequence<N, block_type>,  sequence_array_ref <char>
	typedef typename accumulation_seq::value_type accumulation_type; // ie. Int32,             bit_reference<N, block_type>, SA_Reference<char>

	typedef typename this_type::arg1_type         value_cseq1;
	typedef typename value_cseq1::value_type      value_type1;

	static_assert( std::is_same_v<TValueType, value_type1> );
};

template <typename TAccumulationType, typename TValueType1, typename TValueType2> 
struct binary_partial_accumulation
	:	binary_assign<
			typename sequence_traits<TAccumulationType>:: seq_t,
			typename sequence_traits<TValueType1      >::cseq_t,
			typename sequence_traits<TValueType2      >::cseq_t
		>
{
	using this_type = binary_partial_accumulation;

	typedef typename this_type::assignee_type     accumulation_seq;
	typedef typename accumulation_seq::value_type accumulation_type;

	typedef typename this_type::arg1_type         value_cseq1;
	typedef typename this_type::arg2_type         value_cseq2;

	typedef typename value_cseq1::value_type      value_type1;
	typedef typename value_cseq2::value_type      value_type2;

	static_assert(std::is_same<TValueType1, value_type1>::value);
	static_assert(std::is_same<TValueType2, value_type2>::value);
};


#endif //  !defined(__CLC_PROTOTYPES_H)