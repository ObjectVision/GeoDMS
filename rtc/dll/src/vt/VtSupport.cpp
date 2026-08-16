// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Small vt satellites, merged (2026-08): BaseBounds throwers and
// iterrangefuncs (CharPtrRange Search).

// ==== from BaseBounds.cpp ====

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


// ==== from iterrangefuncs.cpp ====

// Range utilities implementation: Search over CharPtrRange.

#include "vt/iterrangefuncs.h"
#include "vt/CharPtrRange.h"

#include <algorithm>

//----------------------------------------------------------------------
// IterRangeFuncs
//----------------------------------------------------------------------

CharPtr Search(CharPtrRange str, CharPtrRange pattern)
{
	return std::search(str.begin(), str.end(), pattern.begin(), pattern.end());
}

