// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// lookup / collect_by_org_rel - numeric sequence value types

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "lookupImpl.h"

namespace {
	tl_oper::inst_tuple_templ<typelists::numeric_sequences, lookup_instances> operLookup_numseq(cog_lookup);
	tl_oper::inst_tuple_templ<typelists::numeric_sequences, lookup_instances> operCollectByOrgRel_numseq(cog_collect_by_org_rel);
}
