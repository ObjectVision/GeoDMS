// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_PROTOTYPES_H)
#define __CLC_PROTOTYPES_H

#include "RtcBase.h"

template <typename T> struct null_wrap;

template <typename T>
struct is_seq_ref : std::false_type {};

template <typename T>
struct is_seq_ref<SA_Reference<T>> : std::true_type {};

template <typename T>
struct is_seq_ref<SA_ConstReference<T>> : std::true_type {};

template <typename T>
constexpr bool is_seq_ref_v = is_seq_ref<T>::value;

template <typename T>
struct is_null_wrap_t : std::false_type {};

template <typename T>
struct is_null_wrap_t<null_wrap<T>> : std::true_type {};

template <typename T>
constexpr bool is_null_wrap_v = is_null_wrap_t<T>::value;

template <typename T>
constexpr bool can_be_undefined_v = !is_bitvalue_v<T> && (is_fixed_size_element_v<T> || is_seq_ref_v<T> || is_null_wrap_v<T>);

template <typename T>
using nullable_t = std::conditional_t<has_undefines_v<T>&& is_fixed_size_element_v<T>, T, null_wrap<T>>;

// *****************************************************************************
//								Function predicates
// *****************************************************************************

template <typename Func> struct is_safe_for_undefines : std::false_type {};

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
using IndexGetter = AbstrValueGetter<SizeT> ;


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
template <typename E> struct cref<std::vector<E> > { using type = typename sequence_traits<std::vector<E> >::container_type::const_reference; };
template <>           struct cref<SharedStr     > { using type = typename sequence_traits<SharedStr>::container_type::const_reference; };

// func results
template <typename T> struct fref { using type = typename sequence_traits<T>::container_type::reference; };
template <typename T> using fref_t = typename fref<T>::type;

// assign results
template<typename T> struct vref : fref<T> {};
template<>           struct vref<OutStreamBuff> { using type = OutStreamBuff&; };
template <typename T> using vref_t = typename vref<T>::type;

template <typename T> T make_result(T&& output) { return std::forward<T>(output); }
template <typename T> using cref_t = typename cref<T>::type;

// *****************************************************************************
//									FUNCTOR PROTOTYPES
// *****************************************************************************

template <typename _R>
struct nullary_func
{
	typedef _R                            res_type; 
	typedef typename fref<res_type>::type res_ref;
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
{};

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
	using assignee_type = TAssigneeType;
	using assignee_ref = vref_t<assignee_type>;
};

template<typename TAssigneeType, typename TArgumentType>
struct unary_assign : nullary_assign<TAssigneeType>
{
	using arg1_type = TArgumentType;
	using arg1_cref = cref_t<arg1_type>;

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

	void operator()(typename unary_assign::assignee_ref res, auto&& arg) const 
	{ 
		Assign(res, arg); 
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

	void operator()(auto&& res) const
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

// *****************************************************************************
//							ELEMENTARY UNARY FUNCTORS
// *****************************************************************************

namespace impl {
	struct dms_result_type_getter_functor
	{
		template <typename Oper> using apply = Oper::dms_result_type;
	};

	struct assignee_type_getter_functor
	{
		template <typename Oper> using apply = Oper::assignee_type;
	};

	namespace has_dms_result_type_details {
		template< typename U> static char test(typename U::dms_result_type* v);
		template< typename U> static int  test(...);
	}

	template< typename T>
	struct has_dms_result_type
	{
		static const bool value = sizeof(has_dms_result_type_details::test<T>(nullptr)) == sizeof(char);
	};
}

template <typename TAcc1Func>
using dms_result_type_t = std::conditional_t<impl::has_dms_result_type<TAcc1Func>::value
	, impl::dms_result_type_getter_functor
	, impl::assignee_type_getter_functor>::template apply<TAcc1Func>;



#endif //  !defined(__CLC_PROTOTYPES_H)