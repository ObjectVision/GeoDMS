// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "BoostGeometry.h"
#include "CGAL_Traits.h"

namespace CGAL
{
	inline constexpr bool IsDefined(const CGAL_Traits::Point& v) { return true; }
}

// Convert Boost ring to CGAL polygon
auto boost_ring_to_cgal_polygon(const bg_ring_t& bgRing) -> CGAL_Traits::Ring
{
	CGAL_Traits::Ring cgalRing;

	for (const auto& p : bgRing)
		cgalRing.push_back(CGAL_Traits::Point(boost::geometry::get<0>(p), boost::geometry::get<1>(p)));

	// CGAL polygons should not repeat the first point for closure
	auto sz = cgalRing.size();
	if (sz > 1 && cgalRing[0] == cgalRing[sz - 1])
		cgalRing.erase(cgalRing.vertices_end() - 1);

	cgalRing.erase(remove_adjacents_and_spikes(cgalRing.vertices_begin(), cgalRing.vertices_end()), cgalRing.vertices_end());

	if (cgalRing.size() < 3)
		cgalRing.clear();

	return cgalRing;
}

bool cgal_ring_to_bg(bg_ring_t& outputRing, const CGAL_Traits::Ring& cgalRing)
{
	outputRing.clear();
	for (const auto& resP : cgalRing)
		bg_assign_cgal_point(outputRing, resP);


	outputRing.erase(remove_adjacents_and_spikes(outputRing.begin(), outputRing.end()), outputRing.end());

	if (outputRing.size() < 3)
		return false;

	outputRing.emplace_back(outputRing[0]);
	std::reverse(outputRing.begin(), outputRing.end());

	return true;
}

auto fix_bg_polygons_with_CGAL(bg_multi_polygon_t&& input) -> bg_multi_polygon_t
{
	CGAL_Traits::Polygon_set resMP;
	CGAL_Traits::Polygon_set res2, res3;
	//	std::vector<CGAL_Traits::Ring> foundHoles;

	for (const auto& bgPolygon : input)
	{
		auto repairedOuterRing = CGAL::Polygon_repair::repair(boost_ring_to_cgal_polygon(bgPolygon.outer()));

		res2.clear();
		for (const auto& resPolygonWithHoles : repairedOuterRing)
			res2.join(resPolygonWithHoles);

		// remove holes now
		for (const auto& lake : bgPolygon.inners())
		{
			auto repairedInnerRing = CGAL::Polygon_repair::repair(boost_ring_to_cgal_polygon(lake));
			for (const auto& resPolygonWithHoles : repairedInnerRing)
				res2.difference(resPolygonWithHoles);
		}
		resMP.join(std::move(res2));
	}

	bg_multi_polygon_t output;
	bg_ring_t outputRing;
	bg_polygon_t outputPolygonWithHoles;

	std::vector<CGAL_Traits::Polygon_with_holes> polyVec;
	resMP.polygons_with_holes(std::back_inserter(polyVec));

	for (const auto& resPolygonWithHoles : polyVec)
	{
		outputPolygonWithHoles.clear();
		if (!cgal_ring_to_bg(outputRing, resPolygonWithHoles.outer_boundary()))
			continue;

		outputPolygonWithHoles.outer().swap(outputRing);

		for (const auto& inner : resPolygonWithHoles.holes())
		{
			// cgal_ring_to_bg returns true on success; the test used to be inverted, which
			// dropped every converted hole and emplaced the degenerate ones instead
			if (!cgal_ring_to_bg(outputRing, inner))
				continue;

			outputPolygonWithHoles.inners().emplace_back(outputRing);
		}
		output.emplace_back(outputPolygonWithHoles);
	}

	return output;
}

#include "GEOS_Traits.h"

namespace bg = boost::geometry;
using BoostPoint = DPoint;
using BoostPolygon = bg_polygon_t;
using BoostMultiPolygon = bg_multi_polygon_t;
using GEOS_point_index = unsigned int;

using x = bg_multi_polygon_t;

// Convert Boost polygon to GEOS polygon
auto to_geos_polygon(const BoostPolygon& poly, geos_create_linear_ring_helper_data<DPoint>& tmp) -> std::unique_ptr<geos::geom::Polygon>
{
	auto shell = geos_create_linear_ring<DPoint>(begin_ptr(poly.outer()), end_ptr(poly.outer()), tmp);

	std::vector<std::unique_ptr<geos::geom::LinearRing>> holes;

	for (const auto& inner : poly.inners()) {
		holes.emplace_back(geos_create_linear_ring<DPoint>(begin_ptr(inner), end_ptr(inner), tmp));
	}

	return geos_factory()->createPolygon(std::move(shell), std::move(holes));
}

// Convert Boost multipolygon to GEOS multipolygon
auto to_geos_multipolygon(const bg_multi_polygon_t& mp, geos_create_linear_ring_helper_data<DPoint>& tmp) -> std::unique_ptr<geos::geom::MultiPolygon>
{
	std::vector<std::unique_ptr<geos::geom::Polygon>> polys;
	for (const auto& p : mp) {
		polys.emplace_back(to_geos_polygon(p, tmp));
	}
	auto r = geos_factory()->createMultiPolygon(std::move(polys));
	r->normalize();
	return r;
}

