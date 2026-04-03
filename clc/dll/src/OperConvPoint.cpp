// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvPoint.cpp - Point type conversion operator instantiations
// Split from OperConv.cpp for parallel compilation

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {

	// Point conversions
	// typelists::points × typelists::points
	tl_oper::inst_tuple_templ<typelists::points, convertAndCastOpers<typelists::points>::apply_TA > pointConvertAndCastOpers;

	// Points to string conversions
	tl_oper::inst_tuple_templ<typelists::points, convertAndCastOpers<typelists::strings>::apply_TA > points2stringConvertAndCastOpers;

} // end anonymous namespace
