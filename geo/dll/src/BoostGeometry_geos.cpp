// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The GEOS backend: geos_simplify_*, geos_intersect/union/xor/difference
// (incl. their infix registrations * & + | - ^ over float points),
// geos_buffer_* and the composition-dispatching geos_buffer (#1038).
// Shared machinery in BoostGeometryImpl.h (split from BoostGeometry.cpp 2026-08).

#include "BoostGeometryImpl.h"

static CommonOperGroup grGeosSimplify_linestring   ("geos_simplify_linestring", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosSimplify_multi_polygon("geos_simplify_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grgeosIntersect ("geos_intersect",  oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grgeosUnion     ("geos_union",      oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grgeosXOR       ("geos_xor",        oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grgeosDifference("geos_difference", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosBuffer_point      ("geos_buffer_point",       oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosBuffer_multi_point("geos_buffer_multi_point", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosBuffer_linestring("geos_buffer_linestring", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosBuffer_multi_polygon("geos_buffer_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosBuffer("geos_buffer", oper_policy::better_not_in_meta_scripting);

namespace
{
	template <typename P> using GeosSimplifyLinestringOperator = SimplifyLinestringOperator<P, geometry_library::geos>;
	template <typename P> using GeosSimplifyMultiPolygonOperator = SimplifyMultiPolygonOperator<P, geometry_library::geos>;
	tl_oper::inst_tuple_templ<typelists::points, GeosSimplifyLinestringOperator> geos_simplifyLineStringOperators(grGeosSimplify_linestring);
	tl_oper::inst_tuple_templ<typelists::points, GeosSimplifyMultiPolygonOperator> geos_simplifyMultiPolygonOperators(grGeosSimplify_multi_polygon);
	template <typename P> using GeosBufferPointOperator = BufferPointOperator<P, geometry_library::geos>;
	template <typename P> using GeosBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::geos>;
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferPointOperator> geosBufferPointOperators(grGeosBuffer_point);
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferMultiPointOperator> geosBeosBufferMultiPointOperators(grGeosBuffer_multi_point);
	template <typename P> using GeosBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::geos>;
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferLineStringOperator> geosBufferLineStringOperators(grGeosBuffer_linestring);
	template <typename P> using GeosBufferMultiPolygonOperator = BufferMultiPolygonOperator<P, geometry_library::geos>;
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferMultiPolygonOperator> geos_bufferMultiPolygonOperators(grGeosBuffer_multi_polygon);
	// generic geos_buffer: dispatches to multi_polygon / linestring / multi_point by ValueComposition (#1038)
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferOperator> geosBufferOperators(grGeosBuffer);

	template <typename P> using GEOS_IntersectMultiPolygonOperator = GEOS_MultiPolygonOperator < P, geos_intersection>;
	tl_oper::inst_tuple_templ<typelists::points, GEOS_IntersectMultiPolygonOperator> geosIntersectMultiPolygonOperatorsNamed(grgeosIntersect);
	tl_oper::inst_tuple_templ<typelists::float_points, GEOS_IntersectMultiPolygonOperator> bgIntersectMultiPolygonOperatorsMul(cog_mul);
	tl_oper::inst_tuple_templ<typelists::float_points, GEOS_IntersectMultiPolygonOperator> bgIntersectMultiPolygonOperatorsBitAnd(cog_bitand);


	template <typename P> using GEOS_UnionMultiPolygonOperator = GEOS_MultiPolygonOperator<P, geos_union>;
	tl_oper::inst_tuple_templ<typelists::points, GEOS_UnionMultiPolygonOperator> geosUnionMultiPolygonOperatorsNamed(grgeosUnion);
	tl_oper::inst_tuple_templ<typelists::float_points, GEOS_UnionMultiPolygonOperator> bgUnionMultiPolygonOperatorsAdd(cog_add);
	tl_oper::inst_tuple_templ<typelists::float_points, GEOS_UnionMultiPolygonOperator> bgUnionMultiPolygonOperatorsBitOr(cog_bitor);


	template <typename P> using GEOS_DifferenceMultiPolygonOperator = GEOS_MultiPolygonOperator<P, geos_difference>;
	tl_oper::inst_tuple_templ<typelists::points, GEOS_DifferenceMultiPolygonOperator> geosDifferenceMultiPolygonOperatorsNamed(grgeosDifference);
	tl_oper::inst_tuple_templ<typelists::float_points, GEOS_DifferenceMultiPolygonOperator> bgDifferenceMultiPolygonOperatorsSub(cog_sub);


	template <typename P> using GEOS_SymmetricDifferenceMultiPolygonOperator = GEOS_MultiPolygonOperator<P, geos_sym_difference>;
	tl_oper::inst_tuple_templ<typelists::points, GEOS_SymmetricDifferenceMultiPolygonOperator> geosSymmetricDifferenceMultiPolygonOperatorsNamed(grgeosXOR);
	tl_oper::inst_tuple_templ<typelists::float_points, GEOS_SymmetricDifferenceMultiPolygonOperator> bgSymmetricDifferenceMultiPolygonOperatorsBitXOR(cog_bitxor);
}
