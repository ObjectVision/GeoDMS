// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvNumeric_float.cpp - floating-point source + rounded conversions
// Split from OperConv.cpp / OperConv{Numeric,Sequence}.cpp for parallel compilation

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::floats, convertAndCastOpers<typelists::numerics>::apply_TA > numericConvertAndCastOpers_float;
	tl_oper::inst_tuple_templ<typelists::floats, roundedConvertOpers<typelists::numerics>::apply_TA > numericRoundedConvertOpers;

} // end anonymous namespace
