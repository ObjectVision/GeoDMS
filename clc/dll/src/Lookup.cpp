// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// lookup / collect_by_org_rel - unsigned integer value types

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "LookupImpl.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::uints, lookup_instances> operLookup_uint(cog_lookup);
	tl_oper::inst_tuple_templ<typelists::uints, lookup_instances> operCollectByOrgRel_uint(cog_collect_by_org_rel);
}
