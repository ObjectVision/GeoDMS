// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

// The winding-order family of issue #302: reverse_polygon, fix_winding_order, fix_polygon and
// has_correct_winding. Shared machinery in BoostGeometryImpl.h and GEOS_Traits.h.
//
// Its own translation unit for the same reason the backends were split out of BoostGeometry.cpp in
// 2026-08: these operators instantiate over every point type, and adding them to an existing file
// pushed BoostGeometry_geos.obj past what the linker accepts (LNK1163, invalid selection for COMDAT
// section). A Debug object in this project is already tens of megabytes; keep new families here.
//
// Plain operator names rather than geos_ prefixed ones: unlike union or intersect there is only one
// right answer for ring order, so there is no per-backend variant to tell apart.

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "BoostGeometryImpl.h"

static CommonOperGroup grReverse_polygon("reverse_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grFix_winding_order("fix_winding_order", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grFix_polygon("fix_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grHas_correct_winding("has_correct_winding", oper_policy::better_not_in_meta_scripting);

namespace
{
	// reverse_polygon has no geometry-library dependency at all: it only reorders points within
	// each ring. fix_winding_order and has_correct_winding use GEOS for the ANALYSIS only and
	// re-emit the original points, so all three are exact for every coordinate type.
	tl_oper::inst_tuple_templ<typelists::points, ReversePolygonOperator> reversePolygonOperators(grReverse_polygon);
	tl_oper::inst_tuple_templ<typelists::points, FixWindingOrderOperator> fixWindingOrderOperators(grFix_winding_order);
	tl_oper::inst_tuple_templ<typelists::points, HasCorrectWindingOperator> hasCorrectWindingOperators(grHas_correct_winding);

	// fix_polygon runs MakeValid, which introduces intersection points an integer coordinate grid
	// cannot hold, so it stays on dpoint like the rest of the geos family.
	tl_oper::inst_tuple_templ<tl::type_list<DPoint>, FixPolygonOperator> fixPolygonOperators(grFix_polygon);
}
