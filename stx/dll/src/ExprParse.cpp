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

#include "SpiritTools.h"

#include "DataBlockParse.h"
#include "ExprParse.h"
#include "ExprProd.h"
#include "ParseExpr.h"

#include "act/MainThread.h"
#include "xml/XmlTreeOut.h"

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
		ErrMsgPtr descr = problem.descriptor;
		descr->TellExtraF("CalculationRule(%d, %d) at\n%s", 
			problemLoc.line, problemLoc.column, 
			strAtProblemLoc.c_str()
		);
		throw DmsException(descr);
	}

	if (!prod.m_Result.size()) // Empty LispRefList is allowed (no expression)
		return LispRef();

	MG_CHECK(prod.m_Result.size() == 1);
	dms_assert(prod.m_ExprListBase.size() == 0);

	return prod.m_Result.back();
}

SYNTAX_CALL void annotateExpr(OutStreamBase& outStream, const TreeItem* searchContext, SharedStr expr)
{
	dms_assert(IsMainThread());

	HtmlProd prod(outStream, searchContext, expr);
	expr_grammar<HtmlProd> p(prod);

	try {
		boost::spirit::parse(expr.cbegin(), expr.csend()
			, p >> boost::spirit::end_p
			, comment_skipper()
		);
	}
	catch (...){}
}

namespace {
	struct InitAnnotateFunc {
		InitAnnotateFunc()
		{
			dms_assert(s_AnnotateExprFunc == nullptr);
			s_AnnotateExprFunc = annotateExpr;
		}

#if defined(MG_DEBUG)
		~InitAnnotateFunc()
		{
			dms_assert(s_AnnotateExprFunc != nullptr);
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
	typedef SharedStr argument_type;
	typedef LispRef   result_type;

	LispRef operator ()(WeakStr exprStr)
	{
		if (exprStr.empty())
			return LispRef();

		return parseExpr(exprStr.begin(), exprStr.send());
	}
};

#include "set/Cache.h"

using parse_result_cache = Cache<ParseExprFunctor> ;

static parse_result_cache g_Cache;

LispRef ParseExpr(WeakStr exprStr)
{
	return parseExpr(exprStr.begin(), exprStr.send());
}
