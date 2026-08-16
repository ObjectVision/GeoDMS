// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// bitand / bitor / bitxor and the logical and / or over the integer types.
// Split from OperAttrBin.cpp (2026-08) for parallel compilation; the shared
// machinery lives in OperAttrBinImpl.h.

#include "OperAttrBinImpl.h"

using namespace typelists;

namespace {
	BinaryInstantiation<aints, binary_and>  sBitAnd(&cog_bitand);
	BinaryInstantiation<aints, binary_or >  sBitOr(&cog_bitor);
	BinaryInstantiation<aints, binary_xor >  sBitXOr(&cog_bitxor);

	CogBinaryInstantiation<aints, logical_and> sAnd("and");
	CogBinaryInstantiation<aints, logical_or > sOr("or");
} // namespace
