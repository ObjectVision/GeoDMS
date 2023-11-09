// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "OperAccUniNum.h"


namespace 
{
	CommonOperGroup cogCount("count");
	OperAccUniNum::AggrOperators<count_total_best_UInt32, count_partial_best_UInt32, typelists::ranged_unit_objects> s_CountOpers(&cogCount);
}
