// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

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

