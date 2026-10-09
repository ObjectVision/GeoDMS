// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The dms_ backend of the shared operator machinery in BoostGeometryImpl.h: dms_minkowski_sum,
// dms_minkowski_difference and dms_buffer_multi_polygon, which assemble their result per element
// from the pieces of DmsMinkowski.h in one sweep of DMS_Traits.h instead of with a geometry
// library, and therefore accept a geometry argument that is not a valid polygon.
//
// The rest of the dms_ family: the four Boolean operators in PolygonOper.cpp, and dms_polygon,
// dms_union_polygon, their split_ variants, dms_overlay_polygon and dms_polygon_connectivity in
// BoostPolygon.cpp, each next to the machinery it plugs into.

#include "BoostGeometryImpl.h"

static CommonOperGroup grDmsMinkowskiSum("dms_minkowski_sum", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grDmsMinkowskiDifference("dms_minkowski_difference", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grDmsBuffer_multi_polygon("dms_buffer_multi_polygon", oper_policy::better_not_in_meta_scripting);

namespace
{
	// Both signatures of issue #917, as the other four backends register them: (geometry, kernel)
	// with the kernel as a polygon argument, and (geometry, size, variant) with one of the named
	// shapes. Convex kernels only (#1301).
	template <typename P> using DmsMinkowskiSumKernel  = MinkowskiKernelOperator<P, geometry_library::dms, false>;
	template <typename P> using DmsMinkowskiDiffKernel = MinkowskiKernelOperator<P, geometry_library::dms, true >;
	template <typename P> using DmsMinkowskiSumNamed   = MinkowskiNamedOperator <P, geometry_library::dms, false>;
	template <typename P> using DmsMinkowskiDiffNamed  = MinkowskiNamedOperator <P, geometry_library::dms, true >;

	tl_oper::inst_tuple_templ<typelists::points, DmsMinkowskiSumKernel > dmsMinkowskiSumKernelOperators (grDmsMinkowskiSum);
	tl_oper::inst_tuple_templ<typelists::points, DmsMinkowskiDiffKernel> dmsMinkowskiDiffKernelOperators(grDmsMinkowskiDifference);
	tl_oper::inst_tuple_templ<typelists::points, DmsMinkowskiSumNamed  > dmsMinkowskiSumNamedOperators  (grDmsMinkowskiSum,        "dms_minkowski_difference");
	tl_oper::inst_tuple_templ<typelists::points, DmsMinkowskiDiffNamed > dmsMinkowskiDiffNamedOperators (grDmsMinkowskiDifference, "dms_minkowski_sum");

	// dms_buffer_multi_polygon(geometry, distance, quadrantSegments), the dms_ counterpart of
	// geos_buffer_multi_polygon (#1302), with its arguments: a distance and a uint8 that GEOS takes
	// as the number of segments per quarter circle, each a parameter or one per element. A negative
	// distance erodes. The points are placed as GEOS places them (see DmsMinkowski.h), so that a
	// configuration can switch between the two families with the same arguments.
	template <typename P>
	struct DmsBufferMultiPolygonOperator : AbstrBufferOperator
	{
		using PolygonType = typename sequence_traits<P>::container_type;
		using Arg1Type = DataArray<PolygonType>;

		DmsBufferMultiPolygonOperator(AbstrOperGroup& gr)
			: AbstrBufferOperator(gr, ValueComposition::Polygon, Arg1Type::GetStaticClass())
		{}

		void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem
			, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
			, bool e3IsVoid, const AbstrDataItem* qsItem, UInt8 quadrantSegments
			, tile_id t, Timer& processTimer, CharPtr itemRef) const override
		{
			auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
			auto distData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
			auto qsData   = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8  >(qsItem)->GetTile(t);
			auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
			assert(polyData.size() == resData.size());

			SizeT n = polyData.size();
			CharPtr operName = GetGroup()->GetNameStr();
			dms_overlay::dms_minkowski_tile<P>(resData, n, operName
				, [&](dms_overlay::DmsMinkowskiWorker<P>& w, SizeT i, dms_overlay::dms_polygon_t<P>& result)
				{
					dms_overlay::dms_buffer<P>(w, result, polyData[i]
						, e2IsVoid ? bufferDistance : distData[i]
						, e3IsVoid ? quadrantSegments : qsData[i]
					);
				}
				, [&](SizeT nrDone)
				{
					if (processTimer.PassedSecs())
						reportF(SeverityTypeID::ST_MajorTrace, "{}{}: processed {} / {} sequences of tile {} / {}"
							, itemRef
							, operName
							, AsString(nrDone), AsString(n)
							, AsString(t), AsString(resObj->GetTiledRangeData()->GetNrTiles())
						);
				}
			);
		}
	};

	tl_oper::inst_tuple_templ<typelists::points, DmsBufferMultiPolygonOperator> dmsBufferMultiPolygonOperators(grDmsBuffer_multi_polygon);
}
