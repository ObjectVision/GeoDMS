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

namespace
{
	template <typename P> using BpBufferPointOperator = BufferPointOperator<P, geometry_library::boost_polygon>;
	template <typename P> using BpBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::boost_polygon>;
	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferPointOperator> bpBufferPointOperators(grBpBuffer_point);
	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferMultiPointOperator> bpBufferMultiPointOperators(grBpBuffer_multi_point);
	template <typename P> using BpBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::boost_polygon>;
	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferLineStringOperator> bpBufferLineStringOperators(grBpBuffer_linestring);
}
