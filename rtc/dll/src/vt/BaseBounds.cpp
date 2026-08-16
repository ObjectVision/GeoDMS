// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Element bounds implementation: the out-of-line minmax_traits values.

#include "vt/BaseBounds.h"

//----------------------------------------------------------------------
// Section      : Void
//----------------------------------------------------------------------

Float64 AsFloat64(const Void& ) { throwIllegalAbstract(MG_POS, "AsFloat64(Void)"); }

//----------------------------------------------------------------------
// Section      : Transformations
//----------------------------------------------------------------------


[[noreturn]] RTC_CALL void IllegalSingularity()
{
	throwErrorD("Transformation",
		"illegal singular factor\n"
		"This error may result from visualising a geographic layer without features, for which no bounding box can be determined."
	);
}
