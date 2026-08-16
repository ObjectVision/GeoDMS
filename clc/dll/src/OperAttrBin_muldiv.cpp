// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// mul / mul_or_null / div / mod over the ranged unit types.
// Split from OperAttrBin.cpp (2026-08) for parallel compilation; the shared
// machinery lives in OperAttrBinImpl.h.

#include "OperAttrBinImpl.h"

using namespace typelists;

namespace {
	CommonOperGroup cog_mul_or_null("mul_or_null");

	BinaryInstantiation<ranged_unit_objects, mul_func > sMul(&cog_mul);
	BinaryInstantiation<ranged_unit_objects, mul_or_null_func > sMulOrNull(&cog_mul_or_null);
	BinaryInstantiation<ranged_unit_objects, div_func > sDiv(&cog_div);

	CogBinaryInstantiation<ranged_unit_objects, mod_func  > sMod("mod");
} // namespace
