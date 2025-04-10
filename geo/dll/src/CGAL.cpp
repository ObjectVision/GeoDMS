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
			if (cgal_ring_to_bg(outputRing, inner))
				continue;

			outputPolygonWithHoles.inners().emplace_back(outputRing);
		}
		output.emplace_back(outputPolygonWithHoles);
	}

	return output;
}

#include "GEOS_Traits.h"

