// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Shared machinery of the multi-backend geometry operators, split out of
 *  BoostGeometry.cpp (2026-08) so each backend's instantiations compile in
 *  their own TU (BoostGeometry.cpp = boost.geometry incl. the outer_*
 *  operators; BoostGeometry_bp.cpp = boost.polygon buffers;
 *  BoostGeometry_cgal.cpp; BoostGeometry_geos.cpp incl. the infix +*-^
 *  registrations over float points): the assign/store helpers and the
 *  Simplify-, Buffer-, Outer- and set-operation operator templates parameterized
 *  on geometry_library.
 */

#if !defined(__GEO_BOOSTGEOMETRYIMPL_H)
#define __GEO_BOOSTGEOMETRYIMPL_H


#include <numbers>

#include "BoostGeometry.h"

#include "DataArrayValue.h" // GetTheCurrValue, for the Minkowski operators' variant parameter
#include "Parallel.h" // MaxConcurrentTreads, for the buffer operators' working-memory estimate
#include "geom/BoostPolygon.h"
#include "vt/AssocTower.h" // divide & conquer union of the Minkowski cells

#include "CGAL_Traits.h"
#include "GEOS_Traits.h"
#include "minkowski.h"

#include <geos/simplify/DouglasPeuckerSimplifier.h>

#include <CGAL/minkowski_sum_2.h> // cgal_minkowski_sum uses CGAL's own exact convolution

template <geometry_library>
inline constexpr bool unsupported_geometry_library_v = false;

// *****************************************************************************
//	more conversion functions
// *****************************************************************************

template <typename P>
void bp_load_multi_linestring(std::vector<std::vector<bp::point_data<scalar_of_t<P>>>>& mls, SA_ConstReference<P> multiLineStringRef, std::vector<bp::point_data<scalar_of_t<P>>>& helperLineString)
{
	mls.clear();

	auto lineStringBegin = begin_ptr(multiLineStringRef)
		, sequenceEnd = end_ptr(multiLineStringRef);

	while (lineStringBegin != sequenceEnd)
	{
		while (!IsDefined(*lineStringBegin))
			if (++lineStringBegin == sequenceEnd)
				return;

		auto lineStringEnd = lineStringBegin + 1;
		while (lineStringEnd != sequenceEnd && IsDefined(*lineStringEnd))
			++lineStringEnd;

		helperLineString.assign(lineStringBegin, lineStringEnd);
		if (!helperLineString.empty())
			mls.emplace_back(std::move(helperLineString));

		lineStringBegin = lineStringEnd;
	}
}

template <typename P>
void bg_load_multi_linestring(bg_multi_linestring_t& mls, SA_ConstReference<P> multiLineStringRef, bg_linestring_t& helperLineString)
{
	mls.clear();

	auto lineStringBegin = begin_ptr(multiLineStringRef)
		, sequenceEnd = end_ptr(multiLineStringRef);

	while (lineStringBegin != sequenceEnd)
	{
		while (!IsDefined(*lineStringBegin))
			if (++lineStringBegin == sequenceEnd)
				return;

		auto lineStringEnd = lineStringBegin + 1;
		while (lineStringEnd != sequenceEnd && IsDefined(*lineStringEnd))
			++lineStringEnd;

		helperLineString.assign(lineStringBegin, lineStringEnd);
		if (!helperLineString.empty())
			mls.emplace_back(std::move(helperLineString));

		lineStringBegin = lineStringEnd;
	}
}

// The buffer of a polyline is the Minkowski sum of its segments with the buffer disc. With the disc
// approximated by a pointsPerCircle-gon, the sum over one segment is exactly the convex hull of that
// polygon placed at both segment endpoints, and the union of those capsules over all segments is the
// round-join / round-cap shape that geos_buffer_linestring and bp_buffer_linestring produce -- so the
// three implementations stay comparable. Sub-linestrings are separated by undefined points, exactly as in
// bg_load_multi_linestring / bp_load_multi_linestring above.
//
// Issue #1172: this replaces an offset of the INTERIOR straight skeleton of the point sequence read as a
// closed ring. That is a polygon shrink rather than a buffer, and it is empty for any open polyline (a
// 2-point linestring has no interior at all), which is why cgal_buffer_linestring always returned nothing.
template <typename P>
void cgal_buffer_multi_linestring(CGAL_Traits::Polygon_set& resPS, SA_ConstReference<P> multiLineStringRef
	, const CGAL_Traits::Ring& circle
	, std::vector<CGAL_Traits::Ring>& capsules, std::vector<CGAL_Traits::Point>& helperPoints)
{
	resPS.clear();
	capsules.clear();

	std::vector<CGAL_Traits::Point> hullPoints;

	auto appendCapsule = [&](P a, P b)
	{
		CGAL::Aff_transformation_2<CGAL_Traits::Kernel> translateA(CGAL::TRANSLATION, CGAL_Traits::Kernel::Vector_2(a.X(), a.Y()));
		CGAL::Aff_transformation_2<CGAL_Traits::Kernel> translateB(CGAL::TRANSLATION, CGAL_Traits::Kernel::Vector_2(b.X(), b.Y()));

		helperPoints.clear();
		helperPoints.reserve(2 * circle.size());
		for (const auto& v : circle)
		{
			helperPoints.push_back(translateA.transform(v));
			helperPoints.push_back(translateB.transform(v));
		}

		hullPoints.clear();
		CGAL::convex_hull_2(helperPoints.begin(), helperPoints.end(), std::back_inserter(hullPoints));
		if (hullPoints.size() < 3) // degenerate disc (bufferDistance <= 0): nothing to join
			return;
		capsules.emplace_back(hullPoints.begin(), hullPoints.end()); // convex_hull_2 yields counterclockwise
	};

	auto lineStringBegin = begin_ptr(multiLineStringRef)
		, sequenceEnd = end_ptr(multiLineStringRef);

	while (lineStringBegin != sequenceEnd)
	{
		if (!IsDefined(*lineStringBegin))
		{
			++lineStringBegin;
			continue;
		}
		auto lineStringEnd = lineStringBegin + 1;
		while (lineStringEnd != sequenceEnd && IsDefined(*lineStringEnd))
			++lineStringEnd;

		if (lineStringEnd - lineStringBegin == 1)
			appendCapsule(lineStringBegin[0], lineStringBegin[0]); // isolated point: the disc itself
		else
			for (auto p = lineStringBegin; p + 1 != lineStringEnd; ++p)
				appendCapsule(p[0], p[1]);

		lineStringBegin = lineStringEnd;
	}

	if (!capsules.empty())
		resPS.join(capsules.begin(), capsules.end()); // one divide & conquer union, not N incremental ones
}

// *****************************************************************************
//	Minkowski sum and difference, per backend (issue #917)
// *****************************************************************************
//
// boost.polygon and CGAL each have a Minkowski primitive and use it. boost.geometry and GEOS have
// none, so they build the answer out of the convex cells that minkowski.h derives -- see the proof
// of the identity there, and why an OR-union of boost.polygon's oriented convolution quads would
// NOT do.
//
// The difference (erosion) is the complement identity on all four backends:
//
//     A (-) K  =  A \ ( (R \ A) (+) -K ),   R a box containing A (+) -K with room to spare
//
// which is the same trick boost.polygon's polygon_set_data::resize plays for a negative resize.
// Callers hand in the ALREADY REFLECTED kernel, so the sum inside the identity is the ordinary one.

// The box that the erosion inverts within: A's extent, grown past anything -K can reach outward,
// so that R \ A really is the outside and no part of the eroded rim is clipped by R's own edge.
inline auto MinkowskiErosionPad(Float64 reach) -> Float64
{
	return 2.0 * reach + 1.0;
}

inline auto MinkowskiErosionPad(const PreparedMinkowskiKernel& reflectedKernel) -> Float64
{
	return MinkowskiErosionPad(reflectedKernel.reach);
}

// ---- boost.geometry ---------------------------------------------------------

struct union_bg_multi_polygon_lean
{
	void operator()(bg_multi_polygon_t& lvalue, bg_multi_polygon_t&& rvalue) const
	{
		if (rvalue.empty())
			return;
		if (lvalue.empty())
		{
			lvalue = std::move(rvalue);
			return;
		}
		bg_multi_polygon_t result;
		bg_union()(lvalue, rvalue, result);
		result.swap(lvalue);
	}
};

inline void bg_minkowski_sum(bg_multi_polygon_t& res, const bg_multi_polygon_t& a, const PreparedMinkowskiKernel& kernel)
{
	res.clear();
	if (a.empty() || kernel.empty())
		return;

	assoc_tower<bg_multi_polygon_t, union_bg_multi_polygon_lean> tower;
	std::vector<DPoint> scratch;

	auto addCell = [&tower](const MinkowskiRing& cell)
	{
		if (cell.size() < 4) // degenerate: a collinear kernel part along a zero-length edge
			return;
		bg_polygon_t poly;
		poly.outer().assign(cell.begin(), cell.end());
		boost::geometry::correct(poly); // the cells come out counter-clockwise; bg rings are clockwise
		bg_multi_polygon_t mp;
		mp.push_back(std::move(poly));
		tower.add(std::move(mp));
	};

	for (const auto& part : kernel.parts)
	{
		bg_multi_polygon_t shifted = a; // (A + c), the term that fills the interior
		move(shifted, part.front());
		tower.add(std::move(shifted));

		for (const auto& poly : a)
		{
			auto addRingCells = [&](const bg_ring_t& ring)
			{
				for (SizeT i = 0, n = ring.size(); i + 1 < n; ++i)
					addCell(MinkowskiEdgeCell(ring[i], ring[i + 1], part, scratch));
			};
			addRingCells(poly.outer());
			for (const auto& inner : poly.inners())
				addRingCells(inner);
		}
	}
	res = tower.get_result();
}

inline void bg_minkowski_difference(bg_multi_polygon_t& res, const bg_multi_polygon_t& a, const PreparedMinkowskiKernel& reflectedKernel)
{
	res.clear();
	if (a.empty() || reflectedKernel.empty())
		return;

	boost::geometry::model::box<DPoint> env;
	boost::geometry::envelope(a, env);
	auto pad = MinkowskiErosionPad(reflectedKernel);

	bg_polygon_t box;
	auto lo = env.min_corner(), hi = env.max_corner();
	box.outer().assign({
		  MinkowskiKernelPoint(lo.X() - pad, lo.Y() - pad)
		, MinkowskiKernelPoint(lo.X() - pad, hi.Y() + pad)
		, MinkowskiKernelPoint(hi.X() + pad, hi.Y() + pad)
		, MinkowskiKernelPoint(hi.X() + pad, lo.Y() - pad)
		, MinkowskiKernelPoint(lo.X() - pad, lo.Y() - pad)
		});
	boost::geometry::correct(box);

	bg_multi_polygon_t outside, grownOutside;
	bg_multi_polygon_t boxMP;
	boxMP.push_back(std::move(box));

	// bg_checked_operation may hand its arguments to clean_bg_geometry, which takes an rvalue, so
	// both operands have to be modifiable here.
	bg_multi_polygon_t subject = a;

	bg_difference()(boxMP, subject, outside);
	bg_minkowski_sum(grownOutside, outside, reflectedKernel);
	bg_difference()(subject, grownOutside, res);
}

