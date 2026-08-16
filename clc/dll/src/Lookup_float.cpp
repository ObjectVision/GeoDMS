// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// lookup / collect_by_org_rel - floating-point value types

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "LookupImpl.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::floats, lookup_instances> operLookup_float(cog_lookup);
	tl_oper::inst_tuple_templ<typelists::floats, lookup_instances> operCollectByOrgRel_float(cog_collect_by_org_rel);
}
