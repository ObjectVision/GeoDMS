// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

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
