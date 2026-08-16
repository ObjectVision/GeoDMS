// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__EXPRCALCULATOR_H)
#define __EXPRCALCULATOR_H

#include "AbstrCalculator.h"

// *****************************************************************************
// Section:     ExprCalculator Interface
// *****************************************************************************

class ExprCalculator : public AbstrCalculator // TODO G8: RENAME TO ParsedExprKey
{
	typedef AbstrCalculator base_type;
public:
	ExprCalculator(const TreeItem* context, WeakStr expr, CalcRole cr);

//	override AbstrCalculator
	LispRef GetLispExprOrg() const override; 

	bool      CheckSyntax() const override;
	SharedStr GetExpr    () const override { return m_Expression; }
	void WriteHtmlExpr(OutStreamBase& outStream) const override;

private:
	SharedStr                 m_Expression;
};

// *****************************************************************************

#endif // __EXPRCALCULATOR_H
