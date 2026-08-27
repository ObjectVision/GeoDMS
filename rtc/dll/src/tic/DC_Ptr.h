// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#pragma once

#if !defined(__DC_PTR_H)
#define __DC_PTR_H

#include "AbstrCalculator.h"
#include "DataController.h"

// *****************************************************************************
// Section:     DataControllerPtr Implementation
// *****************************************************************************


class DC_Ptr : public AbstrCalculator // // TODO G8: RENAME TO AssignedExprKey, or remove.
{
	typedef AbstrCalculator base_type;
public:
	DC_Ptr(const TreeItem* context, LispPtr lispRefOrg, CalcRole cr)
		: AbstrCalculator(context, lispRefOrg, cr)
	{}

	bool IsDcPtr   () const override { return true; }

protected:
	DC_Ptr();
};


#endif // !defined(__DC_PTR_H)
