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

// *****************************************************************************
// ExprCalculator Implementation
// *****************************************************************************

#include "ClcPCH.h"
#pragma hdrstop

#include "ExprCalculator.h"

//tic
#include "LispContextHandle.h"

// stx
#include "ParseExpr.h"

//----------------------------------------------------------------------
// ExprCalculator constructor / destructor
//----------------------------------------------------------------------

ExprCalculator::ExprCalculator(const TreeItem* context, WeakStr expr, CalcRole cr)
	:	AbstrCalculator(context, cr)
	,	m_Expression(expr)
{
	dms_assert(context);
}

LispRef ExprCalculator::GetLispExprOrg() const
{
	if (!m_HasParsed)
	{
		m_LispExprOrg = LispRef();
		LispContextHandle lch(m_Expression.c_str(), LispRef());
		m_LispExprOrg = ParseExpr(m_Expression);
		m_HasParsed = true;
	}
	return m_LispExprOrg;
}

bool ExprCalculator::CheckSyntax() const
{
	DBG_START("ExprCalculator", "CheckSyntax", false);

	return ! GetLispExprOrg().EndP();
}

#if defined(DMS_COUNT_SUPPLIERS)
void IncArrayInterestCount(const TreeItemCRefArray& supplierArray)
{
	std::for_each(
		supplierArray.begin(),
		supplierArray.end(),
		std::mem_fun(&TreeItem::IncInterestCount)
	);
}

void DecArrayInterestCount(const TreeItemCRefArray& supplierArray)
{
	std::for_each(
		supplierArray.begin(),
		supplierArray.end(),
		std::mem_fun(&TreeItem::DecInterestCount)
	);
}
#endif //defined(DMS_COUNT_SUPPLIERS)

void ExprCalculator::WriteHtmlExpr(OutStreamBase& outStream) const 
{
	annotateExpr(outStream, SearchContext(), m_Expression);
}

//----------------------------------------------------------------------
// Test
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

#include "StxInterface.h"
#include "ptr/AutoDeletePtr.h"

CLC_CALL bool ExprCalculatorTest()
{
	DBG_START("ExprCalculator", "TEST", true);

	AutoDeletePtr<TreeItem> testConfig = DMS_CreateTreeFromString(
		"container Test { "
		"	parameter<UInt32> Item1: Expr = \"3+5\"; "
		"	parameter<UInt32> Item2: Expr = \"Item1\"; "
		"}"
	);
	const TreeItem* item1 = testConfig->FindItem("Item1");
	const TreeItem* item2 = testConfig->FindItem("Item2");
	bool result = true;
	result &= DBG_TEST("SourceItem", item2->GetSourceItem() == item1);
	result &= DBG_TEST("RefObj",     AsDataItem(item2)->LockAndGetValue<UInt32>(0) == 8);
	return result;
}

#endif //defined(MG_DEBUG)
