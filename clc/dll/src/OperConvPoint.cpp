// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvPoint.cpp - Point type conversion operator instantiations
// Split from OperConv.cpp for parallel compilation

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {

	// Point conversions
	// typelists::points × typelists::points
	tl_oper::inst_tuple_templ<typelists::points, convertAndCastOpers<typelists::points>::apply_TA > pointConvertAndCastOpers;

	// Points to string conversions: convert() and lookup() only. CLC-A11: the cast registered a second
	// string(point) beside asstring_assign (OperAttrUni_str.cpp), in the same group, and static-init order
	// decided which one a configuration got; the cast lacks AF1_HASUNDEFINED, so with it a null point
	// became the text xy(null; null). asstring_assign, which gives null, is the one that stays.
	tl_oper::inst_tuple_templ<typelists::points, convertOpersWithoutCast<typelists::strings>::apply_TA > points2stringConvertOpers;

} // end anonymous namespace
