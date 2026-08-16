// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// lookup / collect_by_org_rel - signed integer + bit value types

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "LookupImpl.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::jv2_t<typelists::sints, typelists::bints>, lookup_instances> operLookup_sint(cog_lookup);
	tl_oper::inst_tuple_templ<typelists::jv2_t<typelists::sints, typelists::bints>, lookup_instances> operCollectByOrgRel_sint(cog_collect_by_org_rel);
}