// ---- GEOS -------------------------------------------------------------------

// GEOS geometries carry no translate of their own in this build, and the cells need the rings
// anyway, so both come out of one extraction pass.
struct GeosPolygonRings
{
	MinkowskiRing outer;
	std::vector<MinkowskiRing> holes;
};

inline void geos_extract_rings(const geos::geom::Geometry* geometry, std::vector<GeosPolygonRings>& result)
{
	result.clear();
	if (!geometry)
		return;

	auto appendRing = [](const geos::geom::LineString* ls, MinkowskiRing& into)
	{
		into.clear();
		if (!ls)
			return;
		const auto* cs = ls->getCoordinatesRO();
		into.reserve(cs->size());
		for (std::size_t i = 0, n = cs->size(); i != n; ++i)
			into.push_back(MinkowskiKernelPoint(cs->getX(i), cs->getY(i)));
	};

	for (std::size_t g = 0, ng = geometry->getNumGeometries(); g != ng; ++g)
	{
		const auto* polygon = dynamic_cast<const geos::geom::Polygon*>(geometry->getGeometryN(g));
		if (!polygon || polygon->isEmpty())
			continue;

		GeosPolygonRings rings;
		appendRing(polygon->getExteriorRing(), rings.outer);
		rings.holes.resize(polygon->getNumInteriorRing());
		for (std::size_t h = 0, nh = polygon->getNumInteriorRing(); h != nh; ++h)
			appendRing(polygon->getInteriorRingN(h), rings.holes[h]);
		result.push_back(std::move(rings));
	}
}

inline auto geos_build_translated(const std::vector<GeosPolygonRings>& rings, DPoint delta
	, geos_create_linear_ring_helper_data<DPoint>& tmpRingData) -> std::unique_ptr<geos::geom::Geometry>
{
	std::vector<std::unique_ptr<geos::geom::Geometry>> polygons;
	MinkowskiRing moved;

	auto movedRing = [&](const MinkowskiRing& ring) -> std::unique_ptr<geos::geom::LinearRing>
	{
		if (ring.size() < 4)
			return {};
		moved.clear();
		moved.reserve(ring.size());
		for (auto p : ring)
			moved.push_back(MinkowskiKernelPoint(p.X() + delta.X(), p.Y() + delta.Y()));
		return geos_create_linear_ring<DPoint>(moved.data(), moved.data() + moved.size(), tmpRingData);
	};

	for (const auto& polygonRings : rings)
	{
		auto shell = movedRing(polygonRings.outer);
		if (!shell)
			continue;
		std::vector<std::unique_ptr<geos::geom::LinearRing>> holes;
		for (const auto& hole : polygonRings.holes)
			if (auto h = movedRing(hole))
				holes.push_back(std::move(h));
		polygons.push_back(geos_factory()->createPolygon(std::move(shell), std::move(holes)));
	}
	if (polygons.empty())
		return {};
	return geos_factory()->createGeometryCollection(std::move(polygons));
}

inline auto geos_minkowski_sum(const geos::geom::Geometry* a, const PreparedMinkowskiKernel& kernel)
-> std::unique_ptr<geos::geom::Geometry>
{
	if (!a || a->isEmpty() || kernel.empty())
		return {};

	std::vector<GeosPolygonRings> rings;
	geos_extract_rings(a, rings);
	if (rings.empty())
		return {};

	geos_create_linear_ring_helper_data<DPoint> tmpRingData;
	std::vector<DPoint> scratch;
	std::vector<std::unique_ptr<geos::geom::Geometry>> cells;

	auto addCell = [&](const MinkowskiRing& cell)
	{
		if (cell.size() < 4)
			return;
		auto ring = geos_create_linear_ring<DPoint>(cell.data(), cell.data() + cell.size(), tmpRingData);
		if (ring)
			cells.push_back(geos_factory()->createPolygon(std::move(ring), {}));
	};

	for (const auto& part : kernel.parts)
	{
		if (auto shifted = geos_build_translated(rings, part.front(), tmpRingData)) // (A + c)
			cells.push_back(std::move(shifted));

		for (const auto& polygonRings : rings)
		{
			auto addRingCells = [&](const MinkowskiRing& ring)
			{
				for (SizeT i = 0, n = ring.size(); i + 1 < n; ++i)
					addCell(MinkowskiEdgeCell(ring[i], ring[i + 1], part, scratch));
			};
			addRingCells(polygonRings.outer);
			for (const auto& hole : polygonRings.holes)
				addRingCells(hole);
		}
	}
	if (cells.empty())
		return {};

	// One cascaded unary union over every cell at once; unioning them in pairs would be quadratic.
	auto collection = geos_factory()->createGeometryCollection(std::move(cells));
	auto result = collection->Union();
	cleanupPolygons(result);
	return result;
}

inline auto geos_minkowski_difference(const geos::geom::Geometry* a, const PreparedMinkowskiKernel& reflectedKernel)
-> std::unique_ptr<geos::geom::Geometry>
{
	if (!a || a->isEmpty() || reflectedKernel.empty())
		return {};

	auto env = a->getEnvelopeInternal();
	auto pad = MinkowskiErosionPad(reflectedKernel);

	MinkowskiRing boxRing{
		  MinkowskiKernelPoint(env->getMinX() - pad, env->getMinY() - pad)
		, MinkowskiKernelPoint(env->getMaxX() + pad, env->getMinY() - pad)
		, MinkowskiKernelPoint(env->getMaxX() + pad, env->getMaxY() + pad)
		, MinkowskiKernelPoint(env->getMinX() - pad, env->getMaxY() + pad)
		, MinkowskiKernelPoint(env->getMinX() - pad, env->getMinY() - pad)
	};
	geos_create_linear_ring_helper_data<DPoint> tmpRingData;
	auto boxLinearRing = geos_create_linear_ring<DPoint>(boxRing.data(), boxRing.data() + boxRing.size(), tmpRingData);
	if (!boxLinearRing)
		return {};
	std::unique_ptr<geos::geom::Geometry> box = geos_factory()->createPolygon(std::move(boxLinearRing), {});

	auto outside = box->difference(a);
	auto grownOutside = geos_minkowski_sum(outside.get(), reflectedKernel);
	if (!grownOutside || grownOutside->isEmpty())
		return a->clone();

	auto result = a->difference(grownOutside.get());
	cleanupPolygons(result);
	return result;
}

// ---- CGAL -------------------------------------------------------------------

// CGAL has an exact Minkowski sum of its own (reduced convolution over the exact-construction
// kernel), so cgal_minkowski_sum does not go through the convex cells at all. The kernel arrives
// as a single closed ring; CGAL wants it counter-clockwise and without its closing duplicate.
inline auto cgal_make_kernel_polygon(const MinkowskiRing& kernelRing) -> CGAL_Traits::Ring
{
	CGAL_Traits::Ring result;
	for (SizeT i = 0, n = kernelRing.size() - 1; i != n; ++i)
		result.push_back(CGAL_Traits::Point(kernelRing[i].X(), kernelRing[i].Y()));
	if (result.orientation() == CGAL::CLOCKWISE)
		result.reverse_orientation();
	return result;
}

inline void cgal_minkowski_sum(CGAL_Traits::Polygon_set& res, const CGAL_Traits::Polygon_set& a, const CGAL_Traits::Ring& kernel)
{
	res.clear();
	if (a.is_empty() || kernel.size() < 3)
		return;

	std::vector<CGAL_Traits::Polygon_with_holes> parts;
	a.polygons_with_holes(std::back_inserter(parts));

	std::vector<CGAL_Traits::Polygon_with_holes> sums;
	sums.reserve(parts.size());
	for (const auto& part : parts)
		sums.push_back(CGAL::minkowski_sum_2(part, kernel));

	if (!sums.empty())
		res.join(sums.begin(), sums.end()); // one divide & conquer union
}

inline void cgal_minkowski_difference(CGAL_Traits::Polygon_set& res, const CGAL_Traits::Polygon_set& a
	, const CGAL_Traits::Ring& reflectedKernel, Float64 pad)
{
	res.clear();
	if (a.is_empty() || reflectedKernel.size() < 3)
		return;

	// Polygon_set_2 has no bbox() of its own; take the union of its parts' boxes.
	std::vector<CGAL_Traits::Polygon_with_holes> parts;
	a.polygons_with_holes(std::back_inserter(parts));
	if (parts.empty())
		return;
	auto bbox = parts.front().bbox();
	for (SizeT i = 1, n = parts.size(); i != n; ++i)
		bbox += parts[i].bbox();

	CGAL_Traits::Ring box;
	box.push_back(CGAL_Traits::Point(bbox.xmin() - pad, bbox.ymin() - pad));
	box.push_back(CGAL_Traits::Point(bbox.xmax() + pad, bbox.ymin() - pad));
	box.push_back(CGAL_Traits::Point(bbox.xmax() + pad, bbox.ymax() + pad));
	box.push_back(CGAL_Traits::Point(bbox.xmin() - pad, bbox.ymax() + pad));

	CGAL_Traits::Polygon_set outside(box);
	outside.difference(a);

	CGAL_Traits::Polygon_set grownOutside;
	cgal_minkowski_sum(grownOutside, outside, reflectedKernel);

	res = a;
	res.difference(grownOutside);
}

// ---- boost.polygon ----------------------------------------------------------

// boost.polygon's own convolution, which accumulates under the winding rule and therefore needs
// no convex decomposition and no assumption that the kernel contains the origin.
template <typename C>
void bp_minkowski_sum(bp::polygon_set_data<C>& res, const bp::polygon_set_data<C>& a, const bp::polygon_set_data<C>& kernel
	, typename bp::polygon_set_data<C>::convolve_resources& resources)
{
	res.clear();
	bp::detail::minkowski_offset<C>::convolve_two_polygon_sets(res, a, kernel, resources);
	res.clean(resources.cleanResources);
}

