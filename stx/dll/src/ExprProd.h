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
#pragma once

#if !defined(__STX_EXPRPROD_H)
#define __STX_EXPRPROD_H

#include "set/StackUtil.h"
#include "LispRef.h"
#include "LispTreeType.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"

#include "StringProd.h"
#include "TextPosition.h"

struct ExprProd
{
	void ProdIIF();
	void ProdBinaryOper(TokenID id);

	void ProdBracketedExpr();
	void ProdUnaryOper(TokenID id);
	void ProdStringValue();
	void ProdFunctionCall();
	void ProdIdentifier(iterator_t first, iterator_t last);
	void ProdUInt64(UInt64 n);
	void ProdFloat64(Float64 x);
	void ProdUInt32WithoutSuffix();
	void ProdSuffix    (iterator_t first, iterator_t last);
	void RefocusAfterArrow() {}
	void ProdArrow() { ProdBinaryOper(token::arrow); }

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
	void ProdArrow() {}
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

	OutStreamBase& m_OutStream;
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
	}
	void ProdArrow() 
	{
		dms_assert(!m_SearchContextStack.empty());
		m_SearchContext = m_SearchContextStack.back(); m_SearchContextStack.pop_back();
	}

	using string_prod_type = StringProd;
	StringProd  m_StringProd;
};

#endif //!defined(__STX_EXPRPROD_H)
