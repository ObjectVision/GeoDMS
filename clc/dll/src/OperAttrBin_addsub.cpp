// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// add / add_or_null / sub / sub_or_null over the ranged unit types.
// Split from OperAttrBin.cpp (2026-08) for parallel compilation; the shared
// machinery lives in OperAttrBinImpl.h.

#include "OperAttrBinImpl.h"

using namespace typelists;

namespace {
	CommonOperGroup
		cog_add_or_null("add_or_null"),
		cog_sub_or_null("sub_or_null");

	BinaryInstantiation<ranged_unit_objects, plus_func> sAdd(&cog_add);
	BinaryInstantiation<ranged_unit_objects, plus_or_null_func> sAddOrNull(&cog_add_or_null);

	BinaryInstantiation<ranged_unit_objects, minus_func> sSub(&cog_sub);
	BinaryInstantiation<ranged_unit_objects, minus_or_null_func> sSubOrNull(&cog_sub_or_null);
} // namespace
