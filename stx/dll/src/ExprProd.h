// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__STX_EXPRPROD_H)
#define __STX_EXPRPROD_H

#include "set/StackUtil.h"
#include "LispRef.h"
#include "LispTreeType.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"

#include "StringProd.h"
#include "TextPosition.h"

// §5.11 tier B: receiver of function-literal extents found during the config-side
// expression capture; implemented by ConfigProd (lambda lifting)
struct FunctionLiteralSink
{
	virtual void OnExprFunctionLiteral(CharPtr first, CharPtr last) = 0;
	virtual void OnWidenFunctionLiteral(CharPtr first, CharPtr last) = 0;
};

struct ExprProd
{
	void ProdIIF();
	void ProdBinaryOper(TokenID id);

	void ProdBracketedExpr();
	void ProdUnaryOper(TokenID id);
	void ProdStringValue();
	void ProdFunctionCall();
	void ProdContainerMember();          // §5.9: (member <name> <value>)
	void ProdContainerLiteral(bool hasDomain); // §5.9: (container_literal <domain|no_domain> <member>...)
	// §5.11: literals are lifted at config parse; reaching this parser is an error.
	// Templated: the expression grammar is instantiated with several scanner types.
	[[noreturn]] void throwFunctionLiteralError();
	template <typename Iter> void ProdFunctionLiteral(Iter, Iter) { throwFunctionLiteralError(); }
	template <typename Iter> void WidenFunctionLiteral(Iter, Iter) {}
	void ProdIdentifier(iterator_t first, iterator_t last);
	void ProdUInt64(UInt64 n);
	void ProdFloat64(Float64 x);
	void ProdUInt32WithoutSuffix();
	void ProdSuffix    (iterator_t first, iterator_t last);
	void RefocusAfterArrow() {}
	void RefocusAfterScope() {}
	void ProdArrow() { ProdBinaryOper(token::arrow); }
	void ProdScope() { ProdBinaryOper(token::scope); }

	void StartExprList();
	void CloseExprList();

	MG_DEBUGCODE( bool empty() const; )

	using string_prod_type = StringProd;

	quick_replace_stack<LispRef> m_Result;
	std::vector<SizeT>           m_ExprListBase;
	StringProd                   m_StringProd;
};

struct ExprProdBase
{
	void ProdIIF() {}
	void ProdBinaryOper(TokenID id) {}

	void ProdBracketedExpr() {}
	void ProdUnaryOper(TokenID id) {}
//	void ProdNullaryOper(TokenID id) {}
	void ProdStringValue() {}
	void ProdFunctionCall() {}
	void ProdContainerMember() {}
	void ProdContainerLiteral(bool hasDomain) {}
	template <typename Iter> void ProdFunctionLiteral(Iter, Iter) {}
	template <typename Iter> void WidenFunctionLiteral(Iter, Iter) {}
	void ProdUInt64(UInt64 n) {}
	void ProdFloat64(Float64 x) {}
	void ProdUInt32WithoutSuffix() {}

	void StartExprList() {}
	void CloseExprList() {}
};

struct EmptyExprProd : ExprProdBase
{
	using string_prod_type = StringProdBase;
	StringProdBase m_StringProd;

	void ProdIdentifier(iterator_t first, iterator_t last) {}
	void ProdSuffix(iterator_t first, iterator_t last) {}
	void RefocusAfterArrow() {}
	void RefocusAfterScope() {}
	void ProdArrow() {}
	void ProdScope() {}

	// §5.11 tier B: the config-side capture lifts function literals via the sink.
	// Templated: the expression grammar is instantiated with several scanner types;
	// '&*it' yields the underlying CharPtr for each of them.
	FunctionLiteralSink* m_LiteralSink = nullptr;
	template <typename Iter> void ProdFunctionLiteral(Iter first, Iter last)
	{
		if (m_LiteralSink)
			m_LiteralSink->OnExprFunctionLiteral(&*first, &*last);
	}
	template <typename Iter> void WidenFunctionLiteral(Iter first, Iter last)
	{
		if (m_LiteralSink)
			m_LiteralSink->OnWidenFunctionLiteral(&*first, &*last);
	}
};

struct HtmlProd : ExprProdBase
{
	HtmlProd(OutStreamBase& outStream, const TreeItem* searchContext, SharedStr orgExpr)
		:	m_OutStream(outStream)
		,	m_SearchContext(searchContext)
		,	m_OrgExpr(orgExpr)
		,	m_LastPos(m_OrgExpr.begin())
	{}
	~HtmlProd();

	OutStreamBase&  m_OutStream;
	const TreeItem* m_SearchContext;
	const TreeItem* m_LastIdentifier = nullptr;

	std::vector< const TreeItem*> m_SearchContextStack;

	const SharedStr m_OrgExpr;
	CharPtr m_LastPos;

	void ProdIdentifier(CharPtr first, CharPtr last);
	void ProdSuffix(CharPtr first, CharPtr last) {}

	void RefocusAfterArrow() 
	{
		m_SearchContextStack.emplace_back(m_SearchContext);
		if (IsDataItem(m_LastIdentifier))
			m_SearchContext = AsDataItem(m_LastIdentifier)->GetAbstrValuesUnit();
		else
			m_SearchContext = m_LastIdentifier;
	}
	void RefocusAfterScope()
	{
		m_SearchContextStack.emplace_back(m_SearchContext);
		m_SearchContext = m_LastIdentifier;
	}

	void popSearchContext()
	{
		assert(!m_SearchContextStack.empty());
		m_SearchContext = m_SearchContextStack.back(); m_SearchContextStack.pop_back();
	}

	void ProdArrow() { popSearchContext(); }
	void ProdScope() { popSearchContext(); }

	using string_prod_type = StringProd;
	StringProd  m_StringProd;
};

#endif //!defined(__STX_EXPRPROD_H)
