// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/MainThread.h"
#include "ptr/StaticPtr.h"
#include "CalcFactory.h"
#include "DC_Ptr.h"

#include "ExprCalculator.h"
#include "ExprRewrite.h"
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>


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
	{
		if (cr == CalcRole::Calculator)
			context->Fail("Invalid CalculationRule", FR_MetaInfo);
		return nullptr;
	}
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
