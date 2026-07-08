// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvNumeric_sint.cpp - signed integer + bit source conversions
// Split from OperConv.cpp / OperConv{Numeric,Sequence}.cpp for parallel compilation

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::jv2_t<typelists::sints, typelists::bints>, convertAndCastOpers<typelists::numerics>::apply_TA > numericConvertAndCastOpers_sint;

} // end anonymous namespace