template <typename C>
void bp_minkowski_difference(bp::polygon_set_data<C>& res, const bp::polygon_set_data<C>& a
	, const bp::polygon_set_data<C>& reflectedKernel, Float64 pad
	, typename bp::polygon_set_data<C>::convolve_resources& resources)
{
	using namespace bp::operators;

	res.clear();
	if (a.empty())
		return;

	// polygon_set_data::extents and the set operators below are non-const, so work on a copy.
	bp::polygon_set_data<C> subject = a;

	bp::rectangle_data<C> box;
	if (!subject.extents(box))
		return;
	bp::bloat(box, static_cast<typename bp::coordinate_traits<C>::unsigned_area_type>(pad + 1));

	bp::polygon_set_data<C> boxSet;
	boxSet.insert(box);

	// The set operators yield a polygon_set_view, and in this fork evaluating one needs the clean
	// resources handed in explicitly; bp::assign is the way to do that (see PolyOper.h).
	bp::polygon_set_data<C> outside;
	bp::assign(outside, boxSet - subject, resources.cleanResources);

	bp::polygon_set_data<C> grownOutside;
	bp_minkowski_sum<C>(grownOutside, outside, reflectedKernel, resources);

	bp::assign(res, subject - grownOutside, resources.cleanResources);
	res.clean(resources.cleanResources);
}

// ---- the kernel as the operators handle it ----------------------------------

// boost.polygon's state only exists for the boost.polygon backend: naming
// polygon_set_data<Float64>::convolve_resources would instantiate the integer-only machinery for
// the float point types the other three backends serve.
template <typename C, bool Enabled> struct BpMinkowskiState {};

template <typename C> struct BpMinkowskiState<C, true>
{
	bp::polygon_set_data<C> kernel;
	typename bp::polygon_set_data<C>::convolve_resources resources;
};

// The kernel argument as a plain ring. A Minkowski kernel is a single ring by definition here:
// boost.polygon's own convolution documents the same restriction ("kernel is a single ring, no
// holes nor islands"), and keeping all four backends to it is what lets them agree.
template <typename P>
auto MinkowskiKernelRingFromSequence(SA_ConstReference<P> kernelRef, CharPtr operName) -> MinkowskiRing
{
	SA_ConstRingIterator<P> ri(kernelRef, 0), re(kernelRef, -1);
	if (ri == re)
		throwErrorF(operName, "the kernel is empty");

	auto firstRing = *ri;
	MinkowskiRing result(firstRing.begin(), firstRing.end());

	++ri; // SA_ConstRingIterator::operator++ returns void
	if (ri != re)
		throwErrorF(operName, "the kernel must be a single ring; holes and islands are not supported. "
			"Build it from one closed sequence of points, e.g. with points2polygon");

	if (result.size() > 1 && (result.front().X() != result.back().X() || result.front().Y() != result.back().Y()))
		result.push_back(result.front()); // tolerate an unclosed ring rather than silently dropping an edge
	if (result.size() < 4)
		throwErrorF(operName, "the kernel has fewer than 3 distinct points and encloses no area");

	return result;
}

// One prepared kernel plus the per-element scratch, so that the common case -- a Void-domain
// kernel or a parameter size, i.e. one kernel for the whole attribute -- prepares exactly once
// per operator call rather than once per element.
template <typename P, geometry_library GL, bool Erode>
struct MinkowskiEngine
{
	using CoordType = scalar_of_t<P>;

	void SetKernelRing(const MinkowskiRing& ring, CharPtr operName)
	{
		// xx_minkowski_difference erodes, and A (-) K = A \ ((R \ A) (+) -K). Reflecting once here
		// is what lets everything below stay an ordinary sum.
		auto effective = Erode ? MinkowskiReflect(ring) : ring;
		m_Pad = MinkowskiErosionPad(MinkowskiReach(effective));

		if constexpr (GL == geometry_library::cgal)
		{
			m_CgalKernel = cgal_make_kernel_polygon(effective);
			if (m_CgalKernel.size() < 3 || !m_CgalKernel.is_simple())
				throwErrorF(operName, "the kernel is not a simple ring with at least 3 distinct points");
		}
		else if constexpr (GL == geometry_library::boost_polygon)
		{
			// Inserted through the same polygon_set_traits the geometry arguments go through, so
			// the kernel picks up the DMS ring convention instead of a second, hand-rolled one.
			std::vector<P> points;
			points.reserve(effective.size());
			for (auto p : effective)
			{
				P q;
				q.X() = CoordType(p.X());
				q.Y() = CoordType(p.Y());
				points.push_back(q);
			}
			using vector_traits = bp::polygon_set_traits<std::vector<P>>;
			m_Bp.kernel.clear();
			m_Bp.kernel.insert(vector_traits::begin(points), vector_traits::end(points));
			m_Bp.kernel.clean(m_Bp.resources.cleanResources);
			if (m_Bp.kernel.empty())
				throwErrorF(operName, "the kernel encloses no area at the resolution of the integer coordinates; "
					"use a larger kernel or a float-coordinate backend");
		}
		else
		{
			m_Kernel = PrepareMinkowskiKernel(effective);
			if (m_Kernel.empty())
				throwErrorF(operName, "the kernel is not a simple ring with at least 3 distinct points");
		}
	}

	template <typename ResRef, typename GeomRef>
	void Apply(ResRef&& resRef, GeomRef&& geometryRef)
	{
		if constexpr (GL == geometry_library::boost_geometry)
		{
			assign_multi_polygon(m_BgGeometry, geometryRef, true, m_BgHelperPolygon, m_BgHelperRing);
			if (m_BgGeometry.empty())
				return;
			if constexpr (Erode)
				bg_minkowski_difference(m_BgResult, m_BgGeometry, m_Kernel);
			else
				bg_minkowski_sum(m_BgResult, m_BgGeometry, m_Kernel);
			bg_store_multi_polygon(resRef, m_BgResult);
		}
		else if constexpr (GL == geometry_library::geos)
		{
			auto geometry = geos_create_polygons(geometryRef);
			if (!geometry || geometry->isEmpty())
				return;
			auto result = Erode
				? geos_minkowski_difference(geometry.get(), m_Kernel)
				: geos_minkowski_sum(geometry.get(), m_Kernel);
			geos_assign_geometry(resRef, result.get());
		}
		else if constexpr (GL == geometry_library::cgal)
		{
			assign_multi_polygon(m_CgalGeometry, geometryRef, true, m_CgalHelperPolygon, m_CgalHelperRing);
			if (m_CgalGeometry.is_empty())
				return;
			if constexpr (Erode)
				cgal_minkowski_difference(m_CgalResult, m_CgalGeometry, m_CgalKernel, m_Pad);
			else
				cgal_minkowski_sum(m_CgalResult, m_CgalGeometry, m_CgalKernel);
			cgal_assign_polygon_set(resRef, m_CgalResult);
		}
		else if constexpr (GL == geometry_library::boost_polygon)
		{
			bp::polygon_set_data<CoordType> geometry, result;
			bp::assign(geometry, geometryRef, m_Bp.resources.cleanResources);
			if (geometry.empty())
				return;
			if constexpr (Erode)
				bp_minkowski_difference<CoordType>(result, geometry, m_Bp.kernel, m_Pad, m_Bp.resources);
			else
				bp_minkowski_sum<CoordType>(result, geometry, m_Bp.kernel, m_Bp.resources);
			bp_assign(resRef, result, m_Bp.resources.cleanResources);
		}
		else
			static_assert(unsupported_geometry_library_v<GL>);
	}

private:
	PreparedMinkowskiKernel m_Kernel;   // boost.geometry and GEOS: the convex cells
	Float64 m_Pad = 0;                  // erosion: how far outside A the inverted box must reach

	bg_ring_t m_BgHelperRing;
	bg_polygon_t m_BgHelperPolygon;
	bg_multi_polygon_t m_BgGeometry, m_BgResult;

	CGAL_Traits::Ring m_CgalKernel, m_CgalHelperRing;
	CGAL_Traits::Polygon_with_holes m_CgalHelperPolygon;
	CGAL_Traits::Polygon_set m_CgalGeometry, m_CgalResult;

	BpMinkowskiState<CoordType, GL == geometry_library::boost_polygon> m_Bp;
};

// *****************************************************************************
//	operation groups
// *****************************************************************************


extern CommonOperGroup grBgSimplify_polygon; // defined in BoostGeometry.cpp; referenced inside SimplifyPolygonOperator's ctor

// *****************************************************************************
//	map algebraic operations on boost geometry polygons
// *****************************************************************************

template <typename P> using sequence_t = sequence_traits<P>::container_type;
template <typename P> using BinaryMapAlgebraicOperator = BinaryAttrOper<sequence_t<P>, sequence_t<P>, sequence_t<P>>;

template <typename P, typename BinaryBgMpOper>
struct BgMultiPolygonOperator : BinaryMapAlgebraicOperator<P>
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using ArgType = DataArray<PolygonType>;

	BgMultiPolygonOperator(AbstrOperGroup& gr, BinaryBgMpOper&& oper = BinaryBgMpOper())
		: BinaryMapAlgebraicOperator<P>(&gr, compatible_simple_values_unit_creator, ValueComposition::Polygon)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[0]), ValueComposition::Polygon);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[1]), ValueComposition::Polygon);
		return BinaryMapAlgebraicOperator<P>::CreateResult(resultHolder, args, mustCalc);
	}

	using st = sequence_traits<PolygonType>;
	using seq_t = typename st::seq_t;
	using cseq_t = typename st::cseq_t;

	void CalcTile(seq_t resData, cseq_t arg1Data, cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		tile_offset n1 = arg1Data.size();
		tile_offset n2 = arg2Data.size();
		tile_offset n = std::max(n1, n2);
		assert(n1 == n || (af & AF1_ISPARAM));
		assert(n2 == n || (af & AF2_ISPARAM));
		assert(resData.size() == n);

		bg_ring_t helperRing;
		bg_polygon_t helperPolygon;
		bg_multi_polygon_t currMP1, currMP2, resMP;

		bool domain1IsVoid = (af & AF1_ISPARAM);
		bool domain2IsVoid = (af & AF2_ISPARAM);
		if (domain1IsVoid)
			assign_multi_polygon(currMP1, arg1Data[0], true, helperPolygon, helperRing);
		if (domain2IsVoid)
			assign_multi_polygon(currMP2, arg2Data[0], true, helperPolygon, helperRing);

		for (SizeT i = 0; i != n; ++i)
		{
			if (!domain1IsVoid)
				assign_multi_polygon(currMP1, arg1Data[i], true, helperPolygon, helperRing);
			if (!domain2IsVoid)
				assign_multi_polygon(currMP2, arg2Data[i], true, helperPolygon, helperRing);
			resMP.clear();
			m_Oper(currMP1, currMP2, resMP);
			bg_store_multi_polygon(resData[i], resMP);
		}
	}

	[[no_unique_address]] BinaryBgMpOper m_Oper{};
};

// *****************************************************************************
//	map algebraic operations on CGAL polygons
// *****************************************************************************

