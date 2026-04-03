// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvNumeric.cpp - Numeric type conversion operator instantiations
// Split from OperConv.cpp for parallel compilation

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {

	// Numeric conversions - the largest instantiation set
	// typelists::scalars × typelists::numerics
	tl_oper::inst_tuple_templ<typelists::scalars, convertAndCastOpers<typelists::numerics>::apply_TA > numericConvertAndCastOpers;

	// Rounded conversions for floats
	// typelists::floats × typelists::numerics
	tl_oper::inst_tuple_templ<typelists::floats, roundedConvertOpers<typelists::numerics>::apply_TA > numericRoundedConvertOpers;

} // end anonymous namespace
