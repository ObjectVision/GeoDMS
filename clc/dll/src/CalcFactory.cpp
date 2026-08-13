// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "act/MainThread.h"
#include "ptr/StaticPtr.h"
#include "CalcFactory.h"
#include "DC_Ptr.h"

#include "ExprCalculator.h"
#include "ExprRewrite.h"


//----------------------------------------------------------------------
// instantiation and registration of ExprCalculator Class Factory
//----------------------------------------------------------------------


// register ExprCalculator ctor upon loading the .DLL.
CalcFactory::CalcFactory()
{
	AbstrCalculator::SetConstructor(this); 
}

CalcFactory::~CalcFactory()
{
	AbstrCalculator::SetConstructor(nullptr);
}

AbstrCalculatorRef CalcFactory::ConstructExpr(const TreeItem* context, WeakStr expr, CalcRole cr)
{
	dms_assert(IsMetaThread());
	if (expr.empty())
		// No calculation rule. A leading-'=' rule that evaluates to "" says exactly that, and says it
		// deliberately -- it is how a rule can make itself conditional -- so it is accepted in silence.
		// A rule spelled out as empty in the configuration is warned about by ExprPropDef, where the
		// configured text is still in hand; failing here would report it a second time, and later.
		return {};
	AbstrCalculatorRef exprCalc = new ExprCalculator(context, expr, cr); // hold resource for now; beware: this line can trigger new inserts/deletes in s_CalcFactory
	return exprCalc; // second alloc succeeded, release hold an from now on it's callers' responsibility to destroy the new ExprCalculator
}

#include "DataBlockTask.h"

AbstrCalculatorRef CalcFactory::ConstructDBT(AbstrDataItem* context, const AbstrCalculator* src)
{
	dms_assert(src);
	dms_assert(src->IsDataBlock());
	return new DataBlockTask(
		context, 
		*debug_cast<const DataBlockTask*>(src)
	);
}

LispRef CalcFactory::RewriteExprTop(LispPtr org)
{
	return ::RewriteExprTop(org);
}

//----------------------------------------------------------------------
// the Singleton
//----------------------------------------------------------------------

CalcFactory calcFactory;