auto geos_point_to_bg(const geos::geom::Coordinate& c) -> DPoint
{
	return shp2dms_order(c.x, c.y);
}

// Rings that come back from GEOS (in particular from MakeValid) can be degenerate: collapsed
// slivers, spikes and 'there and back' rings with a zero signed area. boost::geometry rejects
// those with failure_wrong_topological_dimension (fewer than 4 distinct consecutive points) or
// failure_wrong_orientation (ring_area not STRICTLY positive for an outer / negative for an
// inner ring, see boost/geometry/algorithms/detail/is_valid/ring.hpp), and every algorithm fed
// such a geometry is undefined behaviour. So clean each ring the way the other readers do
// (assign_multi_polygon calls clean(), cgal_ring_to_bg calls remove_adjacents_and_spikes) and
// drop what remains degenerate rather than passing it on - see issue #1176.
auto geos_lr_to_bg(const geos::geom::LinearRing* lr) -> bg_ring_t
{
	assert(lr);
	const auto* coords = lr->getCoordinatesRO();
	assert(coords);
	auto s = coords->getSize();
	if (s <= 3)
		return {};

	bg_ring_t result; result.reserve(s);
	MG_CHECK(coords->getAt(0) == coords->getAt(s - 1));
	for (SizeT i = 0; i != s; ++i)
		result.emplace_back(geos_point_to_bg(coords->getAt(i)));

	if (!clean(result)) // removes adjacent duplicates and spikes; fails on < 3 distinct points
		return {};

	if (boost::geometry::area(result) == 0.0) // a collapsed ring; boost calls this 'wrong orientation'
	{
		result.clear();
		return {};
	}

	return result;
}


auto geos_polygon_with_holes_to_bg(const geos::geom::Polygon* poly) -> bg_polygon_t
{
	assert(poly);
	auto outerRing = geos_lr_to_bg(poly->getExteriorRing());
	if (outerRing.empty())
		return {};
	bg_polygon_t result;
	result.outer().swap(outerRing);

	SizeT irCount = poly->getNumInteriorRing();
	for (SizeT ir = 0; ir != irCount; ++ir)
	{
		auto innerRing = geos_lr_to_bg(poly->getInteriorRingN(ir));
		if (innerRing.empty()) // an empty inner ring would make the whole multi_polygon invalid
			continue;
		result.inners().emplace_back(std::move(innerRing));
	}

	return result;
}

auto bg_from_geos_mp(const geos::geom::MultiPolygon* mp) -> bg_multi_polygon_t
{
	assert(mp);
	if (mp->isEmpty())
		return {};

	bg_multi_polygon_t result;

	SizeT polygonCount = mp->getNumGeometries();
	assert(polygonCount != 0); // follows from poly.size()
	result.reserve(polygonCount);

	for (SizeT i = 0; i != polygonCount; ++i)
	{
		const auto* poly = debug_cast<const geos::geom::Polygon*>(mp->getGeometryN(i));
		auto resPoly = geos_polygon_with_holes_to_bg(poly);
		if (resPoly.outer().empty()) // don't let a degenerate polygon into the multi_polygon
			continue;
		result.emplace_back(std::move(resPoly));
	}
	return result;
}


auto bg_from_geos_geometry(const geos::geom::Geometry* geometry) -> bg_multi_polygon_t
{
	if (!geometry || geometry->isEmpty())
		return {};

	if (auto mp = dynamic_cast<const geos::geom::MultiPolygon*>(geometry))
		return bg_from_geos_mp(mp);

	if (auto poly = dynamic_cast<const geos::geom::Polygon*>(geometry))
	{
		bg_multi_polygon_t result;
		auto polyWithHoles = geos_polygon_with_holes_to_bg(poly);
		if (polyWithHoles.outer().empty())
			return result;
		result.emplace_back(std::move(polyWithHoles));
		return result;
	}
	if (auto gc = dynamic_cast<const geos::geom::GeometryCollection*>(geometry))
	{
		bg_multi_polygon_t result;
		for (SizeT i = 0; i != gc->getNumGeometries(); ++i)
		{
			const auto* subGeom = gc->getGeometryN(i);
			auto subResult = bg_from_geos_geometry(subGeom);
			result.reserve(result.size() + subResult.size());
			for (auto& subPoly : subResult)
				result.emplace_back(std::move(subPoly));
		}
		return result;
	}

	// separate points and linestrings are discarded here
	return {};
}


// Full wrapper: fix Boost MultiPolygon using GEOSMakeValid
auto fix_bg_polygons_with_GEOS(bg_multi_polygon_t&& input) -> bg_multi_polygon_t
{
	geos_create_linear_ring_helper_data<DPoint> tmp;
	auto raw_input = to_geos_multipolygon(input, tmp);

	auto result = raw_input->isValid()
		? bg_from_geos_mp(raw_input.get())
		: bg_from_geos_geometry(clean_geos_geometry(raw_input.get()).get());

	// GEOS validity ignores ring orientation and MakeValid output is not normalized, so restore
	// the clockwise-outer / counterclockwise-inner order that boost::geometry requires (#1176).
	fixWindingOrders(result);

	return result;
}
