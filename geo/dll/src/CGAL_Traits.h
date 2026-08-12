// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_GEO_CGAL_TRAITS_H)
#define DMS_GEO_CGAL_TRAITS_H

#include <numbers>
#include <optional>
#include "geo/RingIterator.h"


#include "RemoveAdjacentsAndSpikes.h"

//============================  CGAL  ============================

#if defined(MG_DEBUG)
// #define CGAL_SS_VERBOSE
#endif

#include <CGAL/Polygon_set_2.h>
#include <CGAL/intersections.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/partition_2.h>
#include <CGAL/Partition_traits_2.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/convex_hull_2.h>

#include "CGAL_60/Polygon_repair/repair.h"

struct CGAL_Traits
{
	using Kernel = CGAL::Exact_predicates_exact_constructions_kernel;
//	using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel; 
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
void assign_polyline(CGAL_Traits::Ring& resMP, SA_ConstReference<DmsPointType> polyRef)
{
	resMP.clear();
	auto pb = polyRef.begin(), pe = polyRef.end();
	if (pb == pe)
		return;

	for (auto p = pb; p != pe; ++p)
		append_point(resMP, *p);
}

template <typename DmsPointType>
void assign_multi_polygon(CGAL_Traits::Polygon_set& resMP, SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings
	, CGAL_Traits::Polygon_with_holes& helperPolygon, CGAL_Traits::Ring& helperRing)
{
	resMP.clear();

	SA_ConstRingIterator<DmsPointType> ri(polyRef, 0), re(polyRef, -1);

	if (ri == re)
		return;

	// Rings arrive one polygon at a time: an outer ring opens a polygon and each inner ring
	// that follows belongs to THAT polygon, exactly as the bg and geos readers assume. The
	// open polygon is kept apart from resMP until it is complete: subtracting a hole from the
	// whole of resMP - as this used to do with a collected foundHoles vector - also erases any
	// polygon that happens to lie inside that hole, such as an annex in the courtyard of a
	// merged building block (issue #1178).
	// The common case (an outer ring without holes) never builds an intermediate Polygon_set:
	// currOuter holds the open polygon until a hole forces the switch to currPS. A Polygon_set
	// allocates its traits and its arrangement, so it stays unmaterialized for the many polygons
	// that have no holes at all.
	std::optional<CGAL_Traits::Polygon_set> currPS;
	CGAL_Traits::Ring                       currOuter;
	bool hasCurrPolygon = false; // an outer ring is open, in currPS when that exists else in currOuter

	auto flushCurrPolygon = [&]
	{
		if (!hasCurrPolygon)
			return;
		if (currPS)
		{
			resMP.join(*currPS);
			currPS.reset();
		}
		else
			resMP.join(currOuter);
		hasCurrPolygon = false;
	};

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
			{
				// an outer ring closes the polygon that was open and opens a new one
				flushCurrPolygon();
				currOuter = std::move(helperRing);
				hasCurrPolygon = true;
			}
			else if (hasCurrPolygon && mustInsertInnerRings)
			{
				helperRing.reverse_orientation();
				if (!currPS)
				{
					currPS.emplace();
					currPS->join(currOuter);
				}
				currPS->difference(helperRing); // only from the polygon this hole belongs to
			}
		}
		else
		{
			// backtrack and do all at once with repair polygons
			resMP.clear();
			currPS.reset();
			hasCurrPolygon = false;
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
			return; // the repair covered the complete sequence
		}
	}
	flushCurrPolygon();
}

template <typename CoordType>
auto cgal_circle(double radius, int pointsPerCircle) -> CGAL_Traits::Ring
{
	if (pointsPerCircle < 3)
		pointsPerCircle = 3;
	auto anglePerPoint = 2.0 * std::numbers::pi_v<double> / pointsPerCircle;
	//	auto points = create_circle_points<CoordType>(radius, pointsPerCircle);

	CGAL_Traits::Ring points;
	points.resize(pointsPerCircle);
	for (int i = 0; i < pointsPerCircle; ++i) {

		auto angle = i * anglePerPoint;
		auto x = radius * std::cos(angle);
		auto y = radius * std::sin(angle);
		points.set(points.begin()+i, CGAL_Traits::Point(x, y));
	}
	return points;
}



template <dms_sequence E, typename K>
void cgal_assign_point(E&& ref, const CGAL::Point_2<K>& p)
{
	using coordinate_type = scalar_of_t<std::remove_reference_t<E>>;
	ref.push_back( shp2dms_order<coordinate_type>(CGAL::to_double( p.x() ), CGAL::to_double( p.y())) MG_DEBUG_ALLOCATOR_SRC("cgal_assign_point"));
}

template <dms_sequence E, typename K>
void cgal_assign_ring(E&& ref, const CGAL::Polygon_2<K>& polyData)
{
	using coordinate_type = scalar_of_t<std::remove_reference_t<E>>;
	assert(polyData.size() > 0);

	auto pb = polyData.vertices_begin(), pe = polyData.vertices_end();

	// reassign points in reverse order to restore clockwise order
	cgal_assign_point(std::forward<E>(ref), *pb); // add first ring point that will become also the last GeoDms ring point as closing point
	for (auto pri=pe; pri!=pb;)
		cgal_assign_point(ref, *--pri);
}

