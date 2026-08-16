// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// eq / ne over all field types, gt / ge / lt / le over the scalar types, and
// dist / sqrdist over the point types.
// Split from OperAttrBin.cpp (2026-08) for parallel compilation; the shared
// machinery lives in OperAttrBinImpl.h.

#include "OperAttrBinImpl.h"

using namespace typelists;

namespace {
	CogBinaryInstantiation<points, dist_func   > sDist("dist");
	CogBinaryInstantiation<points, sqrdist_func> sSqrDist("sqrdist");

	BinaryInstantiation<fields, equal_to    > sEq(&cog_eq);
	BinaryInstantiation<fields, not_equal_to> sNe(&cog_ne);

	CogBinaryInstantiation<scalars, greater      > sGt("gt");
	CogBinaryInstantiation<scalars, greater_equal> sGe("ge");
	CogBinaryInstantiation<scalars, less         > sLt("lt");
	CogBinaryInstantiation<scalars, less_equal   > sLe("le");
} // namespace
