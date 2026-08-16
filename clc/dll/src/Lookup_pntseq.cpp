// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// lookup / collect_by_org_rel - point sequence value types

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "LookupImpl.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::point_sequences, lookup_instances> operLookup_pntseq(cog_lookup);
	tl_oper::inst_tuple_templ<typelists::point_sequences, lookup_instances> operCollectByOrgRel_pntseq(cog_collect_by_org_rel);
}
