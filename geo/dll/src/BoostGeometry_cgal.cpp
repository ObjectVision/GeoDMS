// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The CGAL backend: cgal_intersect/union/xor/difference and cgal_buffer_*.
// Shared machinery in BoostGeometryImpl.h (split from BoostGeometry.cpp 2026-08).

#include "BoostGeometryImpl.h"

static CommonOperGroup grcgalIntersect ("cgal_intersect",  oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalUnion     ("cgal_union",      oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalXOR       ("cgal_xor",        oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalDifference("cgal_difference", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grCgalBuffer_point      ("cgal_buffer_point",       oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grCgalBuffer_multi_point("cgal_buffer_multi_point", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grCgalBuffer_linestring("cgal_buffer_linestring", oper_policy::better_not_in_meta_scripting);

namespace
{
	template <typename P> using CgalBufferPointOperator = BufferPointOperator<P, geometry_library::cgal>;
	template <typename P> using CgalBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::cgal>;
	tl_oper::inst_tuple_templ<typelists::points, CgalBufferPointOperator> cgalBufferPointOperators(grCgalBuffer_point);
	tl_oper::inst_tuple_templ<typelists::points, CgalBufferMultiPointOperator> cgalBufferMultiPointOperators(grCgalBuffer_multi_point);
	template <typename P> using CgalBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::cgal>;
	tl_oper::inst_tuple_templ<typelists::points, CgalBufferLineStringOperator> cgalBufferLineStringOperators(grCgalBuffer_linestring);

	template <typename P> using CGAL_IntersectMultiPolygonOperator = CGAL_MultiPolygonOperator < P, cgal_intersection>;
	tl_oper::inst_tuple_templ<typelists::points, CGAL_IntersectMultiPolygonOperator> cgalIntersectMultiPolygonOperatorsNamed(grcgalIntersect);

	template <typename P> using CGAL_UnionMultiPolygonOperator = CGAL_MultiPolygonOperator<P, cgal_union>;
	tl_oper::inst_tuple_templ<typelists::points, CGAL_UnionMultiPolygonOperator> cgalUnionMultiPolygonOperatorsNamed(grcgalUnion);

	template <typename P> using CGAL_DifferenceMultiPolygonOperator = CGAL_MultiPolygonOperator<P, cgal_difference>;
	tl_oper::inst_tuple_templ<typelists::points, CGAL_DifferenceMultiPolygonOperator> cgalDifferenceMultiPolygonOperatorsNamed(grcgalDifference);

	template <typename P> using CGAL_SymmetricDifferenceMultiPolygonOperator = CGAL_MultiPolygonOperator<P, cgal_sym_difference>;
	tl_oper::inst_tuple_templ<typelists::points, CGAL_SymmetricDifferenceMultiPolygonOperator> cgalSymmetricDifferenceMultiPolygonOperatorsNamed(grcgalXOR);
}
