// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Shared machinery of the binary attribute operators, split out of
 *  OperAttrBin.cpp (2026-08) so the instantiation farm can be partitioned
 *  over parallel TUs (OperAttrBin_muldiv/_addsub/_compare/_bits.cpp + the
 *  residual OperAttrBin.cpp for string/pow/unit operators):
 *  - dist/sqrdist functors over point types;
 *  - the checked comparison/logical/bitwise functor family (equal_to,
 *    not_equal_to, greater(_equal), less(_equal), logical_and/or,
 *    binary_and/or/eq/xor, incl. the bit_value<N> block specializations);
 *  - BinaryAttrFuncOper: the BinaryAttrOper that computes one tile with a
 *    binary_func functor via do_binary_func;
 *  - BinaryInstantiation / CogBinaryInstantiation: instantiate such an
 *    operator template over a typelist (optionally owning its OperGroup).
 */

#if !defined(__CLC_OPERATTRBINIMPL_H)
#define __CLC_OPERATTRBINIMPL_H

#include "mci/CompositeCast.h"
#include "vt/CheckedCalc.h"

#include "AttrBinStruct.h"
#include "LispTreeType.h"
#include "OperAttrBin.h"
#include "OperUnit.h"
#include "UnitCreators.h"

// *****************************************************************************
//											DEFINES
// *****************************************************************************

template <typename P> using norm_type_t = product_type_t<scalar_of_t<P>>;
template <typename P> using dist_type_t = div_type_t<norm_type_t<P>>;

template <typename P> struct sqrdist_func: binary_func<norm_type_t<P>, P, P>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<norm_type_t<P>>(); }

	auto operator ()(cref_t<P> a, cref_t<P> b) const
	{
		return Norm<norm_type_t<P>>(a - b);
	}
};

template <typename P> struct dist_func: binary_func<dist_type_t<P>, P, P>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<typename dist_func::res_type>(); }

	dist_type_t<P> operator ()(cref_t<P> a, cref_t<P> b) const
	{
		return sqrt( Norm<Float64>(a - b ) );
	}
};

// *****************************************************************************
//		BINARY FUNCTORS TAKEN FROM std
// *****************************************************************************

template <typename T> struct compare_func_base : binary_func<Bool, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return compare_unit_creator(gr, args, true); }

};

template <typename T, typename Cmp> struct checked_compare_func : compare_func_base<T>
{
	Bool operator()(cref_t<T> a, cref_t<T> b) const
	{
		if constexpr (has_undefines_v<T>)
		{
			if (!IsDefined(a) || !IsDefined(b))
				return false;
//				throwDmsErrD("Invalid attempt to compare undefined values!");
		}
		return cmp(a, b);
	}
	Cmp cmp;
};

template <typename T> struct equal_to : compare_func_base<T>
{
	Bool operator ()(cref_t<T> a, cref_t<T> b) const
	{
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(a) || !IsDefined(b))
				return false;
		return a == b;
	}
};

template <typename T> struct not_equal_to : compare_func_base<T>
{
	Bool operator ()(cref_t<T> a, cref_t<T> b) const
	{
		if constexpr (has_undefines_v<T>)
			if (!IsDefined(a) || !IsDefined(b))
				return false;
		return !(a == b);
	}
};

template <typename T> struct greater : checked_compare_func < T, decltype([](cref_t<T> a, cref_t<T> b) -> Bool { return b < a; }) > {};
template <typename T> struct greater_equal : checked_compare_func < T, decltype([](cref_t<T> a, cref_t<T> b) -> Bool { return !(a < b); }) > {};
template <typename T> struct less: checked_compare_func < T, decltype([](cref_t<T> a, cref_t<T> b) -> Bool { return a < b; }) > {};
template <typename T> struct less_equal: checked_compare_func < T, decltype([](cref_t<T> a, cref_t<T> b) -> Bool { return !(b < a); }) > {};

template <typename T> struct logical_and : compare_func_base<T> { Bool operator ()(cref_t<T> a, cref_t<T> b) const { return a && b; } };
template <typename T> struct logical_or  : compare_func_base<T> { Bool operator ()(cref_t<T> a, cref_t<T> b) const { return a || b; } };

