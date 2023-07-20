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

#include "StxPCH.h"
#pragma hdrstop

#include "ExprProd.h"

#include "geo/Conversions.h"
#include "Parallel.h"

#include "LispList.h"
#include "ExprRewrite.h"

#if defined(MG_DEBUG)
bool ExprProd::empty() const
{
	return m_Result.empty() && m_ExprListBase.empty() && m_StringProd.empty();
}
#endif

void ExprProd::ProdIIF()
{
	static LispRef iifHead(token::iif);
	// exprL0 -> exprL1 ? exprL1 : exprL1 
	// (expr3, expr2, expr1, tail) -> (('iif', expr1, expr2, expr3), tail)
	m_Result.repl_back3(
		RewriteExprTop_InParse(
			List4<LispRef>(
				iifHead, 
				m_Result.Third(), 
				m_Result.Second(), 
				m_Result.First()
			)
		)
	);
}

void ExprProd::ProdBinaryOper(TokenID id)
{
	// expr -> expr2 OPER expr1
	// (expr1, expr2, tail) -> ((id, expr2, expr1), tail)
	m_Result.repl_back2(
		RewriteExprTop_InParse(
			List3<LispRef>(
				LispRef(id), 
				m_Result.Second(),
				m_Result.First()
			)
		)
	);
}

void ExprProd::ProdBracketedExpr()
{
	static LispRef lookupLispref = LispRef(token::lookup);
	// expr -> expr1 [ expr2 ]
	// (expr2, expr1, tail) -> ((id, expr2, expr1), tail)
	m_Result.repl_back2(
		RewriteExprTop_InParse(
			List3<LispRef>(
				lookupLispref, 
				m_Result.First(),
				m_Result.Second()
			)
		)
	);
}
		
void ExprProd::ProdUnaryOper(TokenID id)
{
	// expr -> OPER(exprL1)
	// (expr1, tail) -> ((id, expr1), tail)
	m_Result.repl_back1(
		RewriteExprTop_InParse(
			List2<LispRef>(
				LispRef(id), 
				m_Result.First()
			)
		)
	);
}
		
void ExprProd::ProdStringValue()
{
	// tail -> (string, tail)
	m_Result.push_back(
		m_StringProd.m_StringValue.empty()
			?	LispRef(CharPtr(0), CharPtr(0) )
			:	LispRef(m_StringProd.m_StringValue.begin(), m_StringProd.m_StringValue.send() )
	);
}

void ExprProd::ProdFunctionCall()
{
	// function_call -> function_name ( list ) 
	// (arg_list, (function_name, tail)) -> ((function_name, arg_list), tail)
	m_Result.repl_back2(
		RewriteExprTop_InParse(
			LispRefList(
				m_Result.Second()
			,	m_Result.First()
			)
		)
	);
}

void ExprProd::ProdIdentifier(iterator_t first, iterator_t last)
{
	dms_assert(first != last);
	m_Result.push_back(LispRef(GetTokenID_mt(&*first, &*last)));
}

static TokenID t_uint32 = GetTokenID_st("UInt32");

void ExprProd::ProdUInt32WithoutSuffix()
{
	// (n tail) -> ((UInt32 n) tail)
	dms_assert(m_Result.back().IsUI64());
	auto value = m_Result.First().GetUI64Val();
	if (value > UInt32(-1))
		throwErrorF("ExprProd", "Cannot convert %u to UInt32, consider adding the suffix u64 or i64", value);
	static LispRef uint32Head(t_uint32);
	m_Result.repl_back1(
			List2<LispRef>(
				uint32Head
			,	m_Result.back()
			)
		);
}

void ExprProd::ProdUInt64(UInt64 n)
{
	// (integerValue -> NUMBER ) >> (signature | end_p )
	// (tail) -> (n tail)
	m_Result.push_back(LispRef(n));
}

void ExprProd::ProdFloat64(Float64 x)
{
	// floatValue -> integerValue . NUMBER 
	// (tail) -> (x tail)
	m_Result.push_back(LispRef(Number(x)));
}

#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "set/VectorMap.h"

typedef vector_map<TokenID, ValueClassID> map_type;
typedef map_type::value_type P;

