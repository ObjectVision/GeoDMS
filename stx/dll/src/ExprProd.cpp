// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ExprProd.h"

#include "vt/Conversions.h"
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

static StaticLateTokenID t_apply_value("apply_value");

void ExprProd::ProdFunctionCall()
{
	// function_call -> head ( list )
	// (arg_list, (head, tail)) -> ((head, arg_list), tail)
	// §5.10: when the head is itself a call (a chained suffix like f(a)(b)), every
	// expression head must stay a plain symbol, so emit a marker form instead:
	// (apply_value (innerCall) arg...)
	LispRef head = m_Result.Second();
	LispRef call;
	if (head.IsRealList())
	{
		static LispRef applyValueHead(t_apply_value);
		call = LispRef(applyValueHead, LispRef(head, m_Result.First()));
	}
	else
		call = LispRefList(head.AsLispPtr(), m_Result.First().AsLispPtr());
	m_Result.repl_back2(RewriteExprTop_InParse(call));
}

void ExprProd::ProdIdentifier(iterator_t first, iterator_t last)
{
	assert(first != last);
	m_Result.push_back(LispRef(GetTokenID_mt(&*first, &*last)));
}

static StaticLateTokenID t_member           ("member");
static StaticLateTokenID t_container_literal("container_literal");
static StaticLateTokenID t_no_domain        ("no_domain");

void ExprProd::ProdContainerMember()
{
	// container_member -> name ( ':' | ':=' ) value
	// (value, name, tail) -> ((member name value), tail)
	static LispRef memberHead(t_member);
	m_Result.repl_back2(
		List3<LispRef>(
			memberHead,
			m_Result.Second(),
			m_Result.First()
		)
	);
}

void ExprProd::ProdContainerLiteral(bool hasDomain)
{
	// with domain:  (members, domain, tail) -> ((container_literal domain m1 m2 …), tail)
	// no domain:    (members, tail)         -> ((container_literal no_domain m1 m2 …), tail)
	static LispRef litHead(t_container_literal);
	static LispRef noDomain(t_no_domain);
	LispRef members = m_Result.First();
	if (hasDomain)
		m_Result.repl_back2(LispRef(litHead, LispRef(m_Result.Second(), members)));
	else
		m_Result.repl_back1(LispRef(litHead, LispRef(noDomain, members)));
}

void ExprProd::throwFunctionLiteralError()
{
	// §5.11: function literals are lifted into hidden declarations at configuration
	// parse; an unlifted literal can only reach this parser through leading-'='
	// string evaluation, which is not supported
	throwErrorD("ExprProd", "a function literal is only supported in configuration calculation rules"
		"; it cannot appear in string-evaluated expressions");
}

static StaticLateTokenID t_uint32("uint32");

