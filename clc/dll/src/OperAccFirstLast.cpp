// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperAccUniNum.h"

CommonOperGroup cog_First("first");
CommonOperGroup cog_Last("last");

namespace
{
	OperAccUniNum::AggrOperators<first_total_best, first_partial_best, typelists::value_elements> s_FirstOpers(&cog_First);
	OperAccUniNum::AggrOperators<last_total_best,  last_partial_best , typelists::value_elements> s_LastOpers (&cog_Last );
}
