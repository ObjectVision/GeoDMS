// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// rlookup - numeric sequence value types

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "RLookupImpl.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::numeric_sequences, RLookupOperator> rlookupInstances;
}
