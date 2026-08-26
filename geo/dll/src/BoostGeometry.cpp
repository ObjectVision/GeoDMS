// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The boost.geometry backend: bg_simplify_*, bg_intersect/union/xor/difference,
// bg_buffer_* and the (bg_)outer_* operators. Shared machinery in BoostGeometryImpl.h;
// the other backends live in BoostGeometry_{bp,cgal,geos}.cpp (split 2026-08).

#include "BoostGeometryImpl.h"

static CommonOperGroup grBgSimplify_multi_polygon("bg_simplify_multi_polygon", oper_policy::better_not_in_meta_scripting);
CommonOperGroup grBgSimplify_polygon      ("bg_simplify_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgSimplify_linestring   ("bg_simplify_linestring", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgIntersect   ("bg_intersect" ,   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgUnion       ("bg_union"     ,   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgXOR         ("bg_xor"       ,   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgDifference  ("bg_difference",   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_point        ("bg_buffer_point",         oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_multi_point  ("bg_buffer_multi_point",   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_single_polygon("bg_buffer_single_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_linestring("bg_buffer_linestring", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_multi_polygon("bg_buffer_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grOuter_polygon("outer_single_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grOuter_multi_polygon("outer_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgOuter_single_polygon("bg_outer_single_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgOuter_multi_polygon("bg_outer_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgMinkowskiSum("bg_minkowski_sum", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgMinkowskiDifference("bg_minkowski_difference", oper_policy::better_not_in_meta_scripting);

namespace
{
	template <typename P> using BgSimplifyLinestringOperator = SimplifyLinestringOperator<P, geometry_library::boost_geometry>;
	template <typename P> using BgSimplifyMultiPolygonOperator = SimplifyMultiPolygonOperator<P, geometry_library::boost_geometry>;
	tl_oper::inst_tuple_templ<typelists::points, BgSimplifyLinestringOperator> bg_simplifyLineStringOperators(grBgSimplify_linestring);
	tl_oper::inst_tuple_templ<typelists::points, BgSimplifyMultiPolygonOperator> bg_simplifyMultiPolygonOperators(grBgSimplify_multi_polygon);
	tl_oper::inst_tuple_templ<typelists::points, SimplifyPolygonOperator> simplifyPolygonOperators;
	template <typename P> using BgBufferPointOperator = BufferPointOperator<P, geometry_library::boost_geometry>;
	template <typename P> using BgBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::boost_geometry>;
	tl_oper::inst_tuple_templ<typelists::points, BgBufferPointOperator> bgBufferPointOperators(grBgBuffer_point);
	tl_oper::inst_tuple_templ<typelists::points, BgBufferMultiPointOperator> bgBufferMultiPointOperators(grBgBuffer_multi_point);
	template <typename P> using BgBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::boost_geometry>;
	tl_oper::inst_tuple_templ<typelists::points, BgBufferLineStringOperator> bgBufferLineStringOperators(grBgBuffer_linestring);

	template <typename P> using BgIntersectMultiPolygonOperator = BgMultiPolygonOperator < P, bg_intersection> ;
	tl_oper::inst_tuple_templ<typelists::points, BgIntersectMultiPolygonOperator> bgIntersectMultiPolygonOperatorsNamed(grBgIntersect);

	template <typename P> using BgUnionMultiPolygonOperator = BgMultiPolygonOperator<P, bg_union>;
	tl_oper::inst_tuple_templ<typelists::points, BgUnionMultiPolygonOperator> bgUnionMultiPolygonOperatorsNamed(grBgUnion);

	template <typename P> using BgDifferenceMultiPolygonOperator = BgMultiPolygonOperator<P, bg_difference>;
	tl_oper::inst_tuple_templ<typelists::points, BgDifferenceMultiPolygonOperator> bgDifferenceMultiPolygonOperatorsNamed(grBgDifference);

	template <typename P> using BgSymmetricDifferenceMultiPolygonOperator = BgMultiPolygonOperator<P, bg_sym_difference>;
	tl_oper::inst_tuple_templ<typelists::points, BgSymmetricDifferenceMultiPolygonOperator> bgSymmetricDifferenceMultiPolygonOperatorsNamed(grBgXOR);
	template <typename P> using BgBufferMultiPolygonOperator = BufferMultiPolygonOperator<P, geometry_library::boost_geometry>;
	tl_oper::inst_tuple_templ<typelists::points, BufferSinglePolygonOperator> bg_buffersinglePolygonOperators(grBgBuffer_single_polygon);
	tl_oper::inst_tuple_templ<typelists::points, BgBufferMultiPolygonOperator> bg_bufferMultiPolygonOperators(grBgBuffer_multi_polygon);

	tl_oper::inst_tuple_templ<typelists::points, OuterSinglePolygonOperator> outerSinglePolygonOperators(grOuter_polygon);
	tl_oper::inst_tuple_templ<typelists::points, OuterMultiPolygonOperator> outerMultiPolygonOperators(grOuter_multi_polygon);

	tl_oper::inst_tuple_templ<typelists::points, OuterSinglePolygonOperator> bg_outerSinglePolygonOperators(grBgOuter_single_polygon);
	tl_oper::inst_tuple_templ<typelists::points, OuterMultiPolygonOperator> bg_outerMultiPolygonOperators(grBgOuter_multi_polygon);

	// issue #917: both signatures land on one group -- (geometry, kernel) and (geometry, size, variant)
	template <typename P> using BgMinkowskiSumKernel  = MinkowskiKernelOperator<P, geometry_library::boost_geometry, false>;
	template <typename P> using BgMinkowskiDiffKernel = MinkowskiKernelOperator<P, geometry_library::boost_geometry, true >;
	template <typename P> using BgMinkowskiSumNamed   = MinkowskiNamedOperator <P, geometry_library::boost_geometry, false>;
	template <typename P> using BgMinkowskiDiffNamed  = MinkowskiNamedOperator <P, geometry_library::boost_geometry, true >;

	tl_oper::inst_tuple_templ<typelists::points, BgMinkowskiSumKernel > bgMinkowskiSumKernelOperators (grBgMinkowskiSum);
	tl_oper::inst_tuple_templ<typelists::points, BgMinkowskiDiffKernel> bgMinkowskiDiffKernelOperators(grBgMinkowskiDifference);
	tl_oper::inst_tuple_templ<typelists::points, BgMinkowskiSumNamed  > bgMinkowskiSumNamedOperators  (grBgMinkowskiSum,        "bg_minkowski_difference");
	tl_oper::inst_tuple_templ<typelists::points, BgMinkowskiDiffNamed > bgMinkowskiDiffNamedOperators (grBgMinkowskiDifference, "bg_minkowski_sum");
}
