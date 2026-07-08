// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvSequence_floatseq.cpp - float sequence source conversions
// Split from OperConv.cpp / OperConv{Numeric,Sequence}.cpp for parallel compilation

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::float_sequences, convertAndCastOpers<typelists::numeric_sequences>::apply_TA > numericSequenceConvertAndCastOpers_float;

} // end anonymous namespace
