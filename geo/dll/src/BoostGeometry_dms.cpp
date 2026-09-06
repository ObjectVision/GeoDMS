// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The dms_ backend of the shared machinery in BoostGeometryImpl.h: dms_minkowski_sum and
// dms_minkowski_difference, which union the convex cells of minkowski.h with the sweep of
// DMS_Traits.h instead of with a geometry library, and therefore accept a geometry argument that
// is not a valid polygon.
//
// The rest of the dms_ family: the four Boolean operators in PolygonOper.cpp, and dms_polygon,
// dms_union_polygon, their split_ variants, dms_overlay_polygon and dms_polygon_connectivity in
// BoostPolygon.cpp, each next to the machinery it plugs into.

#include "BoostGeometryImpl.h"

static CommonOperGroup grDmsMinkowskiSum("dms_minkowski_sum", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grDmsMinkowskiDifference("dms_minkowski_difference", oper_policy::better_not_in_meta_scripting);

namespace
{
	// Both signatures of issue #917, as the other four backends register them: (geometry, kernel)
	// with the kernel as a polygon argument, and (geometry, size, variant) with one of the named
	// shapes.
	template <typename P> using DmsMinkowskiSumKernel  = MinkowskiKernelOperator<P, geometry_library::dms, false>;
	template <typename P> using DmsMinkowskiDiffKernel = MinkowskiKernelOperator<P, geometry_library::dms, true >;
	template <typename P> using DmsMinkowskiSumNamed   = MinkowskiNamedOperator <P, geometry_library::dms, false>;
	template <typename P> using DmsMinkowskiDiffNamed  = MinkowskiNamedOperator <P, geometry_library::dms, true >;

	tl_oper::inst_tuple_templ<typelists::points, DmsMinkowskiSumKernel > dmsMinkowskiSumKernelOperators (grDmsMinkowskiSum);
	tl_oper::inst_tuple_templ<typelists::points, DmsMinkowskiDiffKernel> dmsMinkowskiDiffKernelOperators(grDmsMinkowskiDifference);
	tl_oper::inst_tuple_templ<typelists::points, DmsMinkowskiSumNamed  > dmsMinkowskiSumNamedOperators  (grDmsMinkowskiSum,        "dms_minkowski_difference");
	tl_oper::inst_tuple_templ<typelists::points, DmsMinkowskiDiffNamed > dmsMinkowskiDiffNamedOperators (grDmsMinkowskiDifference, "dms_minkowski_sum");
}
