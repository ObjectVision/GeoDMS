// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/iterrangefuncs.h"
#include "geo/CharPtrRange.h"

#include <algorithm>

//----------------------------------------------------------------------
// IterRangeFuncs
//----------------------------------------------------------------------

CharPtr Search(CharPtrRange str, CharPtrRange pattern)
{
	return std::search(str.begin(), str.end(), pattern.begin(), pattern.end());
}

