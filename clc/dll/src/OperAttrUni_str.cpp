// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The string-valued unary attribute operators: AsString, quote/unquote,
// dquote/undquote, Url/Html En/Decode, to_utf/from_utf, AsItemName,
// UpperCase/LowerCase, AsHex, strlen/strlen64, trim/ltrim/rtrim.
// Split from OperAttrUni.cpp (2026-08) for parallel compilation.

#include "OperAttrUni.h"
#include "UnitCreators.h"

#include "AttrUniStructStr.h"

#include "OperConv.h" // GetUnitGroup<SharedStr>()

#include "Prototypes.h"
#include "RtcTypeLists.h"

namespace
{
	#define UNARY_TL_INSTANTIATION(TL, M, MetaFunc, Group) \
		tl_oper::inst_tuple<TL, tl::bind_placeholders<UnaryAttr##M##Operator, MetaFunc<ph::_1> >> s_##TL##MetaFunc(Group)

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

	using namespace typelists;

	UNARY_TL_INSTANTIATION(numerics,    Assign,      asstring_assign,     GetUnitGroup<SharedStr>());
	UNARY_TL_INSTANTIATION(points,      Assign,      asstring_assign,     GetUnitGroup<SharedStr>());

	UnaryAttrAssignOperator<    quote_assign> s_QuoteAssign(&cog_quote);
	UnaryAttrAssignOperator<  unquote_assign> s_UnquoteAssign(&cog_unquote);
	UnaryAttrAssignOperator<   dquote_assign> s_DQuoteAssign(&cog_dquote);
	UnaryAttrAssignOperator< undquote_assign> s_UnDquoteAssign(&cog_undquote);
	UnaryAttrAssignOperator<urldecode_assign> s_UrlDecodeAssign(&cog_urldecode);
	UnaryAttrAssignOperator<urlencode_assign> s_UrlEncodeAssign(&cog_urlencode);   // #1177: was a registered name without an implementation
	UnaryAttrAssignOperator<htmldecode_assign> s_HtmlDecodeAssign(&cog_htmldecode); // idem
	UnaryAttrAssignOperator<htmlencode_assign> s_HtmlEncodeAssign(&cog_htmlencode); // idem
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

	UnaryAttrFuncOperator<strlen32_func> g_strlenU32(&cog_strlen32);
	UnaryAttrFuncOperator<strlen64_func> g_strlenU64(&cog_strlen64);

#define INST(X) \
	CommonOperGroup cog##X(#X); \
	UnaryAttrAssignOperator<X##_assign> g_U##X(&cog##X); \

	INST(trim);
	INST(ltrim);
	INST(rtrim);
#undef INST
}
