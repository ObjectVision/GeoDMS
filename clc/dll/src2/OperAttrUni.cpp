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

#include "ClcPCH.h"
#pragma hdrstop

#include "OperAttrUni.h"
#include "UnitCreators.h"

#include "AttrUniStructNum.h"
#include "AttrUniStructStr.h"

#include "OperUnit.h"
#include "UnitGroup.h"

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
#include "geo/Round.h"

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

		static const bool result_more_than_4_bytes = (sizeof(typename scalar_of<TR>::type) > 4);
		RoundOpers()
			: m_Round(result_more_than_4_bytes ? &cog_Round_64 : &cog_Round)
			, m_RoundUp(result_more_than_4_bytes ? &cog_RoundUp_64 : &cog_RoundUp)
			, m_RoundDn(result_more_than_4_bytes ? &cog_RoundDown_64 : &cog_RoundDown)
			, m_RoundTZ(result_more_than_4_bytes ? &cog_RoundToZero_64 : &cog_RoundToZero)
		{}
	};

	#define UNARY_TL_INSTANTIATION(TL, M, MetaFunc, Group) \
		tl_oper::inst_tuple<TL, UnaryAttr##M##Operator<MetaFunc<_> >, AbstrOperGroup*> s_##TL##MetaFunc(Group)

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
	CommonOperGroup cog_unquote("unquote");
	CommonOperGroup cog_quote("quote");
	CommonOperGroup cog_undquote("undquote");
	CommonOperGroup cog_dquote("dquote");
	CommonOperGroup cog_urldecode("UrlDecode");
	CommonOperGroup cog_urlencode("UrlEncode");
	CommonOperGroup cog_htmldecode("HtmlDecode");
	CommonOperGroup cog_htmlencode("HtmlEncode");
	CommonOperGroup cog_toUtf("to_utf");
	CommonOperGroup cog_fromUtf("from_utf");
	CommonOperGroup cog_asItemName("AsItemName");
	CommonOperGroup cog_UpperCase("UpperCase");
	CommonOperGroup cog_LowerCase("LowerCase");
	CommonOperGroup cog_strlen32("strlen");
	CommonOperGroup cog_strlen64("strlen64");
	CommonOperGroup cog_AsHex("AsHex");
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
	UNARY_TL_INSTANTIATION(numerics,    Assign,      asstring_assign,     GetUnitGroup<SharedStr>());
	UNARY_TL_INSTANTIATION(points,      Assign,      asstring_assign,     GetUnitGroup<SharedStr>());

//	UnaryAttrOperator<asstring_function<SPoint> > fSPointasString("SharedStr", true);


	UnaryAttrAssignOperator<    quote_assign> s_QuoteAssign(&cog_quote);
	UnaryAttrAssignOperator<  unquote_assign> s_UnquoteAssign(&cog_unquote);
	UnaryAttrAssignOperator<   dquote_assign> s_DQuoteAssign(&cog_dquote);
	UnaryAttrAssignOperator< undquote_assign> s_UnDquoteAssign(&cog_undquote);
	UnaryAttrAssignOperator<urldecode_assign> s_UrlDecodeAssign(&cog_urldecode);
	UnaryAttrAssignOperator<to_utf_assign   > s_ToUtfAssign(&cog_toUtf);
	UnaryAttrAssignOperator<from_utf_assign > s_FromUtfAssign(&cog_fromUtf);
	UnaryAttrAssignOperator<item_name_assign> s_AsItemName(&cog_asItemName);
	UnaryAttrAssignOperator<UpperCase_assign> s_UpperCaseAssign(&cog_UpperCase);
	UnaryAttrAssignOperator<LowerCase_assign> s_LowerCaseAssign(&cog_LowerCase);

	UnaryAttrAssignOperator<ashex_assign<UInt4   > >  g_AsHexAssignUI04(&cog_AsHex);
	UnaryAttrAssignOperator<ashex_assign<UInt8   > >  g_AsHexAssignUI08(&cog_AsHex);
	UnaryAttrAssignOperator<ashex_assign<UInt16   > >  g_AsHexAssignUI16(&cog_AsHex);
	UnaryAttrAssignOperator<ashex_assign<UInt32   > >  g_AsHexAssignUI32(&cog_AsHex);
	UnaryAttrAssignOperator<ashex_assign<UInt64   > >  g_AsHexAssignUI64(&cog_AsHex);
	UnaryAttrAssignOperator<ashex_assign<SharedStr> >  g_AsHexAssignStr(&cog_AsHex);

	UNARY_TL_INSTANTIATION(numerics, Func, logical_not_func, &cog_not);
	UNARY_TL_INSTANTIATION(aints, Func, complement_func, &cog_complement);

	UNARY_TL_INSTANTIATION(value_elements, SpecialFunc, is_undefined_func, &cog_IsNull);
	UNARY_TL_INSTANTIATION(value_elements, SpecialFunc, is_defined_func,   &cog_IsDefined);
	UNARY_TL_INSTANTIATION(num_objects,    SpecialFunc, is_positive_func,  &cog_IsPositive);
	UNARY_TL_INSTANTIATION(num_objects,    SpecialFunc, is_negative_func,  &cog_IsNegative);
	UNARY_TL_INSTANTIATION(numerics,       SpecialFunc, is_zero_func,      &cog_IsZero);


	UnaryAttrFuncOperator<strlen32_func> g_strlenU32(&cog_strlen32);
	UnaryAttrFuncOperator<strlen64_func> g_strlenU64(&cog_strlen64);

#define INST(X) \
	CommonOperGroup cog##X(#X); \
	UnaryAttrAssignOperator<X##_assign> g_U##X(&cog##X); \

	INST(trim);
	INST(ltrim);
	INST(rtrim);
#undef INST

	tl_oper::inst_tuple<num_objects, UnitSqrtOperator<_>, AbstrOperGroup*> g_UnitSqrtOper(&cog_sqrt);
}
