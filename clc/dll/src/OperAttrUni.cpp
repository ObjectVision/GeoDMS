// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The numeric unary attribute operators: neg, exp/log/sin/cos/tan/atan/sqrt,
// Round* families, not/complement, IsNull/IsDefined/isPositive/isNegative/isZero,
// and the unit-level sqrt. The string-valued operators were split into
// OperAttrUni_str.cpp (2026-08) for parallel compilation.

#include "OperAttrUni.h"
#include "UnitCreators.h"

#include "AttrUniStructNum.h"

#include "OperUnit.h"

#include "Prototypes.h"

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

template<typename T> struct complement_func_base: unary_func<T, T> 
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return arg1_values_unit(args); }
};

template<typename T> 
struct complement_func: complement_func_base<T> 
{
	typename complement_func::res_type operator()(typename complement_func::arg1_cref x) const
	{
		return ~x;
	}
};

template <bit_size_t N> struct complement_func<bit_value<N> > : complement_func_base<bit_value<N> >
{
	using block_func = complement_func<bit_block_t>;
	block_func m_BlockFunc;
};

template<typename T> struct logical_not_func : std_unary_func<std::logical_not<T>, Bool, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return boolean_unit_creator(gr, args); }
};

template <> struct logical_not_func<Bool> : complement_func<Bool> {};


#include "RtcTypeLists.h"
#include "vt/Round.h"

namespace 
{
	CommonOperGroup cog_Round("Round"), cog_RoundUp("RoundUp"), cog_RoundDown("RoundDown"), cog_RoundToZero("RoundToZero"),
		cog_Round_64("Round_64"), cog_RoundUp_64("RoundUp_64"), cog_RoundDown_64("RoundDown_64"), cog_RoundToZero_64("RoundToZero_64");

	template <typename TR, typename TA>
	struct RoundOpers
	{
		struct RoundOperWrap : unary_func<TR, TA>
		{
			static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return cast_unit_creator<TR>(args); }
		};

		struct RoundWrap   : RoundOperWrap { TR operator ()(TA arg) const { return Round      <sizeof(TR)>(arg); } };
		struct RoundUpWrap : RoundOperWrap { TR operator ()(TA arg) const { return RoundUp    <sizeof(TR)>(arg); } };
		struct RoundDnWrap : RoundOperWrap { TR operator ()(TA arg) const { return RoundDown  <sizeof(TR)>(arg); } };
		struct RoundTZWrap : RoundOperWrap { TR operator ()(TA arg) const { return RoundToZero<sizeof(TR)>(arg); } };

		UnaryAttrSpecialFuncOperator<RoundWrap  > m_Round;
		UnaryAttrSpecialFuncOperator<RoundUpWrap> m_RoundUp;
		UnaryAttrSpecialFuncOperator<RoundDnWrap> m_RoundDn;
		UnaryAttrSpecialFuncOperator<RoundTZWrap> m_RoundTZ;

		static const bool result_more_than_4_bytes = (sizeof(scalar_of_t<TR>) > 4);
		RoundOpers()
			: m_Round(result_more_than_4_bytes ? &cog_Round_64 : &cog_Round)
			, m_RoundUp(result_more_than_4_bytes ? &cog_RoundUp_64 : &cog_RoundUp)
			, m_RoundDn(result_more_than_4_bytes ? &cog_RoundDown_64 : &cog_RoundDown)
			, m_RoundTZ(result_more_than_4_bytes ? &cog_RoundToZero_64 : &cog_RoundToZero)
		{}
	};

	#define UNARY_TL_INSTANTIATION(TL, M, MetaFunc, Group) \
		tl_oper::inst_tuple<TL, tl::bind_placeholders<UnaryAttr##M##Operator, MetaFunc<ph::_1> >> s_##TL##MetaFunc(Group)

	CommonOperGroup cog_neg("neg");
	CommonOperGroup cog_not("not");
	CommonOperGroup cog_complement("complement");
	CommonOperGroup cog_exp("exp");
	CommonOperGroup cog_log("log");
	CommonOperGroup cog_sin("sin");
	CommonOperGroup cog_cos("cos");
	CommonOperGroup cog_tan("tan");
	CommonOperGroup cog_atan("atan");
	CommonOperGroup cog_sqrt("sqrt");
	CommonOperGroup cog_IsNull("IsNull");
	CommonOperGroup cog_IsDefined("IsDefined");
	CommonOperGroup cog_IsPositive("isPositive");
	CommonOperGroup cog_IsNegative("isNegative");
	CommonOperGroup cog_IsZero("isZero");

	// Rounding of floats / float_points to ints and int points

	RoundOpers<Int32, Float32> s_RoundF32;
	RoundOpers<Int32, Float64> s_RoundF64;
	RoundOpers<Int64, Float32> s_RoundF32_64;
	RoundOpers<Int64, Float64> s_RoundF64_64;
	RoundOpers<IPoint, FPoint> s_RoundDP;
	RoundOpers<IPoint, DPoint> s_RoundSP;

	using namespace typelists;

	UNARY_TL_INSTANTIATION(num_objects, Func,        neg_func_unchecked,  &cog_neg);
	UNARY_TL_INSTANTIATION(floats,      SpecialFunc, exp_func_checked,    &cog_exp);
	UNARY_TL_INSTANTIATION(floats,      SpecialFunc, log_func_checked,    &cog_log);
	UNARY_TL_INSTANTIATION(floats,      Func,        sin_func,            &cog_sin);
	UNARY_TL_INSTANTIATION(floats,      Func,        cos_func,            &cog_cos);
	UNARY_TL_INSTANTIATION(floats,      Func,        tan_func,            &cog_tan);
	UNARY_TL_INSTANTIATION(floats,      Func,        atan_func,           &cog_atan);
	UNARY_TL_INSTANTIATION(num_objects, SpecialFunc, sqrt_func_checked,   &cog_sqrt);




	UNARY_TL_INSTANTIATION(numerics, Func, logical_not_func, &cog_not);
	UNARY_TL_INSTANTIATION(aints, Func, complement_func, &cog_complement);

	UNARY_TL_INSTANTIATION(value_elements, SpecialFunc, is_undefined_func, &cog_IsNull);
	UNARY_TL_INSTANTIATION(value_elements, SpecialFunc, is_defined_func,   &cog_IsDefined);
	UNARY_TL_INSTANTIATION(num_objects,    SpecialFunc, is_positive_func,  &cog_IsPositive);
	UNARY_TL_INSTANTIATION(num_objects,    SpecialFunc, is_negative_func,  &cog_IsNegative);
	UNARY_TL_INSTANTIATION(numerics,       SpecialFunc, is_zero_func,      &cog_IsZero);




	tl_oper::inst_tuple_templ<num_objects, UnitSqrtOperator> g_UnitSqrtOper(&cog_sqrt);
}