void ExprProd::ProdUInt32WithoutSuffix()
{
	// (n tail) -> ((UInt32 n) tail)
	dms_assert(m_Result.back().IsUI64());
	auto value = m_Result.First().GetUI64Val();
	if (value > UInt32(-1))
		throwErrorF("ExprProd", "Cannot convert {} to UInt32, consider adding the suffix u64 or i64", value);
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
#include "vt/StringBounds.h"
#include <algorithm>

// The value-type suffixes of a numeric literal (1d, 2f, 3u, 4i, 5w, 6s, 7b, 8c, and the sized
// forms u64 .. u2) are recognised by TEXT, exactly and in lower case, and are not interned as
// tokens. They used to be: a static table keyed by TokenID registered 'd', 'f', 'u', 'i', 'w',
// 's', 'b' and 'c' at load time, and since the token registry is one case-folded namespace
// shared with every configuration, each item a modeller named D, F, U, I, W, S, B or C -- a type
// parameter `D: domains` as the function examples write it, a unit W for watt, a container C --
// reported a case mix-up against a suffix letter, on every run (#1161, #1262). A suffix is not a
// name that anything looks up by token: the only consumer is this production, and it wants the
// value class, which the text gives directly.
//
// Since GeoDMS 20.21.0 the match is case-sensitive: 60D is no longer 60 as float64. The upper-case
// spelling was never documented, was found in four test configurations and no wiki example, and
// an exact match is what lets D, F, U, I, W, S, B and C be ordinary names: 60D now means 60 in a
// unit called D, the way 5m means 5 in the unit m, and fails like any other unknown identifier
// when there is none.
struct SuffixEntry { CharPtr text; ValueClassID vt; };

static const SuffixEntry suffixTable[] = {
	{ "d",   ValueClassID::VT_Float64 }, { "f",   ValueClassID::VT_Float32 },
	{ "u",   ValueClassID::VT_UInt32  }, { "i",   ValueClassID::VT_Int32   },
	{ "w",   ValueClassID::VT_UInt16  }, { "s",   ValueClassID::VT_Int16   },
	{ "b",   ValueClassID::VT_UInt8   }, { "c",   ValueClassID::VT_Int8    },
	{ "u64", ValueClassID::VT_UInt64  }, { "i64", ValueClassID::VT_Int64   },
	{ "u32", ValueClassID::VT_UInt32  }, { "i32", ValueClassID::VT_Int32   },
	{ "u16", ValueClassID::VT_UInt16  }, { "i16", ValueClassID::VT_Int16   },
	{ "u8",  ValueClassID::VT_UInt8   }, { "i8",  ValueClassID::VT_Int8    },
	{ "u4",  ValueClassID::VT_UInt4   }, { "u2",  ValueClassID::VT_UInt2   },
};

static ValueClassID GetValueTypeOfSuffix(CharPtr first, CharPtr last)
{
	SizeT size = last - first;
	for (const auto& entry : suffixTable)
		if (size == StrLen(entry.text) && std::equal(first, last, entry.text))
			return entry.vt;
	return ValueClassID::VT_Unknown;
}

void ExprProd::ProdSuffix(iterator_t first, iterator_t last)
{
	// (n tail) -> ((cast n) tail) or ((value n unit) tail);
	MG_CHECK(first != last);
	dms_assert(m_Result.back().IsNumb() || m_Result.back().IsUI64());

	const ValueClass* vc = nullptr;
	TokenID suffixToken;
	ValueClassID vt = GetValueTypeOfSuffix(&*first, &*last);
	if (vt != ValueClassID::VT_Unknown)
	{
		if (vt == ValueClassID::VT_Float64 && m_Result.back().IsNumb())
			return;
		if (vt == ValueClassID::VT_UInt64 && m_Result.back().IsUI64())
			return;

		vc = ValueClass::FindByValueClassID(vt);
		MG_CHECK(vc);
		suffixToken = vc->GetNameID();
	}
	else
	{
		// a value-type script name (1float32) or a unit name (5m): those ARE names, so they are
		// interned, and a spelling that differs in case from the registered one is reported.
		suffixToken = GetTokenID_mt(&*first, &*last);
		vc = ValueClass::FindByScriptName(suffixToken);
		if (vc)
			suffixToken = vc->GetNameID(); // the registered name, whatever key the lookup accepted
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

#include "sym/Token.h"
#include "xml/XMLOut.h"
#include "ptr/SharedStr.h"

#include "TreeItem.h"
#include "Xml/XmlTreeOut.h"

const TreeItem* WriteHtmlLink(OutStreamBase& outStream, const TreeItem* searchContext, CharPtr first, CharPtr last)
{
	if (searchContext && first != last)
	{
		TokenID itemRef = TokenID(first, last, (mt_tag*)nullptr);
		if (!ValueClass::FindByScriptName(itemRef))
		{
			try {
				const TreeItem* item = searchContext->ResolveItemPath(CharPtrRange(first, last)).get();
				if (item)
				{
					XML_hRef xmlElemA(outStream, ItemUrl(item).c_str());
					outStream.WriteRange(first, last);
					return item;
				}
			}
			catch (...) {}
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