template <typename P, typename BinaryBgMpOper>
struct CGAL_MultiPolygonOperator : BinaryMapAlgebraicOperator<P>
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using ArgType = DataArray<PolygonType>;

	CGAL_MultiPolygonOperator(AbstrOperGroup& gr, BinaryBgMpOper&& oper = BinaryBgMpOper())
		: BinaryMapAlgebraicOperator<P>(&gr, compatible_simple_values_unit_creator, ValueComposition::Polygon)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[0]), ValueComposition::Polygon);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[1]), ValueComposition::Polygon);
		return BinaryMapAlgebraicOperator<P>::CreateResult(resultHolder, args, mustCalc);
	}

	using st = sequence_traits<PolygonType>;
	using seq_t = typename st::seq_t;
	using cseq_t = typename st::cseq_t;

	void CalcTile(seq_t resData, cseq_t arg1Data, cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		tile_offset n1 = arg1Data.size();
		tile_offset n2 = arg2Data.size();
		tile_offset n = std::max(n1, n2);
		assert(n1 == n || (af & AF1_ISPARAM));
		assert(n2 == n || (af & AF2_ISPARAM));
		assert(resData.size() == n);

		CGAL_Traits::Ring helperRing;
		CGAL_Traits::Polygon_with_holes helperPolygon;
		CGAL_Traits::Polygon_set currMP1, currMP2, resMP;
		std::vector<DPoint> helperPointArray;

		bool domain1IsVoid = (af & AF1_ISPARAM);
		bool domain2IsVoid = (af & AF2_ISPARAM);
		if (domain1IsVoid)
			assign_multi_polygon(currMP1, arg1Data[0], true, helperPolygon, helperRing);
		if (domain2IsVoid)
			assign_multi_polygon(currMP2, arg2Data[0], true, helperPolygon, helperRing);

		for (SizeT i = 0; i != n; ++i)
		{
			if (!domain1IsVoid)
				assign_multi_polygon(currMP1, arg1Data[i], true, helperPolygon, helperRing);
			if (!domain2IsVoid)
				assign_multi_polygon(currMP2, arg2Data[i], true, helperPolygon, helperRing);
			resMP.clear();
			m_Oper(currMP1, currMP2, resMP);
			cgal_assign_polygon_set(resData[i], resMP);
		}
	}
	[[no_unique_address]] BinaryBgMpOper m_Oper;
};

// *****************************************************************************
//	map algebraic operations on GEOS polygons
// *****************************************************************************

template <typename P, typename BinaryBgMpOper>
struct GEOS_MultiPolygonOperator : BinaryMapAlgebraicOperator<P>
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using ArgType = DataArray<PolygonType>;

	GEOS_MultiPolygonOperator(AbstrOperGroup& gr, BinaryBgMpOper&& oper = BinaryBgMpOper())
		: BinaryMapAlgebraicOperator<P>(&gr, compatible_simple_values_unit_creator, ValueComposition::Polygon)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[0]), ValueComposition::Polygon);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[1]), ValueComposition::Polygon);
		return BinaryMapAlgebraicOperator<P>::CreateResult(resultHolder, args, mustCalc);
	}

	using st = sequence_traits<PolygonType>;
	using seq_t = typename st::seq_t;
	using cseq_t = typename st::cseq_t;

	void CalcTile(seq_t resData, cseq_t arg1Data, cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		tile_offset n1 = arg1Data.size();
		tile_offset n2 = arg2Data.size();
		tile_offset n = std::max(n1, n2);
		assert(n1 == n || (af & AF1_ISPARAM));
		assert(n2 == n || (af & AF2_ISPARAM));
		assert(resData.size() == n);

		std::unique_ptr<geos::geom::Geometry> currMP1, currMP2, resMP;

		bool domain1IsVoid = (af & AF1_ISPARAM);
		bool domain2IsVoid = (af & AF2_ISPARAM);
		if (domain1IsVoid)
			currMP1 = geos_create_polygons(arg1Data[0]);
		if (domain2IsVoid)
			currMP2 = geos_create_polygons(arg2Data[0]);

		for (SizeT i = 0; i != n; ++i)
		{
			if (!domain1IsVoid)
				currMP1 = geos_create_polygons(arg1Data[i]);
			if (!domain2IsVoid)
				currMP2 = geos_create_polygons(arg2Data[i]);
			resMP = m_Oper(currMP1.get(), currMP2.get());
			geos_assign_geometry(resData[i], resMP.get());
		}
	}
	[[no_unique_address]] BinaryBgMpOper m_Oper;
};

// *****************************************************************************
//	xx_minkowski_sum / xx_minkowski_difference operators (issue #917)
// *****************************************************************************

// Signature 1: xx_minkowski_sum(geometry, kernel). The kernel is a polygon attribute, Void-domain
// for one kernel over the whole attribute or domain-matching for a kernel per element. This is the
// general form: the kernel is NOT assumed to contain the origin, so a kernel spanning (0,0)..(a,b)
// displaces the result, exactly as a Minkowski sum should.
template <typename P, geometry_library GL, bool Erode>
struct MinkowskiKernelOperator : BinaryMapAlgebraicOperator<P>
{
	using PolygonType = typename sequence_traits<P>::container_type;

	MinkowskiKernelOperator(AbstrOperGroup& gr)
		: BinaryMapAlgebraicOperator<P>(&gr, compatible_simple_values_unit_creator, ValueComposition::Polygon)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[0]), ValueComposition::Polygon);
		CheckGeometryArgComposition(this->GetGroup(), AsDataItem(args[1]), ValueComposition::Polygon);
		return BinaryMapAlgebraicOperator<P>::CreateResult(resultHolder, args, mustCalc);
	}

	using st = sequence_traits<PolygonType>;
	using seq_t = typename st::seq_t;
	using cseq_t = typename st::cseq_t;

	void CalcTile(seq_t resData, cseq_t arg1Data, cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		tile_offset n1 = arg1Data.size();
		tile_offset n2 = arg2Data.size();
		tile_offset n = std::max(n1, n2);
		assert(n1 == n || (af & AF1_ISPARAM));
		assert(n2 == n || (af & AF2_ISPARAM));
		assert(resData.size() == n);

		bool geometryIsParam = (af & AF1_ISPARAM);
		bool kernelIsParam = (af & AF2_ISPARAM);

		CharPtr operName = this->GetGroup()->GetNameStr();
		MinkowskiEngine<P, GL, Erode> engine;
		if (kernelIsParam)
			engine.SetKernelRing(MinkowskiKernelRingFromSequence<P>(arg2Data[0], operName), operName);

		for (SizeT i = 0; i != n; ++i)
		{
			if (!kernelIsParam)
				engine.SetKernelRing(MinkowskiKernelRingFromSequence<P>(arg2Data[i], operName), operName);
			engine.Apply(resData[i], arg1Data[geometryIsParam ? 0 : i]);
		}
	}
};

// Signature 2: xx_minkowski_sum(geometry, size, variant). The twelve classic kernels that used to
// be operator-name suffixes (bp_polygon_i4HV and friends), now selected by name.
class AbstrMinkowskiNamedOperator : public TernaryOperator
{
protected:
	AbstrMinkowskiNamedOperator(AbstrOperGroup& og, const DataItemClass* polyAttrClass, bool erode, CharPtr siblingOperName)
		: TernaryOperator(&og, polyAttrClass
			, polyAttrClass
			, DataArray<Float64>::GetStaticClass()
			, DataArray<SharedStr>::GetStaticClass()
		)
		, m_Erode(erode)
		, m_SiblingOperName(siblingOperName)
	{}

	bool m_Erode;
	CharPtr m_SiblingOperName;

	// Same reasoning as AbstrBufferOperator's estimate: the transient geometry lives outside the
	// DMS allocator (boost.geometry's cells, GEOS's collection, CGAL's arrangement), one element's
	// worth per chore in flight, and the ledger would otherwise book only the packed result.
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = TernaryOperator::EstimatePerformance(resultHolder, args);

		auto nrElements = result.resultingNrElements;
		if (!nrElements || !result.inputSize)
			return result;

		// A Minkowski sum multiplies vertices at every corner, like a rounded buffer does, and the
		// cell union holds them all at once before it collapses.
		static constexpr SizeT GEOM_INFLATION = 8;

		auto avgElementBytes = result.inputSize / nrElements;
		auto inflight = Min<SizeT>(result.nrChores ? result.nrChores : 1, MaxConcurrentTreads());
		result.workingMemorySizePerChore = avgElementBytes * GEOM_INFLATION;
		result.workingMemorySize = result.workingMemorySizePerChore * inflight;
		return result;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		assert(arg1A && arg2A && arg3A);

		CheckGeometryArgComposition(GetGroup(), arg1A, ValueComposition::Polygon);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* domain3Unit = arg3A->GetAbstrDomainUnit(); bool e3IsVoid = domain3Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();

		domain1Unit->UnifyDomain(domain2Unit, "e1", "e2", UnifyMode(UM_Throw | UM_AllowVoidRight));

		// The variant names one of twelve fixed rings and is read once, before any tile runs.
		if (!e3IsVoid)
			arg3A->throwItemError("the kernel variant must be a parameter, not an attribute");

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, values1Unit, ValueComposition::Polygon);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			auto variantSpec = GetTheCurrValue<SharedStr>(arg3A);
			auto shape = ParseMinkowskiKernelShape(variantSpec.c_str(), m_Erode, GetGroup()->GetNameStr(), m_SiblingOperName);

			Float64 size = e2IsVoid ? const_array_cast<Float64>(arg2A)->GetLockedDataRead()[0] : 0;

			Timer processTimer;
			auto itemRef = resultHolder.GetProgressPrefix(); // #795: names the config item

			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(domain1Unit->GetNrTiles(), [=, this, resObj = resLock.get(), &processTimer, itemRefPtr = itemRef.c_str()](tile_id t)->void
				{
					this->Calculate(resObj, arg1A, e2IsVoid, arg2A, size, shape, t, processTimer, itemRefPtr);
				}
			);

			resLock.Commit();
		}
		return true;
	}

	virtual void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* sizeItem, Float64 size
		, MinkowskiKernelShape shape
		, tile_id t, Timer& processTimer, CharPtr itemRef) const = 0;
};

