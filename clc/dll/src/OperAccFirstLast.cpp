// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "OperAccUniNum.h"

CommonOperGroup cog_First("first");
CommonOperGroup cog_Last("last");

namespace
{
	OperAccUniNum::AggrOperators<first_total_best, first_partial_best, typelists::fixed_size_elements> s_FirstOpers(&cog_First);
	OperAccUniNum::AggrOperators<last_total_best, last_partial_best  , typelists::fixed_size_elements> s_LastOpers (&cog_Last );
}
