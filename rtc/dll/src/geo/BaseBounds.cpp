// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/BaseBounds.h"

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
		"This error may result from vizualising a geographic layer without bounding box or with a bounding box with zero area."
	);
}