template <typename P, geometry_library GL, bool Erode>
struct MinkowskiNamedOperator : AbstrMinkowskiNamedOperator
{
	using PolygonType = typename sequence_traits<P>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	MinkowskiNamedOperator(AbstrOperGroup& gr, CharPtr siblingOperName)
		: AbstrMinkowskiNamedOperator(gr, Arg1Type::GetStaticClass(), Erode, siblingOperName)
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* sizeItem, Float64 size
		, MinkowskiKernelShape shape
		, tile_id t, Timer& processTimer, CharPtr itemRef) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto sizeData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(sizeItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		SizeT n = polyData.size();
		CharPtr operName = GetGroup()->GetNameStr();

		MinkowskiEngine<P, GL, Erode> engine;
		if (e2IsVoid)
			engine.SetKernelRing(MakeMinkowskiKernel(shape, size), operName);

		for (SizeT i = 0; i != n; ++i)
		{
			if (!e2IsVoid)
				engine.SetKernelRing(MakeMinkowskiKernel(shape, sizeData[i]), operName);
			engine.Apply(resData[i], polyData[i]);

			if (processTimer.PassedSecs())
			{
				reportF(SeverityTypeID::ST_MajorTrace, "{}{}: processed {} / {} sequences of tile {} / {}"
					, itemRef
					, operName
					, AsString(i), AsString(n)
					, AsString(t), AsString(resObj->GetTiledRangeData()->GetNrTiles())
				);
			}
		}
	}
};

// *****************************************************************************
//	simplify
// *****************************************************************************

class AbstrSimplifyOperator : public BinaryOperator
{
protected:
	AbstrSimplifyOperator(AbstrOperGroup& gr, ValueComposition expectedInputVC, const DataItemClass* polyAttrClass)
		:	BinaryOperator(&gr, polyAttrClass
			,	polyAttrClass
			,	DataArray<Float64>::GetStaticClass()
			)
		,	m_ExpectedInputVC(expectedInputVC)
	{}

	ValueComposition m_ExpectedInputVC;

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2A);

		CheckGeometryArgComposition(GetGroup(), arg1A, m_ExpectedInputVC);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

		domain1Unit->UnifyDomain(domain2Unit, "e1", "e2", UnifyMode(UM_Throw| UM_AllowVoidRight));

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, values1Unit, arg1A->GetValueComposition());

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			Float64 maxError = const_array_cast<Float64>(arg2A)->GetLockedDataRead()[0];
			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(domain1Unit->GetNrTiles(), [this, resObj = resLock.get(), arg1A, maxError](tile_id t)->void
				{
					ReadableTileLock readPoly1Lock (arg1A->GetCurrRefObj().get(), t);

					Calculate(resObj, arg1A, maxError, t);
				}
			);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const = 0;
};

template <typename Polyline, typename K>
auto cgal_douglas_peucker(Polyline&& polyline, double sqr_tolerance) -> Polyline
{
	if (polyline.size() <= 2)
		return polyline; // Not enough points to simplify

	// Start and end points
	const auto& start = polyline.front();
	const auto& end = polyline.back();

	// Line from start to end
	typename K::Segment_2 line(start, end);

	// Find the point with the maximum distance from the line
	double max_distance_sq = 0.0;
	size_t index = 0;
	for (size_t i = 1; i < polyline.size() - 1; ++i) {
		double distance_sq = CGAL::squared_distance(polyline[i], line);
		if (distance_sq > max_distance_sq) {
			index = i;
			max_distance_sq = distance_sq;
		}
	}


	// Compare squared distance to squared tolerance to avoid unnecessary square roots
	if (max_distance_sq <= sqr_tolerance) {
		Polyline result = {};
		// The points between start and end are not significant; represent with a straight line
		result.reserve(2);
		result.push_back(start);
		result.push_back(end);
		return result;
	}
	// Recursively simplify the segments before and after the point with maximum distance
	Polyline first_segment(polyline.begin(), polyline.begin() + index + 1);
	Polyline second_segment(polyline.begin() + index, polyline.end());

	Polyline result = douglas_peucker(first_segment, sqr_tolerance);
	Polyline result2 = douglas_peucker(second_segment, sqr_tolerance);

	// Combine the results (avoid duplicating the middle point)
	result.insert(result.end(), result2.begin()+1, result2.end());
	return result;
}

template <typename P, geometry_library GL>
struct SimplifyMultiPolygonOperator : public AbstrSimplifyOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	SimplifyMultiPolygonOperator(AbstrOperGroup& aog)
		: AbstrSimplifyOperator(aog, ValueComposition::Polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		if constexpr (GL == geometry_library::boost_geometry)
		{
			bg_ring_t  currRing, resRing;
			std::vector<DPoint> ringClosurePoints;

			for (SizeT i = 0, n = polyData.size(); i != n; ++i)
			{
				auto polyDataElem = polyData[i];
				P lb = MaxValue<P>();
				for (auto p : polyDataElem)
					MakeLowerBound(lb, p);

				ringClosurePoints.clear();
				SA_ConstRingIterator<PointType> rb(polyDataElem, 0), re(polyDataElem, -1);
				auto ri = rb;
				dbg_assert(ri != re);
				if (ri == re)
					continue;
				for (; ri != re; ++ri)
				{
					assert((*ri).begin() != (*ri).end());
					assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

					currRing.assign((*ri).begin(), (*ri).end());
					assert(currRing.begin() != currRing.end());
					assert(currRing.begin()[0] == currRing.end()[-1]); // closed ?
					move(currRing, -DPoint(lb));
					if (empty(currRing))
						continue;

					boost::geometry::simplify(currRing, resRing, maxError);
					move(resRing, DPoint(lb));

					if (empty(resRing))
						continue;

					assert(resRing.begin()[0] == resRing.end()[-1]); // closed ?
					resData[i].append(resRing.begin(), resRing.end() MG_DEBUG_ALLOCATOR_SRC(GetGroup()->GetNameStr()));
					ringClosurePoints.emplace_back(resRing.end()[-1]);
				}
				if (ringClosurePoints.empty())
					continue;
				ringClosurePoints.pop_back();
				while (!ringClosurePoints.empty())
				{
					resData[i].emplace_back(MG_DEBUG_ALLOCATOR_FIRST(GetGroup()->GetNameStr()) ringClosurePoints.back());
					ringClosurePoints.pop_back();
				}
			}
		}
		else if constexpr (GL == geometry_library::cgal)
		{
			maxError *= maxError; // use the squared tolerance for efficiency
			for (SizeT i = 0, n = polyData.size(); i != n; ++i)
			{
				auto polyDataElem = polyData[i];
				CGAL_Traits::Ring currRing;
				assign_polyline(currRing, polyDataElem);
				auto resRing = cgal_douglas_peucker<CGAL_Traits::Ring, CGAL_Traits::Kernel>(std::move(currRing), maxError);
				cgal_assign_geometry(resData[i], resRing);
			}
		}
		else if constexpr (GL == geometry_library::geos)
		{
			for (SizeT i = 0, n = polyData.size(); i != n; ++i)
			{
				auto polyDataElem = polyData[i];
				auto currGeom = geos_create_polygons(polyDataElem);
				if (currGeom)
				{
					geos::simplify::DouglasPeuckerSimplifier simplifier(currGeom.get());
					simplifier.setDistanceTolerance(maxError);

					auto resGeom = simplifier.getResultGeometry();

					geos_assign_geometry(resData[i], resGeom.get());
				}
			}
		}
	}
};

template <typename P>
struct SimplifyPolygonOperator : public AbstrSimplifyOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	SimplifyPolygonOperator()
		: AbstrSimplifyOperator(grBgSimplify_polygon, ValueComposition::Polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		bg_ring_t currRing, resRing;
		std::vector<DPoint> ringClosurePoints;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			auto lb = MaxValue<PointType>();
			for (auto p : polyData[i])
				MakeLowerBound(lb, p);

			ringClosurePoints.clear();
			SA_ConstRingIterator<PointType> rb(polyData[i], 0), re(polyData[i], -1);
			auto ri = rb;
			dbg_assert(ri != re);
			if (ri == re)
				continue;
			for (; ri != re; ++ri)
			{
				assert((*ri).begin() != (*ri).end()); // non-empty ring, must be guaranteed by boost::polygon::SA_ConstRingIterator
//				dms_assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

				currRing.assign((*ri).begin(), (*ri).end());
				if ((*ri).begin()[0] != (*ri).end()[-1])
					currRing.emplace_back(currRing.front());
				assert(currRing.begin()[0] == currRing.end()[-1]); // closed !
				move(currRing, -DPoint(lb));

				boost::geometry::simplify(currRing, resRing, maxError);
				move(resRing, DPoint(lb));

				if (empty(resRing))
				{
					if (ri == rb) // if first ring is empty, assume all further rings are inner rings inside it (this is supposed not to be a multi_polygon)
						break;
					continue;
				}

				dms_assert(resRing.begin()[0] == resRing.end()[-1]); // closed ?
				resData[i].append(resRing.begin(), resRing.end() MG_DEBUG_ALLOCATOR_SRC(GetGroup()->GetNameStr()));
				ringClosurePoints.emplace_back(resRing.end()[-1]);
			}
			if (ringClosurePoints.empty())
				continue;
			ringClosurePoints.pop_back();
			while (!ringClosurePoints.empty())
			{
				resData[i].emplace_back(MG_DEBUG_ALLOCATOR_FIRST(GetGroup()->GetNameStr()) ringClosurePoints.back());
				ringClosurePoints.pop_back();
			}
		}
	}
};

template <typename P, geometry_library GL>
struct SimplifyLinestringOperator : public AbstrSimplifyOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	SimplifyLinestringOperator(AbstrOperGroup& operGroup)
		: AbstrSimplifyOperator(operGroup, ValueComposition::Sequence, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const override
	{
		auto lineStringData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(lineStringData.size() == resData.size());

		assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

		bg_multi_linestring_t currGeometry, resGeometry;
		bg_linestring_t helperLineString;

		for (SizeT i = 0, n = lineStringData.size(); i != n; ++i)
		{
			if constexpr (GL == geometry_library::boost_geometry)
			{
				bg_load_multi_linestring(currGeometry, lineStringData[i], helperLineString);
				resGeometry.resize(0);
				if (!currGeometry.empty())
				{
					auto lb = MaxValue<DPoint>();
					MakeLowerBound(lb, currGeometry);
					move(currGeometry, -lb);

					boost::geometry::simplify(currGeometry, resGeometry, maxError);
					move(resGeometry, DPoint(lb));

				}
				bg_store_multi_linestring(resData[i], resGeometry);
			}
			else if constexpr (GL == geometry_library::geos)
			{
				auto lineStringRef = lineStringData[i];
				auto lineString = geos_create_multi_linestring<P>(lineStringRef.begin(), lineStringRef.end());
				geos::simplify::DouglasPeuckerSimplifier simplifier(lineString.get());
				simplifier.setDistanceTolerance(maxError);
				auto simplifiedLinestring = simplifier.getResultGeometry();
				geos_assign_multi_linestring(resData[i], simplifiedLinestring.get());
			}
			else
			{
				static_assert(unsupported_geometry_library_v<GL>, "Unsupported geometry library for SimplifyLinestringOperator");
			}
		}
	}
};

