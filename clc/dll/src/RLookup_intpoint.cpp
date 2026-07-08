// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// rlookup / rlookup_with_null - integer-point value types

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "RLookupImpl.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::int_points, RLookupOperator> rlookupInstances;
	tl_oper::inst_tuple_templ<typelists::int_points, RLookupWithNullOperator> rlookupWNInstances;
}
