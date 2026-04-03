// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvSequence.cpp - Sequence type conversion operator instantiations
// Split from OperConv.cpp for parallel compilation

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {

	// Numeric sequence conversions
	tl_oper::inst_tuple_templ<typelists::numeric_sequences, convertAndCastOpers<typelists::numeric_sequences>::apply_TA > numericSequenceConvertAndCastOpers;

	// Point sequence conversions
	tl_oper::inst_tuple_templ<typelists::point_sequences, convertAndCastOpers<typelists::point_sequences>::apply_TA > pointSequenceConvertAndCastOpers;

	// Sequences to string conversions
	tl_oper::inst_tuple_templ<typelists::sequences, convertAndCastOpers<typelists::strings>::apply_TA > seq2stringConvertAndCastOpers;

	// Cast sequences from string
	tl_oper::inst_tuple<typelists::sequences, tl::bind_placeholders<NamedCastAttrOper, ph::_1, SharedStr> > castSequenceOpers;

} // end anonymous namespace