static P suffixArray[] = {
	P(GetTokenID_st("d"),   VT_Float64), P(GetTokenID_st("f"),   VT_Float32),
	P(GetTokenID_st("u"),   VT_UInt32), P(GetTokenID_st("i"),   VT_Int32),
	P(GetTokenID_st("w"),   VT_UInt16), P(GetTokenID_st("s"),   VT_Int16),
	P(GetTokenID_st("b"),   VT_UInt8), P(GetTokenID_st("c"),   VT_Int8),
	P(GetTokenID_st("u64"), VT_UInt64), P(GetTokenID_st("i64"), VT_Int64),
	P(GetTokenID_st("u32"), VT_UInt32), P(GetTokenID_st("i32"), VT_Int32),
	P(GetTokenID_st("u16"), VT_UInt16), P(GetTokenID_st("i16"), VT_Int16),
	P(GetTokenID_st("u8"),  VT_UInt8), P(GetTokenID_st("i8") , VT_Int8),
	P(GetTokenID_st("u4"),  VT_UInt4), P(GetTokenID_st("u2") , VT_UInt2),
};

ValueClassID GetValueType(TokenID suffix)
{
	static map_type suffixMap(suffixArray, suffixArray + sizeof(suffixArray) / sizeof(P));

	if (suffix <= suffixMap.back().first)
	{
		map_type::const_iterator iter = suffixMap.find(suffix);
		if (iter != suffixMap.end())
			return iter->second;
	}
	return VT_Unknown;

}

void ExprProd::ProdSuffix(iterator_t first, iterator_t last)
{
	// (n tail) -> ((cast n) tail) or ((value n unit) tail);
	dms_assert(first != last);
	dms_assert(m_Result.back().IsNumb() || m_Result.back().IsUI64());

	TokenID suffixToken = GetTokenID_mt(&*first, &*last);

	const ValueClass* vc = nullptr;
	ValueClassID vt = GetValueType(suffixToken);
	if (vt != VT_Unknown)
	{
		if (vt == VT_Float64 && m_Result.back().IsNumb())
			return;
		if (vt == VT_UInt64 && m_Result.back().IsUI64())
			return;

		vc = ValueClass::FindByValueClassID(vt);
		dms_assert(vc);
		suffixToken = vc->GetID();
	}
	else
	{
		vc = ValueClass::FindByScriptName(suffixToken);
		dms_assert(!vc || suffixToken == vc->GetID());
	}

	if (vc)
	{
		m_Result.repl_back1(List2<LispRef>(LispRef(suffixToken), m_Result.back()));
		return;
	}

	m_Result.repl_back1(slConvertedLispExpr(LispRef(m_Result.back()), LispRef(suffixToken)));
}

void ExprProd::StartExprList()
{
	m_ExprListBase.push_back(m_Result.size());
}

void ExprProd::CloseExprList()
{
	UInt32 listSize = m_ExprListBase.back();
	m_ExprListBase.pop_back();

	LispRef list;
	while (m_Result.size() > listSize)
	{
		list = LispRef(m_Result.First(), list);
		m_Result.pop_back();
	}
	m_Result.push_back(list);
}

#include "set/Token.h"
#include "xml/XMLOut.h"
#include "ptr/SharedStr.h"

#include "TreeItem.h"
#include "xml/XmlTreeOut.h"

const TreeItem* WriteHtmlLink(OutStreamBase& outStream, const TreeItem* searchContext, CharPtr first, CharPtr last)
{
	TokenID itemRef = TokenID(first, last, (mt_tag*)nullptr);
	if (!ValueClass::FindByScriptName(itemRef))
	{
		const TreeItem* item = searchContext->FindItem(CharPtrRange(first, last));
		if (item)
		{
			XML_hRef xmlElemA(outStream, ItemUrl(item).c_str());
			outStream.WriteRange(first, last);
			return item;
		}
	}
	outStream.WriteRange(first, last);
	return nullptr;
}
	
void HtmlProd::ProdIdentifier(CharPtr first, CharPtr last)
{
	m_OutStream.WriteRange(m_LastPos, first);
	m_LastIdentifier = WriteHtmlLink(m_OutStream, m_SearchContext, first, last);
	m_LastPos = last;
}

HtmlProd::~HtmlProd()
{
	m_OutStream.WriteRange(m_LastPos, m_OrgExpr.csend());
}

