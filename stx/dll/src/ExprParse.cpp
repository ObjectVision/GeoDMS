// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "SpiritTools.h"

#include "DataBlockParse.h"
#include "ExprParse.h"
#include "ExprProd.h"
#include "ParseExpr.h"

#include "act/MainThread.h"
#include "xml/XmlTreeOut.h"
#include "dbg/DmsCatch.h"

///////////////////////////////////////////////////////////////////////////////
//
//  Parse a range of characters
//
///////////////////////////////////////////////////////////////////////////////
//using namespace boost::spirit;

#if defined(MG_DEBUG)
	std::atomic<UInt32> sd_ParseExprReentrantCheck = 0;
#endif

LispRef parseExpr(CharPtr exprBegin, CharPtr exprEnd)
{
	dms_assert(IsMetaThread());
#if defined(MG_DEBUG)
	dms_assert(sd_ParseExprReentrantCheck == 0);
	StaticMtIncrementalLock<sd_ParseExprReentrantCheck> reentrantLock;
#endif

	ExprProd prod;
	expr_grammar<ExprProd> p(prod);

	dbg_assert(prod.empty());

//	ProdClearer prodClearer(prod);

	while (exprEnd != exprBegin && !exprEnd[-1]) --exprEnd;

	try {
		parse_info_t info =
			boost::spirit::parse(
				iterator_t(exprBegin, exprEnd, position_t()) 
			,	iterator_t()
			,	p >> boost::spirit::end_p
			,	comment_skipper()
			);
		CheckInfo(info);
	}
	catch (const parser_error_t& problem)
	{
		SharedStr strAtProblemLoc = problemlocAsString(exprBegin, exprEnd, &*problem.where);

		position_t  problemLoc = problem.where.get_position();
		ErrMsgPtr descr = std::make_shared<ErrMsg>(problem.descriptor);
		auto fullDescr = mySSPrintF("%s\nCalculationRule(%d, %d) at\n%s"
			,	problem.descriptor
			,	problemLoc.line, problemLoc.column
			,	strAtProblemLoc.c_str()
		);
		DmsException::throwMsgD(fullDescr);
	}

	if (!prod.m_Result.size()) // Empty LispRefList is allowed (no expression)
		return LispRef();

	MG_CHECK(prod.m_Result.size() == 1);
	dms_assert(prod.m_ExprListBase.size() == 0);

	return prod.m_Result.back();
}

SYNTAX_CALL void annotateExpr(OutStreamBase& outStream, const TreeItem* searchContext, SharedStr expr)
{
	assert(IsMainThread());

	HtmlProd prod(outStream, searchContext, expr);
	expr_grammar<HtmlProd> p(prod);

	try {
		boost::spirit::parse(expr.cbegin(), expr.csend()
			, p >> boost::spirit::end_p
			, comment_skipper()
		);
	}
	catch (...)
	{
		auto err = catchException(false);
		outStream << "Error: " << err->GetAsText().c_str() << "\nwhile annotating " << expr.c_str();
	}
}

namespace {
	struct InitAnnotateFunc {
		InitAnnotateFunc()
		{
			assert(s_AnnotateExprFunc == nullptr);
			s_AnnotateExprFunc = annotateExpr;
		}

#if defined(MG_DEBUG)
		~InitAnnotateFunc()
		{
			assert(s_AnnotateExprFunc != nullptr);
			s_AnnotateExprFunc = nullptr;
		}
#endif
	};

	InitAnnotateFunc s_InitAnnotateFunc;
}
///////////////////////////////////////////////////////////////////////////////
//
//  wrap parse with a cache in order to only parse expr's of template instantiations once (if not dependent on template parameters).
//
///////////////////////////////////////////////////////////////////////////////

struct ParseExprFunctor
{
	using argument_type  = SharedStr;
	using result_type    = LispRef;

	using hasher = SharedStr::cs_hasher;
	using equality_compare = std::equal_to<SharedStr>;

	LispRef operator ()(WeakStr exprStr)
	{
		if (exprStr.empty())
			return LispRef();

		return parseExpr(exprStr.begin(), exprStr.send());
	}
};

#include "set/Cache.h"

using parse_result_cache = UnorderedMapCache<ParseExprFunctor> ;

static parse_result_cache g_Cache;

LispRef ParseExpr(WeakStr exprStr)
{
	return parseExpr(exprStr.begin(), exprStr.send());
}
