// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_GEO_CGAL_TRAITS_H)
#define DMS_GEO_CGAL_TRAITS_H

#include "geo/RingIterator.h"


#include "RemoveAdjacentsAndSpikes.h"

//============================  CGAL  ============================

#if defined(MG_DEBUG)
// #define CGAL_SS_VERBOSE
#endif

#include <CGAL/Polygon_set_2.h>
#include <CGAL/intersections.h>
//#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/partition_2.h>
#include <CGAL/Partition_traits_2.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arrangement_2.h>

#include "CGAL_60/Polygon_repair/repair.h"

struct CGAL_Traits
{
//	using Kernel = CGAL::Exact_predicates_exact_constructions_kernel;
	using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel; 
//  fails on simple intersections, such as identifying a crossing point 
// 112894,685301784 403381,743493082
// 
// 	on segment
	// 113032 403298
	// 112809 403434
	// which has outproduct -8,90577E-08, corresponding with triangular heights of 3.40957E-10, 8.87357E-10, and 5.53717E-10, which misses the crossing point only by 1 to 2^50.

	using Point = CGAL::Point_2< Kernel >;
	using Segment = Kernel::Segment_2;

	using Ring = CGAL::Polygon_2<Kernel>;
	using Polygon_with_holes = CGAL::Polygon_with_holes_2<Kernel>;
	using Polygon_set = CGAL::Polygon_set_2<Kernel>;
};

template <typename DmsPointType>
void append_point(CGAL_Traits::Ring& ring, DmsPointType p)
{
	ring.push_back(CGAL_Traits::Point(p.X(), p.Y()));
}

template <typename DmsPointType>
void assign_multi_polygon(CGAL_Traits::Polygon_set& resMP, SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings
	, CGAL_Traits::Polygon_with_holes&& helperPolygon = CGAL_Traits::Polygon_with_holes()
	, CGAL_Traits::Ring&& helperRing = CGAL_Traits::Ring()
)
{
	resMP.clear();
	std::vector<CGAL_Traits::Ring> foundHoles;

	SA_ConstRingIterator<DmsPointType> rb(polyRef, 0), re(polyRef, -1);
	auto ri = rb;

	if (ri == re)
		return;

	std::vector<DmsPointType> ringPoints;
	for (; ri != re; ++ri)
	{
		auto pb = (*ri).begin(), pe = (*ri).end();
		if (pb == pe)
			continue;

		if (pb[0] == pe[-1]) // closed ?
			--pe; // remove closing point

		helperRing.clear();
		ringPoints.clear();

		ringPoints.reserve(pe - pb);
		for (auto p = pe; p != pb; ) // reverse order
			ringPoints.push_back(*--p);

		remove_adjacents_and_spikes(ringPoints);

		for (const auto& rp : ringPoints)
			append_point(helperRing, rp);
 
		if (helperRing.is_empty())
			continue;

		if (helperRing.is_simple())
		{
			if (helperRing.orientation() == CGAL::COUNTERCLOCKWISE)
				resMP.join(helperRing);
			else
				foundHoles.emplace_back(std::move(helperRing));
		}
		else
		{
			// backtrack and do all at once with repair polygons
			resMP.clear();
			helperRing.clear();
			ringPoints.clear();

			ringPoints.reserve((polyRef.size()));
			for (auto ppr = polyRef.end(), pprb = polyRef.begin(); ppr != pprb; ) // reverse order
				ringPoints.push_back(*--ppr);

			remove_adjacents_and_spikes(ringPoints);

			for (const auto& rp : ringPoints)
				append_point(helperRing, rp);

			if (helperRing.is_empty())
				continue;

			// repair the polygon
			auto resPolygonWithHolesContainer = CGAL::Polygon_repair::repair(helperRing);
			for (const auto& resPolygonWithHoles : resPolygonWithHolesContainer)
				resMP.insert(resPolygonWithHoles);
			foundHoles.clear();
			break;
		}
	}
	// remove holes now
	for (const auto& hole : foundHoles)
		resMP.difference(hole);
}


template <dms_sequence E>
void cgal_assign_point(E&& ref, const CGAL_Traits::Point& p)
{
	using coordinate_type = scalar_of_t<std::remove_reference_t<E>>;
	ref.push_back( shp2dms_order<coordinate_type>(CGAL::to_double( p.x() ), CGAL::to_double( p.y())) );
}

template <dms_sequence E>
void cgal_assign_ring(E&& ref, const CGAL_Traits::Ring& polyData)
{
	using coordinate_type = scalar_of_t<std::remove_reference_t<E>>;
	assert(polyData.size() > 0);

	auto pb = polyData.vertices_begin(), pe = polyData.vertices_end();
//*
	// reassign points in reverse order to restore clockwise order
	for (auto pri=pe; pri!=pb;)
		cgal_assign_point(ref, *--pri);
	cgal_assign_point(ref, *--pe); // add last ring point that became the first GeoDms ring point as closing point
//	*/

/*
	for (auto pi = pb; pi != pe; ++pi)
		cgal_assign_point(ref, *pi);
	cgal_assign_point(ref, *pb); // add first ring point as closing point
//	*/
}

template <dms_sequence E>
void cgal_assign(E&& ref, std::vector<CGAL_Traits::Polygon_with_holes>&& polyVec)
{
	using coordinate_type = scalar_of_t<std::remove_reference_t<E>>;

	std::vector<CGAL_Traits::Point> closurePoints;

	for (const auto& poly : polyVec)
	{
		//		E::value_type polyVec;
		cgal_assign_ring(std::forward<E>(ref), poly.outer_boundary());
		closurePoints.emplace_back(poly.outer_boundary().begin()[0]);
		for (const auto& hole : poly.holes())
		{
			cgal_assign_ring(std::forward<E>(ref), hole);
			closurePoints.emplace_back(hole.begin()[0]);
		}
	}
	if (closurePoints.size() > 1)
	{
		closurePoints.pop_back();
		while (closurePoints.size())
		{
			cgal_assign_point(ref, closurePoints.back());
			closurePoints.pop_back();
		}
	}
}

template <dms_sequence E>
void cgal_assign(E&& ref, const CGAL_Traits::Polygon_set& polyData)
{
	std::vector<CGAL_Traits::Polygon_with_holes> polyVec;

	polyData.polygons_with_holes(std::back_inserter(polyVec));
	cgal_assign(std::forward<E>(ref), std::move(polyVec));
}



#endif //!defined(DMS_GEO_CGAL_TRAITS_H)