template <typename T> struct binary_base: binary_func<T, T, T> {
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
	{
		return compatible_values_unit_creator_func(0, gr, args, false);
	}
};

template<typename T> struct is_safe_for_undefines<equal_to<T>> : std::true_type {};
template<typename T> struct is_safe_for_undefines<not_equal_to<T>> : std::true_type{};
template<typename T> struct is_safe_for_undefines<greater<T>> : std::true_type{};
template<typename T> struct is_safe_for_undefines<greater_equal<T>> : std::true_type{};
template<typename T> struct is_safe_for_undefines<less<T>> : std::true_type{};
template<typename T> struct is_safe_for_undefines<less_equal<T>> : std::true_type{};

template <typename T> struct binary_and : binary_base<T> { T operator()(cref_t<T> a, cref_t<T> b) const { return a&b; } };
template <typename T> struct binary_or  : binary_base<T> { T operator()(cref_t<T> a, cref_t<T> b) const { return a|b; } };
template <typename T> struct binary_eq  : binary_base<T> { T operator()(cref_t<T> a, cref_t<T> b) const { return ~(a^b); } };
template <typename T> struct binary_xor : binary_base<T> { T operator()(cref_t<T> a, cref_t<T> b) const { return (a^b); } };

template <bit_size_t N> struct binary_and<bit_value<N> >   : binary_base<bit_value<N> >
{
	using block_func = binary_and<bit_block_t>;

	block_func m_BlockFunc;
};
template <bit_size_t N> struct binary_or<bit_value<N> >   : binary_base<bit_value<N> >
{
	using block_func = binary_or<bit_block_t>;

	block_func m_BlockFunc;
};
template <> struct logical_and<Bool> : binary_and<Bool> {};
template <> struct logical_or <Bool> : binary_or <Bool> {};

template <> struct equal_to<Bool> : binary_base<Bool>
{
	using block_func = binary_eq<bit_block_t>;

	block_func m_BlockFunc;
};

template <> struct not_equal_to<Bool>: binary_base<Bool>
{
	using block_func = binary_xor<bit_block_t>;

	block_func m_BlockFunc;
};

// *****************************************************************************
//			BinaryAttrFunc operators
// *****************************************************************************

template <typename BinOper>
struct BinaryAttrFuncOper : BinaryAttrOper<typename BinOper::res_type, typename BinOper::arg1_type, typename BinOper::arg2_type>
{
	BinaryAttrFuncOper(AbstrOperGroup* gr)
		: BinaryAttrOper<typename BinOper::res_type, typename BinOper::arg1_type, typename BinOper::arg2_type>(gr, BinOper::unit_creator, composition_of<typename BinOper::res_type>::value)
	{}

	void CalcTile(sequence_traits<typename BinOper::res_type>::seq_t resData, sequence_traits<typename BinOper::arg1_type>::cseq_t arg1Data, sequence_traits<typename BinOper::arg2_type>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		do_binary_func(resData, arg1Data, arg2Data, BinOper(), af & AF1_ISPARAM, af & AF2_ISPARAM, af & AF1_HASUNDEFINED, af & AF2_HASUNDEFINED);
	}
};

// *****************************************************************************
//											INSTANTIATION helpers
// *****************************************************************************

template <typename TL, template <typename T> class MetaFunc>
struct BinaryInstantiation{

	template <typename T> struct OperTemplate : BinaryAttrFuncOper<MetaFunc<T> >
	{
		using BinaryAttrFuncOper<MetaFunc<T> >::BinaryAttrFuncOper; // <MetaFunc<T> >; // inherit constructors
	};

	tl_oper::inst_tuple_templ<TL, OperTemplate> m_OperList;

	BinaryInstantiation(AbstrOperGroup* gr)
		: m_OperList(gr)
	{}

};

template <typename TL, template <typename T> class MetaFunc>
struct CogBinaryInstantiation : CommonOperGroup
{
	CogBinaryInstantiation(CharPtr cogName)
		:	CommonOperGroup(cogName)
		,	m_Instantiations(this)
	{}

	BinaryInstantiation<TL, MetaFunc> m_Instantiations;
};

#include "RtcTypeLists.h"

#endif // __CLC_OPERATTRBINIMPL_H