template <dms_sequence E>
void cgal_assign_polygon_with_holes(E&& ref, const CGAL_Traits::Polygon_with_holes& poly)
{
	using coordinate_type = scalar_of_t<std::remove_reference_t<E>>;

	cgal_assign_ring(std::forward<E>(ref), poly.outer_boundary());

	auto nh = poly.holes().size();
	if (!nh)
		return;
	for (SizeT i = 0; i != nh; ++i)
		cgal_assign_ring(std::forward<E>(ref), poly.holes()[i]);
	--nh;
	while (nh)
	{
		--nh;
		cgal_assign_point(std::forward<E>(ref), poly.holes()[nh].vertices_begin()[0]);
	}
	cgal_assign_point(std::forward<E>(ref), poly.outer_boundary().vertices_begin()[0]);
}

template <dms_sequence E>
void cgal_assign_polygon_with_holes_vector(E&& ref, std::vector<CGAL_Traits::Polygon_with_holes>&& polyVec)
{
	ref.clear();

	auto np = polyVec.size();
	if (!np)
		return;

	SizeT count = np - 1;

	for (const auto& resPoly : polyVec)
	{
		assert(resPoly.outer_boundary().size());
		count += resPoly.outer_boundary().size() + 1;
		for (auto hi = resPoly.holes().begin(), he = resPoly.holes().end(); hi != he; ++hi)
			count += hi->size() + 2;
	}
	ref.reserve(count MG_DEBUG_ALLOCATOR_SRC("cgal_assign_polygon_with_holes_vector"));

	for (const auto& poly : polyVec)
		cgal_assign_polygon_with_holes(ref, poly);

	--np;
	while (np)
	{
		--np;
		cgal_assign_point(ref, polyVec[np].outer_boundary().vertices_begin()[0]);
	}
	assert(ref.size() == count);
}

template <dms_sequence E>
void cgal_assign_polygon_set(E&& ref, const CGAL_Traits::Polygon_set& polyData)
{
	std::vector<CGAL_Traits::Polygon_with_holes> polyVec;

	polyData.polygons_with_holes(std::back_inserter(polyVec));

	cgal_assign_polygon_with_holes_vector(std::forward<E>(ref), std::move(polyVec));
}

template <typename RI>
auto cgal_split_assign_polygon_set(RI resIter, const CGAL_Traits::Polygon_set& polyData) -> RI
{
	std::vector<CGAL_Traits::Polygon_with_holes> polyVec;
	polyData.polygons_with_holes(std::back_inserter(polyVec));
	for (const auto& poly : polyVec)
	{
		// Must match what cgal_assign_polygon_with_holes actually writes, which is what
		// cgal_assign_polygon_with_holes_vector already counts per polygon:
		//   cgal_assign_ring emits n+1 points per ring (first vertex, then all n in reverse, so the
		//   ring closes), and after the rings the hole loop emits nh-1 hole first-vertices plus one
		//   outer first-vertex -- i.e. nh extra. Total: outer+1 + sum(hole+2).
		// This used to count outer + sum(hole+1), short by nh+1 (by 1 with no holes), which both
		// under-reserved and tripped the postcondition assert below.
		assert(poly.outer_boundary().size());
		SizeT count = poly.outer_boundary().size() + 1;

		for (const auto& hole : poly.holes())
			count += hole.size() + 2;


		resIter->clear();
		resIter->reserve(count MG_DEBUG_ALLOCATOR_SRC("cgal_split_assign_polygon_set"));

		cgal_assign_polygon_with_holes(*resIter, poly);

		assert(resIter->size() == count);
		++resIter;
	}
	return resIter;
}

template <typename K>
void bg_assign_cgal_point(bg_ring_t& ref, const CGAL::Point_2<K>& p)
{
	ref.push_back(shp2dms_order<Float64>(CGAL::to_double(p.x()), CGAL::to_double(p.y())));
}


template<typename DmsPointType>
void dms_assign(CGAL_Traits::Polygon_set& lvalue, SA_ConstReference<DmsPointType> rvalue)
{
	lvalue.clear();
	CGAL_Traits::Polygon_with_holes helperPolygon;
	CGAL_Traits::Ring helperRing;

	assign_multi_polygon(lvalue, rvalue, true, helperPolygon, helperRing);
}


template<typename DmsPointType>
void dms_insert(CGAL_Traits::Polygon_set& lvalue, SA_ConstReference<DmsPointType> rvalue)
{
	CGAL_Traits::Polygon_set tmpPS;
	dms_assign(tmpPS, rvalue);

	lvalue.join(tmpPS);
}


struct cgal_intersection {
	void operator ()(const auto& a, const auto& b, auto& r) const { r.intersection(a, b); }
};

struct cgal_union {
	void operator ()(const auto& a, const auto& b, auto& r) const { r.join(a, b); }
};

struct cgal_difference {
	void operator ()(const auto& a, const auto& b, auto& r) const { r.difference(a, b); }
};

struct cgal_sym_difference {
	void operator ()(const auto& a, const auto& b, auto& r) const { r.symmetric_difference(a, b); }
};



#endif //!defined(DMS_GEO_CGAL_TRAITS_H)
