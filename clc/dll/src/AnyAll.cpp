// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "OperAccUniNum.h"

// INSTANTIATION

namespace 
{
	CommonOperGroup cogAny("any");
	CommonOperGroup cogAll("all");

	OperAccTotUniNum<any_total> anyTotal(&cogAny, true);
	OperAccTotUniNum<all_total> allTotal(&cogAll, true);

	OperAccPartUniDirect<any_partial > anyPart(&cogAny, true);
	OperAccPartUniDirect<all_partial > allPart(&cogAll, true);
}
