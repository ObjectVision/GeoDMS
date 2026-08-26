// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The boost.polygon backend: bp_buffer_point / bp_buffer_multi_point /
// bp_buffer_linestring over the integer point types. Shared machinery in
// BoostGeometryImpl.h (split from BoostGeometry.cpp 2026-08).

#include "BoostGeometryImpl.h"

static CommonOperGroup grBpBuffer_point        ("bp_buffer_point",         oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBpBuffer_multi_point  ("bp_buffer_multi_point",   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBpBuffer_linestring("bp_buffer_linestring", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBpMinkowskiSum("bp_minkowski_sum", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBpMinkowskiDifference("bp_minkowski_difference", oper_policy::better_not_in_meta_scripting);

namespace
{
	template <typename P> using BpBufferPointOperator = BufferPointOperator<P, geometry_library::boost_polygon>;
	template <typename P> using BpBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::boost_polygon>;
	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferPointOperator> bpBufferPointOperators(grBpBuffer_point);
	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferMultiPointOperator> bpBufferMultiPointOperators(grBpBuffer_multi_point);
	template <typename P> using BpBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::boost_polygon>;
	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferLineStringOperator> bpBufferLineStringOperators(grBpBuffer_linestring);

	// issue #917: the replacement for the twelve _i4HV / _d4HV / ... name suffixes, which are now
	// depreciated. boost.polygon has its own general convolution and uses it. Registered over the
	// signed integer points, matching the other bp_ polygon set operators.
	template <typename P> using BpMinkowskiSumKernel  = MinkowskiKernelOperator<P, geometry_library::boost_polygon, false>;
	template <typename P> using BpMinkowskiDiffKernel = MinkowskiKernelOperator<P, geometry_library::boost_polygon, true >;
	template <typename P> using BpMinkowskiSumNamed   = MinkowskiNamedOperator <P, geometry_library::boost_polygon, false>;
	template <typename P> using BpMinkowskiDiffNamed  = MinkowskiNamedOperator <P, geometry_library::boost_polygon, true >;

	tl_oper::inst_tuple_templ<typelists::sint_points, BpMinkowskiSumKernel > bpMinkowskiSumKernelOperators (grBpMinkowskiSum);
	tl_oper::inst_tuple_templ<typelists::sint_points, BpMinkowskiDiffKernel> bpMinkowskiDiffKernelOperators(grBpMinkowskiDifference);
	tl_oper::inst_tuple_templ<typelists::sint_points, BpMinkowskiSumNamed  > bpMinkowskiSumNamedOperators  (grBpMinkowskiSum,        "bp_minkowski_difference");
	tl_oper::inst_tuple_templ<typelists::sint_points, BpMinkowskiDiffNamed > bpMinkowskiDiffNamedOperators (grBpMinkowskiDifference, "bp_minkowski_sum");
}
