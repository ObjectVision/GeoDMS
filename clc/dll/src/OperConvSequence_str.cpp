// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConvSequence_str.cpp - sequence-to-string + cast-from-string conversions
// Split from OperConv.cpp / OperConv{Numeric,Sequence}.cpp for parallel compilation

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::sequences, convertAndCastOpers<typelists::strings>::apply_TA > seq2stringConvertAndCastOpers;
	tl_oper::inst_tuple<typelists::sequences, tl::bind_placeholders<NamedCastAttrOper, ph::_1, SharedStr> > castSequenceOpers;

} // end anonymous namespace
