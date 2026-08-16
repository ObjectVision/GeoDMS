// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// lookup / collect_by_org_rel - string and point value types

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "LookupImpl.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::jv2_t<typelists::strings, typelists::points>, lookup_instances> operLookup_strpoint(cog_lookup);
	tl_oper::inst_tuple_templ<typelists::jv2_t<typelists::strings, typelists::points>, lookup_instances> operCollectByOrgRel_strpoint(cog_collect_by_org_rel);
}