// *****************************************************************************
//	buffer
// *****************************************************************************

class AbstrBufferOperator : public TernaryOperator
{
protected:
	AbstrBufferOperator(AbstrOperGroup& og, ValueComposition expectedInputVC, const DataItemClass* polyAttrClass, const DataItemClass* argAttrClass = nullptr)
		: TernaryOperator(&og, polyAttrClass
			, argAttrClass ? argAttrClass : polyAttrClass
			, DataArray<Float64>::GetStaticClass()
			, DataArray<UInt8>::GetStaticClass()
		)
		, m_ExpectedInputVC(expectedInputVC)
	{}

	ValueComposition m_ExpectedInputVC;

	// The buffer families hold their transient geometry OUTSIDE the DMS allocator -- GEOS builds a
	// MultiPolygon per element (geos_create_polygons) and buffers it, boost_geometry keeps currMP /
	// resMP / helperPolygon / helperRing -- so without a working-memory term the ledger books only
	// the packed result and misses the library-side representation entirely (§8.1.34).
	//
	// It is deliberately a PER-ELEMENT term, not a per-tile one: Calculate loops one sequence at a
	// time and releases each element's geometry before the next, so the honest transient is one
	// element's worth per chore in flight. Charging a whole tile here would over-book these
	// operators by the element count and throttle a workload that does not need it -- t301's real
	// consumer is polygon_connectivity, not the buffer.
	auto EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData override
	{
		auto result = TernaryOperator::EstimatePerformance(resultHolder, args);

		// The result domain IS the first argument's domain (UnifyDomain below), so the element count
		// is the result's own; inputSize is dominated by arg1, args 2-3 being void or scalar.
		auto nrElements = result.resultingNrElements;
		if (!nrElements || !result.inputSize)
			return result;

		// Covers the library's node-per-coordinate representation against the packed DMS point
		// sequence, plus the vertex multiplication a rounded buffer applies at every corner.
		static constexpr SizeT GEOM_INFLATION = 8;

		auto avgElementBytes = result.inputSize / nrElements;
		auto inflight = Min<SizeT>(result.nrChores ? result.nrChores : 1, MaxConcurrentTreads());
		result.workingMemorySizePerChore = avgElementBytes * GEOM_INFLATION;
		result.workingMemorySize = result.workingMemorySizePerChore * inflight;
		return result;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		assert(arg1A);
		assert(arg2A);
		assert(arg3A);

		CheckGeometryArgComposition(GetGroup(), arg1A, m_ExpectedInputVC);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

		const AbstrUnit* domain3Unit = arg3A->GetAbstrDomainUnit(); bool e3IsVoid = domain3Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
//		const AbstrUnit* values3Unit = arg3A->GetAbstrValuesUnit();

		domain1Unit->UnifyDomain(domain2Unit, "e1", "e2", UnifyMode(UM_Throw | UM_AllowVoidRight));
		domain1Unit->UnifyDomain(domain3Unit, "e1", "e3", UnifyMode(UM_Throw | UM_AllowVoidRight));

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, values1Unit, ValueComposition::Polygon);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			Timer processTimer;
			auto itemRef = resultHolder.GetProgressPrefix(); // #795: names the config item, also for an intermediate result

			Float64 bufferDistance = e2IsVoid ? const_array_cast<Float64>(arg2A)->GetLockedDataRead()[0] : 0;
			UInt8 nrPointsInCircle = e3IsVoid ? const_array_cast<UInt8  >(arg3A)->GetLockedDataRead()[0] : 0;

			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(domain1Unit->GetNrTiles(), [=, this, resObj = resLock.get(), &processTimer, itemRefPtr = itemRef.c_str()](tile_id t)->void
				{
					this->Calculate(resObj, arg1A
						, e2IsVoid, arg2A, bufferDistance
						, e3IsVoid, arg3A, nrPointsInCircle
						, t, processTimer, itemRefPtr);
				}
			);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resObj
		, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t, Timer& processTimer, CharPtr itemRef = "") const = 0;
};

template <typename CoordType>
auto bp_circle(double radius, int pointsPerCircle) -> std::vector<bp::point_data<CoordType> >
{
	if (pointsPerCircle < 3)
		pointsPerCircle = 3;
	using Point = bp::point_data<CoordType>;
	std::vector<Point> points;
	points.reserve(pointsPerCircle + 1);
	auto anglePerPoint = 2.0 * std::numbers::pi_v<double> / pointsPerCircle;
	for (int i = 0; i < pointsPerCircle; ++i) {
		double angle = i * anglePerPoint;
		int x = static_cast<int>(radius * std::cos(angle));
		int y = static_cast<int>(radius * std::sin(angle));
		points.emplace_back(x, y);
	}
	points.emplace_back(points.front());
	return points;
}

template <typename P, geometry_library GL>
struct BufferPointOperator : public AbstrBufferOperator
{
	using PointType = P;
	using CoordType = scalar_of_t<PointType>;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PointType>;
	using ResultType = DataArray<PolygonType>;

	BufferPointOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, ValueComposition::Single, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* pointItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t, Timer& processTimer, CharPtr itemRef = "") const override
	{
		auto pointData = const_array_cast<PointType>(pointItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData     = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem    )->GetTile(t);

		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(pointData.size() == resData.size());

		SizeT i=0, n = pointData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];
			if constexpr (GL == geometry_library::boost_geometry)
			{
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;

				std::vector<PointType> ringClosurePoints;
				assert(pointItem->GetValueComposition() == ValueComposition::Single);

				using bg_polygon_t = boost::geometry::model::polygon<DPoint>;

				boost::geometry::model::multi_polygon<bg_polygon_t> resMP;
				boost::geometry::buffer(DPoint(0, 0), resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);

				boost::geometry::model::ring<DPoint> resRing = resMP[0].outer();
				boost::geometry::model::ring<DPoint> movedRing;

				do {
					movedRing = resRing;
					move(movedRing, DPoint(pointData[i]));
					bg_store_ring(resData[i], movedRing);

					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::boost_polygon)
			{
				auto resRing = bp_circle<CoordType>(bufferDistance, pointsPerCircle);
				do {
					auto movedRing = resRing;
					move<CoordType>(movedRing, pointData[i]);

					bp_assign_ring(resData[i], movedRing.begin(), movedRing.end());

					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::cgal)
			{
				auto cgalCircle = cgal_circle<CoordType>(bufferDistance, pointsPerCircle);
				do {
					// Define the translation vector (dx, dy)
					CGAL_Traits::Kernel::Vector_2 translation_vector(pointData[i].X(), pointData[i].Y());

					// Define the affine transformation for translation
					CGAL::Aff_transformation_2<CGAL_Traits::Kernel> translate(CGAL::TRANSLATION, translation_vector);

					// Create a new polygon for the translated version
					CGAL::Polygon_2<CGAL_Traits::Kernel> translated_polygon;

					// Apply the translation to each vertex of the original polygon
					for (auto vertex : cgalCircle)
						translated_polygon.push_back(translate.transform(vertex));

					// Store the translated polygon
					cgal_assign_ring(resData[i], translated_polygon);

					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::geos)
			{
				const auto& dmsPoint = pointData[i];
				auto geosPoint = geos_factory()->createPoint(geos::geom::Coordinate(dmsPoint.X(), dmsPoint.Y()));
				auto bufferGeometry = geosPoint->buffer(bufferDistance, (pointsPerCircle + 3) / 4);

				geos_assign_geometry(resData[i], bufferGeometry.get());

				// move to nextPoint
				if (++i == n)
					return;
			}
		}
	}
};

