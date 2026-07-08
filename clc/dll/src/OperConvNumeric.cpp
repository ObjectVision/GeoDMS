// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvNumeric.cpp - unsigned integer source conversions
// Split from OperConv.cpp / OperConv{Numeric,Sequence}.cpp for parallel compilation

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::uints, convertAndCastOpers<typelists::numerics>::apply_TA > numericConvertAndCastOpers_uint;

} // end anonymous namespace
