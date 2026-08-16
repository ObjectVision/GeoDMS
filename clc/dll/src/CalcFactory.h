// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_CALCFACTORY_H)
#define __CLC_CALCFACTORY_H

#include "AbstrCalculator.h"

//----------------------------------------------------------------------
// instantiation and registration of ExprCalculator Class Factory
//----------------------------------------------------------------------

struct CalcFactory : AcConstructor
{
	// register ExprCalculator ctor upon loading the .DLL.
	CalcFactory();
	~CalcFactory();

	AbstrCalculatorRef ConstructExpr(const TreeItem* context, WeakStr expr, CalcRole cr) override;
	AbstrCalculatorRef ConstructDBT(AbstrDataItem* context, const AbstrCalculator* src) override;
	LispRef RewriteExprTop(LispPtr org) override;
};

extern CalcFactory calcFactory;

#endif //!defined(__CLC_CALCFACTORY_H)