template <typename P, geometry_library GL>
struct BufferMultiPointOperator : public AbstrBufferOperator
{
	using PointType = P;
	using CoordType = scalar_of_t<PointType>;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	BufferMultiPointOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, ValueComposition::MultiPoint, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t, Timer& processTimer, CharPtr itemRef = "") const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);

		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		// multipoint is the intended composition (CheckGeometryArgComposition warns otherwise), but arc/polygon
		// input is still processed (each coordinate buffered) rather than aborting debug builds - this keeps configs
		// that predate multipoint composition working without change (#1038 backward compatibility).
		assert(IsAcceptableValuesComposition(polyItem->GetValueComposition()));

		SizeT i = 0, n = polyData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];

			if constexpr (GL == geometry_library::boost_geometry)
			{
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;

				boost::geometry::model::multi_point<DPoint> currGeometry;
				bg_multi_polygon_t resMP;

				do {
					resMP.clear();
					currGeometry.assign(begin_ptr(polyData[i]), end_ptr(polyData[i]));

					auto p = MaxValue<DPoint>();
					MakeLowerBound(p, currGeometry);
					move(currGeometry, -p);

					boost::geometry::buffer(currGeometry, resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
					move(resMP, p);

					bg_store_multi_polygon(resData[i], resMP);

					// move to next geometry
							// move to next geometry
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr(GL == geometry_library::boost_polygon)
			{
				typename bp::polygon_set_data<CoordType>::clean_resources cleanResources;
				typename bp_union_poly_traits<CoordType>::polygon_set_data_type resMP = {};
				auto resRing = bp_circle<CoordType>(bufferDistance, pointsPerCircle);

				// polygon_set_data::insert takes the ring's orientation from polygon_traits, and the
				// point_sequence_traits specialisation for std::vector<bp::point_data<>> declares
				// clockwise_winding unconditionally (rtc/dll/src/geo/BoostPolygon.h). bp_circle emits
				// counterclockwise, so inserting it as-is signs every edge the wrong way: the set then
				// describes the complement of the circles and cleans to nothing -- the empty result of
				// issue #1172. bp_buffer_point does not hit this because it hands the ring straight to
				// bp_assign_ring without a polygon set, so the circle itself must keep its orientation.
				std::reverse(resRing.begin(), resRing.end());

				do {
					resMP.clear();
					for (const auto& dmsPoint : polyData[i])
					{
						auto movedRing = resRing;
						move<CoordType>(movedRing, dmsPoint);
						resMP.insert(movedRing, false);
					}

					bp_assign(resData[i], resMP, cleanResources);
					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);

			}
			else if constexpr (GL == geometry_library::cgal)
			{
				auto cgalCircle = cgal_circle<CoordType>(bufferDistance, pointsPerCircle);
				do {
					CGAL_Traits::Polygon_set result;
					for (const auto& p : polyData[i])
					{
						// Define the translation vector (dx, dy)
						CGAL_Traits::Kernel::Vector_2 translation_vector(p.X(), p.Y());

						// Define the affine transformation for translation
						CGAL::Aff_transformation_2<CGAL_Traits::Kernel> translate(CGAL::TRANSLATION, translation_vector);

						// Create a new polygon for the translated version
						CGAL::Polygon_2<CGAL_Traits::Kernel> translated_polygon;

						// Apply the translation to each vertex of the original polygon
						for (auto vertex : cgalCircle)
							translated_polygon.push_back(translate.transform(vertex));
						result.join( translated_polygon );
					}
					// Store the translated polygon
					cgal_assign_polygon_set(resData[i], result);

					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::geos)
			{
				std::unique_ptr<geos::geom::Geometry> geosResult;
				for (const auto& p : polyData[i])
				{
					auto point = geos_factory()->createPoint(geos::geom::Coordinate(p.X(), p.Y()));
					auto bufferGeometry = point->buffer(bufferDistance, (pointsPerCircle + 3) / 4);

					if (!geosResult)
						geosResult = std::move(bufferGeometry);
					else
						geosResult = geosResult->Union(bufferGeometry.get());
				}

				geos_assign_geometry(resData[i], geosResult.get());

				// move to next geometry
				if (++i == n)
					return;
			}
			else
			{
				static_assert(unsupported_geometry_library_v<GL>, "Unsupported geometry library for BufferMultiPointOperator");
			}
		}
	}
};

template <typename CoordType>
void bp_bufferLineString(bp::polygon_set_data<CoordType>& result, const std::vector<bp::point_data< CoordType>>& lineString, const std::vector<bp::point_data< CoordType>>& circle)
{
	using Point = bp::point_data<CoordType>;
	using bp_convolution = boost::polygon::detail::template minkowski_offset<CoordType>;
	bp_convolution::convolve_two_point_sequences(result, lineString.begin(), lineString.end(), circle.begin(), circle.end(), false);
}


template <typename P, geometry_library GL>
struct BufferLineStringOperator : public AbstrBufferOperator
{
	using PointType = P;
	using CoordType = scalar_of_t<PointType>;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	BufferLineStringOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, ValueComposition::Sequence, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* lineStringItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t, Timer& processTimer, CharPtr itemRef = "") const override
	{
		assert(lineStringItem->GetValueComposition() == ValueComposition::Sequence);

		auto lineStringData = const_array_cast<PolygonType>(lineStringItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);

		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(lineStringData.size() == resData.size());

		SizeT i = 0, n = lineStringData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];
			if constexpr (GL == geometry_library::boost_geometry)
			{
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;


				bg_linestring_t helperLineString;
				bg_multi_linestring_t currGeometry;
				bg_multi_polygon_t resMP;

				do {
					resMP.clear();
					bg_load_multi_linestring(currGeometry, lineStringData[i], helperLineString);

					if (!currGeometry.empty())
					{
						auto p = MaxValue<DPoint>();
						MakeLowerBound(p, currGeometry);
						move(currGeometry, -p);

						boost::geometry::buffer(currGeometry, resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
						move(resMP, p);

					}
					bg_store_multi_polygon(resData[i], resMP);

					// move to next geometry
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::geos)
			{
				auto lineStringRef = lineStringData[i];
				auto lineString = geos_create_multi_linestring<P>(lineStringRef.begin(), lineStringRef.end());
				auto bufferedLineString = lineString->buffer(bufferDistance, (pointsPerCircle + 3) / 4);
				geos_assign_geometry(resData[i], bufferedLineString.get());
				// move to next geometry
				if (++i == n)
					return;
			}
			else if constexpr (GL == geometry_library::cgal)
			{
				auto cgalCircle = cgal_circle<CoordType>(bufferDistance, pointsPerCircle);

				CGAL_Traits::Polygon_set resPS;
				std::vector<CGAL_Traits::Ring> capsules;
				std::vector<CGAL_Traits::Point> helperPoints;

				do {
					cgal_buffer_multi_linestring<P>(resPS, lineStringData[i], cgalCircle, capsules, helperPoints);
					cgal_assign_polygon_set(resData[i], resPS);

					// move to next geometry
					if (++i == n)
						return;

				} while (e2IsVoid && e3IsVoid); // pointsPerCircle varies per element too: reload the disc
			}
			else if constexpr (GL == geometry_library::boost_polygon)
			{
				auto circle = bp_circle<CoordType>(bufferDistance, pointsPerCircle);

				using traits_t = bp_union_poly_traits<CoordType>;
				using bp_linestring = typename traits_t::ring_type;
				bp_linestring helperLineString = {};
				typename bp::polygon_set_data<CoordType>::clean_resources cleanResources;
				std::vector<bp_linestring> lineStrings;

				do {
					lineStrings.clear();
					bp_load_multi_linestring<P>(lineStrings, lineStringData[i], helperLineString);

					bp::polygon_set_data<CoordType> resMP;
					for (const auto& ls : lineStrings)
					{
						bp_bufferLineString(resMP, ls, circle);
					}

					bp_assign(resData[i], resMP, cleanResources);

					// move to next geometry
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else
			{
				static_assert(unsupported_geometry_library_v<GL>, "Unsupported geometry library for BufferLineStringOperator");
			}
		}
	}
};

template <typename P, geometry_library GL>
struct BufferMultiPolygonOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	BufferMultiPolygonOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, ValueComposition::Polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t, Timer& processTimer, CharPtr itemRef = "") const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		SizeT i = 0, n = polyData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];
			if constexpr(GL == geometry_library::boost_geometry)
			{
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;

				bg_ring_t helperRing;

				bg_polygon_t helperPolygon;
				bg_multi_polygon_t currMP, resMP;

				bool takeSmallLoop = e2IsVoid && e3IsVoid;
				do
				{
					assign_multi_polygon(currMP, polyData[i], true, helperPolygon, helperRing);
					if (!currMP.empty())
					{
						auto lb = MaxValue<DPoint>();
						MakeLowerBound(lb, currMP);
						move(currMP, -lb);

						boost::geometry::buffer(currMP, resMP
							, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
						move(resMP, lb);

						bg_store_multi_polygon(resData[i], resMP);
					}

					if (processTimer.PassedSecs())
					{
						reportF(SeverityTypeID::ST_MajorTrace, "{}{}: processed {} / {} sequences of tile {} / {}"
							, itemRef
							, GetGroup()->GetNameStr()
							, AsString(i), AsString(n)
							, AsString(t), AsString(resItem->GetTiledRangeData()->GetNrTiles())
						);
					}
					++i;
				} while (i != n && takeSmallLoop);
				if (i == n)
					break;
			}
			else if constexpr (GL == geometry_library::geos)
			{
				auto mp = geos_create_polygons(polyData[i]);
				if (mp && !mp->isEmpty())
				{
					auto resMP = mp->buffer(bufferDistance, pointsPerCircle);
					geos_assign_geometry(resData[i], resMP.get());
				}
				if (processTimer.PassedSecs())
				{
					reportF(SeverityTypeID::ST_MajorTrace, "{}{}: processed {} / {} sequences of tile {} / {}"
						, itemRef
						, GetGroup()->GetNameStr()
						, AsString(i), AsString(n)
						, AsString(t), AsString(resItem->GetTiledRangeData()->GetNrTiles())
					);
				}
				if (++i == n)
					break;
			}
		}
	}
};

template <typename P>
struct BufferSinglePolygonOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	BufferSinglePolygonOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, ValueComposition::Polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t, Timer& processTimer, CharPtr itemRef = "") const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		SizeT i = 0, n = polyData.size(); if (!n) return;

		while (true)
		{
			try {
				if (!e2IsVoid)
					bufferDistance = bufDistData[i];
				if (!e3IsVoid)
					pointsPerCircle = ppcData[i];
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;

				boost::geometry::model::ring<DPoint> helperRing;

				using bg_polygon_t = boost::geometry::model::polygon<DPoint>;
				bg_polygon_t currPoly;
				boost::geometry::model::multi_polygon<bg_polygon_t> resMP;

			nextPointWithSameResRing:

				assign_polygon(currPoly, polyData[i], true, helperRing);
				if (!currPoly.outer().empty())
				{

					auto lb = MaxValue<DPoint>();
					MakeLowerBound(lb, currPoly);
					move(currPoly, -lb);

					boost::geometry::buffer(currPoly, resMP
						, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
					move(resMP, lb);

					bg_store_multi_polygon(resData[i], resMP);
				}

				// move to nextPoint
				if (++i == n)
					break;
				if (e2IsVoid && e3IsVoid)
					goto nextPointWithSameResRing;
			}
			catch (DmsException& e)
			{
				e.AsErrMsg()->TellExtraF("BufferSinglePolygonOperator::Calculate tile {}, offset {}", t, i);
				throw;
			}
		}
	}
};

// *****************************************************************************
//	geos_buffer: dispatch on the ValueComposition of the argument
// *****************************************************************************

// A single geos_buffer operator that picks the geos_buffer_multi_polygon,
// geos_buffer_linestring or geos_buffer_multi_point behaviour at run-time, based on the
// ValueComposition of its first argument (issue #1038). The result is always a polygon.
template <typename P>
struct GeosBufferOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	// ValueComposition::Unknown: accept any geometry composition (no deprecation nudge), dispatch in Calculate
	GeosBufferOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, ValueComposition::Unknown, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t, Timer& processTimer, CharPtr itemRef = "") const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData     = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		ValueComposition vc = polyItem->GetValueComposition();
		if (vc != ValueComposition::Polygon && vc != ValueComposition::Sequence && vc != ValueComposition::MultiPoint)
			GetGroup()->throwOperErrorF(
				"geos_buffer: unsupported ValueComposition '{}' of the first argument; expected polygon, arc or multipoint geometry"
				, GetValueCompositionID(vc));

		SizeT i = 0, n = polyData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];

			switch (vc)
			{
			case ValueComposition::Polygon: // as geos_buffer_multi_polygon
			{
				auto mp = geos_create_polygons(polyData[i]);
				if (mp && !mp->isEmpty())
				{
					auto resMP = mp->buffer(bufferDistance, pointsPerCircle);
					geos_assign_geometry(resData[i], resMP.get());
				}
				break;
			}
			case ValueComposition::Sequence: // as geos_buffer_linestring
			{
				auto lineStringRef = polyData[i];
				auto lineString = geos_create_multi_linestring<P>(lineStringRef.begin(), lineStringRef.end());
				auto bufferedLineString = lineString->buffer(bufferDistance, (pointsPerCircle + 3) / 4);
				geos_assign_geometry(resData[i], bufferedLineString.get());
				break;
			}
			case ValueComposition::MultiPoint: // as geos_buffer_multi_point
			{
				std::unique_ptr<geos::geom::Geometry> geosResult;
				for (const auto& p : polyData[i])
				{
					auto point = geos_factory()->createPoint(geos::geom::Coordinate(p.X(), p.Y()));
					auto bufferGeometry = point->buffer(bufferDistance, (pointsPerCircle + 3) / 4);
					if (!geosResult)
						geosResult = std::move(bufferGeometry);
					else
						geosResult = geosResult->Union(bufferGeometry.get());
				}
				geos_assign_geometry(resData[i], geosResult.get());
				break;
			}
			default:
				break; // unreachable: checked above
			}

			if (processTimer.PassedSecs())
			{
				reportF(SeverityTypeID::ST_MajorTrace, "{}{}: processed {} / {} sequences of tile {} / {}"
					, itemRef
					, GetGroup()->GetNameStr()
					, AsString(i), AsString(n)
					, AsString(t), AsString(resObj->GetTiledRangeData()->GetNrTiles())
				);
			}

			if (++i == n)
				return;
		}
	}
};

// *****************************************************************************
//	unary polygon -> polygon base
// *****************************************************************************

// Shared skeleton for the unary polygon operators: same domain and values unit as the argument,
// ValueComposition::Polygon in and out, one tile at a time. Subclasses only implement Calculate.
class AbstrUnaryPolygonOperator : public UnaryOperator
{
protected:
	AbstrUnaryPolygonOperator(AbstrOperGroup& og, const DataItemClass* polyAttrClass)
		: UnaryOperator(&og, polyAttrClass, polyAttrClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		dms_assert(arg1A);

		CheckGeometryArgComposition(GetGroup(), arg1A, ValueComposition::Polygon);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, values1Unit, ValueComposition::Polygon);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(domain1Unit->GetNrTiles(), [this, resObj = resLock.get(), arg1A](tile_id t)->void
				{
					ReadableTileLock readPoly1Lock(arg1A->GetCurrRefObj().get(), t);

					Calculate(resObj, arg1A, t);
				}
			);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, tile_id t) const = 0;
};

template <typename P>
struct OuterMultiPolygonOperator : public AbstrUnaryPolygonOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	OuterMultiPolygonOperator(AbstrOperGroup& gr)
		: AbstrUnaryPolygonOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		bg_ring_t currRing;

		using bg_polygon_t = boost::geometry::model::polygon<DPoint>;
		bg_polygon_t currPoly;
		bg_multi_polygon_t currMP;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			assign_multi_polygon(currMP, polyData[i], false, currPoly, currRing);

			if (!currMP.empty())
				bg_store_multi_polygon(resData[i], currMP);
		}
	}
};

template <typename P>
struct OuterSinglePolygonOperator : public AbstrUnaryPolygonOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	OuterSinglePolygonOperator(AbstrOperGroup& gr)
		: AbstrUnaryPolygonOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		bg_ring_t helperRing;

		bg_polygon_t  currPoly;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			try {
				assign_polygon(currPoly, polyData[i], false, helperRing);

				if (!currPoly.outer().empty())
				{
					resData[i].reserve(currPoly.outer().size() MG_DEBUG_ALLOCATOR_SRC("OuterSinglePolygon"));
					bg_store_ring(resData[i], currPoly.outer());
				}
			}
			catch (DmsException& e)
			{
				e.AsErrMsg()->TellExtraF("OuterSinglePolygonOperator::Calculate tile {}, offset {}", t, i);
				throw;
			}

		}
	}
};

// *****************************************************************************
//	reverse_polygon
// *****************************************************************************

// Reverses the winding order of every ring, leaving the sequence layout alone.
//
// Reversing a polygon value as a whole - what the sequence2points / reverse / points2sequence
// workaround in issue #302 does - destroys anything with holes or parts. Rings are self-delimiting
// (a ring ends where its own first point repeats) and the parts of a multi-polygon are strung
// together with backtrack points, so after a whole-sequence reverse fillPointIndexBuffer reads one
// bogus ring spanning the backtrack head plus every hole.
//
// Reversing each ring in place is exact instead: a closed ring p0 p1 .. pk p0 reverses to
// p0 pk .. p1 p0, so its first and last point are unchanged, and therefore so is every ring
// delimiter and every backtrack point (each of which is by construction some ring's first point).
// The points between the rings - the backtracks - are copied through untouched.
template <typename P>
struct ReversePolygonOperator : public AbstrUnaryPolygonOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	ReversePolygonOperator(AbstrOperGroup& gr)
		: AbstrUnaryPolygonOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		index_range_vector_t ringRanges;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			auto polyRef = polyData[i];
			auto resRef = resData[i];

			if (!polyRef.IsDefined())
			{
				resRef.assign(Undefined());
				continue;
			}
			if (polyRef.empty())
				continue;

			resRef.assign(polyRef.begin(), polyRef.end() MG_DEBUG_ALLOCATOR_SRC("ReversePolygon"));

			ringRanges.clear();
			fillPointIndexBuffer(ringRanges, polyRef.begin(), polyRef.end());

			for (const auto& ringRange : ringRanges)
				std::reverse(resRef.begin() + ringRange.first, resRef.begin() + ringRange.second);
		}
	}
};

// *****************************************************************************
//	fix_winding_order / fix_polygon / has_correct_winding  (issue #302)
// *****************************************************************************

// All three run geos_polygons_by_nesting (GEOS_Traits.h), which derives the shell/hole roles from
// geometric nesting rather than from ring orientation, and then applies the clockwise-shell /
// counter-clockwise-hole convention with Polygon::orientRings. They differ only in what they do
// with the verdict it returns.

// fix_winding_order: reorder and reorient the rings, never move a vertex.
//
// Vertices are carried over from the input unchanged - the only points that disappear are the
// repeats and spikes that geos_create_linear_ring drops, the same normalisation every geometry
// reader in GeoDMS already applies. Because GEOS is used for the ANALYSIS only, this is exact for
// integer coordinates as well, so the operator is registered for every point type.
//
// It does not repair self-intersections: for a bow-tie the signed area is not an orientation
// indicator at all (the lobes cancel, and a symmetric bow-tie has area exactly 0), and repairing
// one means moving coordinates. Those features are passed through with a warning naming
// fix_polygon.
template <typename P>
struct FixWindingOrderOperator : public AbstrUnaryPolygonOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	FixWindingOrderOperator(AbstrOperGroup& gr)
		: AbstrUnaryPolygonOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		geos_create_linear_ring_helper_data<PointType> tmpRingData;
		SizeT nrStillInvalid = 0;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			auto polyRef = polyData[i];
			auto resRef = resData[i];

			if (!polyRef.IsDefined())
			{
				resRef.assign(Undefined());
				continue;
			}
			if (polyRef.empty())
				continue;

			auto nesting = geos_polygons_by_nesting(polyRef, tmpRingData);
			if (!nesting.geometry)
				continue;

			if (!nesting.isValid)
				++nrStillInvalid;

			geos_assign_geometry(resRef, nesting.geometry.get());
		}

		if (nrStillInvalid)
			reportF(SeverityTypeID::ST_Warning
				, "{}: {} of {} geometries in tile {} are still invalid after the rings were reordered. "
				  "fix_winding_order never moves a vertex, so a self-intersecting ring survives it; use fix_polygon to repair those."
				, GetGroup()->GetNameStr()
				, nrStillInvalid, polyData.size(), t);
	}
};

// fix_polygon: fix_winding_order, and then GEOS MakeValid for whatever is still invalid.
//
// A single operator rather than geos_polygon(fix_winding_order(g)) in configuration, for two
// reasons. The nesting pass already knows which features are invalid, so MakeValid runs only on
// those instead of on every row. And it cleans an in-memory geometry that has already been given
// the right ring roles, so it never re-reads the point sequence through geos_create_polygons.
//
// This one does move coordinates - that is the point - so it is restricted to dpoint like the rest
// of the geos family: MakeValid introduces intersection points that an integer coordinate grid
// cannot represent, and truncating them back could produce new invalid geometry.
template <typename P>
struct FixPolygonOperator : public AbstrUnaryPolygonOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	FixPolygonOperator(AbstrOperGroup& gr)
		: AbstrUnaryPolygonOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		geos_create_linear_ring_helper_data<PointType> tmpRingData;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			auto polyRef = polyData[i];
			auto resRef = resData[i];

			if (!polyRef.IsDefined())
			{
				resRef.assign(Undefined());
				continue;
			}
			if (polyRef.empty())
				continue;

			auto nesting = geos_polygons_by_nesting(polyRef, tmpRingData);
			if (!nesting.geometry)
				continue;

			if (nesting.isValid)
			{
				geos_assign_geometry(resRef, nesting.geometry.get());
				continue;
			}

			// clean_geos_geometry reports what was wrong and what survived, so the repair is visible
			auto cleaned = clean_geos_geometry(nesting.geometry.get());
			geos_assign_geometry(resRef, cleaned.get());
		}
	}
};

// *****************************************************************************
//	unary polygon -> bool base
// *****************************************************************************

class AbstrPolygonPredicateOperator : public UnaryOperator
{
protected:
	AbstrPolygonPredicateOperator(AbstrOperGroup& og, const DataItemClass* polyAttrClass)
		: UnaryOperator(&og, DataArray<Bool>::GetStaticClass(), polyAttrClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		assert(arg1A);

		CheckGeometryArgComposition(GetGroup(), arg1A, ValueComposition::Polygon);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, Unit<Bool>::GetStaticClass()->CreateDefault());

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_all);

			parallel_tileloop(domain1Unit->GetNrTiles(), [this, resObj = resLock.get(), arg1A](tile_id t)->void
				{
					ReadableTileLock readPoly1Lock(arg1A->GetCurrRefObj().get(), t);

					Calculate(resObj, arg1A, t);
				}
			);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, tile_id t) const = 0;
};

// has_correct_winding: the selection predicate that area(g) < 0 was standing in for.
//
// True only when the feature is certifiably clean: every ring's orientation agrees with its
// nesting parity, no ring collapsed, and IsValidOp accepts the result. Any doubt gives False, so
// select_with_org_rel(!has_correct_winding(g)) never under-selects. Unlike area(g) < 0 it catches
// the half-flipped feature, whose total area is too LARGE rather than negative.
//
// An undefined or empty geometry has no rings and therefore no winding to get wrong: it answers
// True, so selecting on the negation does not drag in every null row.
template <typename P>
struct HasCorrectWindingOperator : public AbstrPolygonPredicateOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using Arg1Type = DataArray<PolygonType>;

	HasCorrectWindingOperator(AbstrOperGroup& gr)
		: AbstrPolygonPredicateOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<Bool>(resObj)->GetWritableTile(t, dms_rw_mode::write_only_all);
		assert(polyData.size() == resData.size());

		geos_create_linear_ring_helper_data<PointType> tmpRingData;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			auto polyRef = polyData[i];

			if (!polyRef.IsDefined() || polyRef.empty())
			{
				resData[i] = true;
				continue;
			}
			resData[i] = geos_polygons_by_nesting(polyRef, tmpRingData).IsClean();
		}
	}
};

#endif // __GEO_BOOSTGEOMETRYIMPL_H
