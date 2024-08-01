// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(MG_GEO_BOOST_GEOMETRY_H)
#define MG_GEO_BOOST_GEOMETRY_H

#include "RtcTypeLists.h"
#include "RtcGeneratedVersion.h"
#include "VersionComponent.h"
#include "ser/SequenceArrayStream.h"

#include "dbg/Check.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/TypeListOper.h"
#include "xct/DmsException.h"

#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"

#include "OperAttrBin.h"
#include "RemoveAdjacentsAndSpikes.h"

#include <boost/geometry.hpp>
#include <boost/geometry/algorithms/within.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>

#include "ipolygon/polygon.hpp"
#include "geo/BoostPolygon.h"

//============================  CGAL  ============================

// #define CGAL_SS_VERBOSE

#include <CGAL/Polygon_set_2.h>
#include <CGAL/intersections.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
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
	using Point = CGAL::Point_2< Kernel >;
	using Segment = Kernel::Segment_2;

	using Ring = CGAL::Polygon_2<Kernel>;
	using Polygon_with_holes = CGAL::Polygon_with_holes_2<Kernel>;
	using Polygon_set = CGAL::Polygon_set_2<Kernel>;

//	using ArrTraits = CGAL::Arr_segment_traits_2<Kernel>;
//	using Arrangement = CGAL::Arrangement_2<ArrTraits>;
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

	boost::polygon::SA_ConstRingIterator<DmsPointType>
		rb(polyRef, 0),
		re(polyRef, -1);
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
			for (auto p = polyRef.end(), pb = polyRef.begin(); p != pb; ) // reverse order
				ringPoints.push_back(*--p);

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

//============================  boost geometry ============================



using bg_multi_point_t = boost::geometry::model::multi_point<DPoint>;
using bg_linestring_t = boost::geometry::model::linestring<DPoint>;
using bg_multi_linestring_t = boost::geometry::model::multi_linestring<bg_linestring_t>;
using bg_ring_t = boost::geometry::model::ring<DPoint>;
using bg_polygon_t = boost::geometry::model::polygon<DPoint>;
using bg_multi_polygon_t = boost::geometry::model::multi_polygon<bg_polygon_t>;

namespace boost::geometry::traits
{
	// define Point<T> as model for concept boost::geometry::point
	template <typename T> struct tag<Point<T>> { using type = point_tag; };
	template <typename T> struct dimension<Point<T>> : ::dimension_of < Point<T>> {};
	template <typename T> struct coordinate_type<Point<T>> { using type = T; };
	template <typename T> struct coordinate_system<Point<T>> { using type = boost::geometry::cs::cartesian; };

	template <typename T, std::size_t Index>
	struct access<Point<T>, Index> {
		static_assert(Index < 2, "Out of range");
		using CoordinateType = T;
		static inline CoordinateType get(Point<T> const& p) { if constexpr (Index == 0) return p.X(); else return p.Y(); }
		static inline void set(Point<T>& p, CoordinateType const& value) { if constexpr (Index == 0) p.X() = value; else p.Y() = value; }
	};

} //  namespace boost::geometry::traits

template <typename P>
struct bg_union_poly_traits
{
//	using coordinate_type = scalar_of_t<P>;
	using coordinate_type = Float64;
	using point_type = Point<coordinate_type>;
	using ring_type = std::vector<point_type>;

	using polygon_with_holes_type = boost::geometry::model::polygon<point_type>;

//	typedef gtl::point_data<coordinate_type>       point_type;
//	typedef gtl::rectangle_data<coordinate_type>   rect_type;
//	typedef std::vector< point_type >              point_seq_type;
	using multi_polygon_type  = boost::geometry::model::multi_polygon<polygon_with_holes_type>;
	using polygon_result_type = multi_polygon_type; //  std::vector<polygon_with_holes_type>;

//	using area_type = Float64;
//	using unsigned_area_type = Float64;
};



// TODO: return_lower_bound obv concepts overloaden

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::ring<P>& mp)
{
	for (const auto& p : mp)
		MakeLowerBound(lb, p);
}

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::linestring<P>& ls)
{
	for (const auto& p : ls)
		MakeLowerBound(lb, p);
}

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::multi_linestring<boost::geometry::model::linestring<P>>& mls)
{
	for (const auto& ls : mls)
		MakeLowerBound(lb, ls);
}

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::multi_point<P>& mp)
{
	for (const auto& p : mp)
		MakeLowerBound(lb, p);
}

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::polygon<P>& mp)
{
	// asume all inner rings are inside the envelope of the outer ring
	MakeLowerBound(lb, mp.outer());
}

template<typename P, typename Polygon>
void MakeLowerBound(P& lb, const boost::geometry::model::multi_polygon<Polygon>& mp)
{
	for (const auto& p : mp)
		MakeLowerBound(lb, p);
}

inline bool clean(bg_ring_t& ring)
{
	assert(ring.empty() || ring.front() == ring.back());
	remove_adjacents_and_spikes(ring);
	if (ring.size() < 3)
	{
		ring.clear();
		return false;
	}
	assert(ring.front() != ring.back());
	ring.emplace_back(ring.front());
	return true;
}

inline void move(bg_ring_t& ring, DPoint displacement)
{
	assert(ring.empty() || ring.front() == ring.back());
	for (auto& p : ring)
		boost::geometry::add_point(p, displacement);
	clean(ring);
}

inline void move(bg_linestring_t& ls, DPoint displacement)
{
	for (auto& p : ls)
		boost::geometry::add_point(p, displacement);
	if (ls.size() < 2)
		return;
	remove_adjacents(ls);
	assert(!ls.empty());
}

inline void move(bg_multi_point_t& ms, DPoint displacement)
{
	for (auto& p : ms)
		boost::geometry::add_point(p, displacement);
}

inline void move(bg_multi_linestring_t& mls, DPoint displacement)
{
	for (auto& ls : mls)
		move(ls, displacement);
}

inline void move(bg_polygon_t& polygon, DPoint displacement)
{
	move(polygon.outer(), displacement);
	if (polygon.outer().empty())
	{
		polygon.inners().clear();
		return;
	}
	for (auto& ir : polygon.inners())
		move(ir, displacement);
}

template<typename P, typename Polygon>
void move(boost::geometry::model::multi_polygon<Polygon>& mp, P displacement)
{
	for (auto& polygon : mp)
		move(polygon, displacement);
}

template <typename P>
bool empty(boost::geometry::model::ring<P>& ring)
{
	if (ring.size() > 3)
		return false;
	if (ring.size() < 3)
		return true;

	dms_assert(ring.size() == 3);
	return ring[0] == ring[2];
}

template <typename DmsPointType>
void assign_polygon(bg_polygon_t& resPoly, SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings, bg_ring_t& helperRing)
{
	resPoly.clear(); dms_assert(resPoly.outer().empty() and resPoly.inners().empty());

	boost::polygon::SA_ConstRingIterator<DmsPointType>
		rb(polyRef, 0),
		re(polyRef, -1);
	auto ri = rb;
	//			dbg_assert(ri != re);
	if (ri == re)
		return;

	bool outerOrientation = true;
	for (; ri != re; ++ri)
	{
		dms_assert((*ri).begin() != (*ri).end());
		dms_assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

		helperRing.assign((*ri).begin(), (*ri).end());
		if (!clean(helperRing))
		{
			if (ri == rb)
				break;
			else
				continue;
		}

		dms_assert(helperRing.begin() != helperRing.end());
		dms_assert(helperRing.begin()[0] == helperRing.end()[-1]); // closed ?

		bool currOrientation = (boost::geometry::area(helperRing) > 0);
		if (ri == rb)
		{
			resPoly.outer() = helperRing;
			outerOrientation = currOrientation;
		}
		else
		{
			if (outerOrientation == currOrientation)
				// don't start on 2nd polygon
				throwErrorD("assign_polygon", "second ring with same orientation detected as first ring; "
					"consider using an operation that supports multi_polygons (bg_buffer_multi_polygon or outer_multi_polygon)"
				);

			if (mustInsertInnerRings)
				resPoly.inners().emplace_back(helperRing);
		}
	}
}

template <typename DmsPointType>
void assign_multi_polygon(bg_multi_polygon_t& resMP, SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings
	, bg_polygon_t& helperPolygon
	, bg_ring_t& helperRing
)
{
	resMP.clear();
	
	boost::polygon::SA_ConstRingIterator<DmsPointType>
		rb(polyRef, 0),
		re(polyRef, -1);
	auto ri = rb;
	//			dbg_assert(ri != re);
	if (ri == re)
		return;
	bool outerOrientation = true;
	for (; ri != re; ++ri)
	{
		assert((*ri).begin() != (*ri).end());
		assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

		helperRing.assign((*ri).begin(), (*ri).end());
		if (!clean(helperRing))
		{
			continue;
		}

		assert(helperRing.begin() != helperRing.end());
		assert(helperRing.begin()[0] == helperRing.end()[-1]); // closed ?
		bool currOrientation = (boost::geometry::area(helperRing) > 0);
		if (ri == rb || currOrientation == outerOrientation)
		{
			if (ri != rb && !helperPolygon.outer().empty())
				resMP.emplace_back(std::move(helperPolygon));
			helperPolygon.clear(); assert(helperPolygon.outer().empty() && helperPolygon.inners().empty());

			helperPolygon.outer().swap(helperRing);	// swap is faster than assign
			outerOrientation = currOrientation;

			// skip outer rings that intersect with a previous outer ring if innerRings are skipped
			if (!mustInsertInnerRings)
			{ 
				SizeT polygonIndex = 0;
				while (polygonIndex < resMP.size())
				{
					auto currPolygon = resMP.begin() + polygonIndex;
					if (boost::geometry::intersects(currPolygon->outer(), helperPolygon.outer()))
					{
						if (boost::geometry::within(currPolygon->outer(), helperPolygon.outer()))
						{
							resMP.erase(currPolygon);
							continue;
						}
						if (boost::geometry::within(helperPolygon.outer(), currPolygon->outer()))
						{
							helperPolygon.clear();
							assert(helperPolygon.outer().empty() && helperPolygon.inners().empty());
							break;
						}

						if (boost::geometry::overlaps(currPolygon->outer(), helperPolygon.outer()))
							throwDmsErrF("OuterPolygon: unexpected overlap of two outer rings in %s", AsString(polyRef).c_str());

						// a combination of touching outer rings such as in an 8 shape is 
					}
					polygonIndex++;
				}
			}
		}
		else if (mustInsertInnerRings)
		{
			helperPolygon.inners().emplace_back(helperRing);
		}
	}
	if (!helperPolygon.outer().empty())
		resMP.emplace_back(helperPolygon);
}

template <typename Numeric>
auto sqr(Numeric x)
{
	return x * x;
}

template <typename P>
void load_multi_linestring(bg_multi_linestring_t& mls, SA_ConstReference<P> multiLineStringRef, bg_linestring_t& helperLineString)
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
void write_linestring(SA_Reference<P> resDataRef, const bg_linestring_t& ls)
{
	for (const auto& p : ls)
		resDataRef.emplace_back(p);
}

template <typename P>
void store_multi_linestring(SA_Reference<P> resDataRef, const bg_multi_linestring_t& mls)
{
	SizeT nrPoints = 0;
	if (!mls.empty())
	{
		for (const auto& ls : mls)
			nrPoints += ls.size();
		nrPoints += mls.size() - 1; // add space for the separators
	}

	resDataRef.clear();
	if (nrPoints == 0)
		return;

	resDataRef.reserve(nrPoints);

	assert(!mls.empty());
	write_linestring(resDataRef, mls.front());

	SizeT nrWrittenLineStrings = 1, nrLineStrings = mls.size();
	while (nrWrittenLineStrings != nrLineStrings)
	{
		resDataRef.emplace_back(Undefined()); // write a separator
		write_linestring(resDataRef, mls[nrWrittenLineStrings++]);
	}
}

template <typename DmsPointType>
void store_ring(SA_Reference<DmsPointType> resDataElem, const auto& ring)
{
	assert(ring.begin()[0] == ring.end()[-1]); // closed ?
	resDataElem.append(ring.begin(), ring.end());
}

template <typename DmsPointType>
void store_ring(std::vector<DmsPointType>& resDataElem, const auto& ring)
{
	assert(ring.begin()[0] == ring.end()[-1]); // closed ?
	resDataElem.append_range(ring);
}

void store_multi_polygon(auto&& resDataElem, const bg_multi_polygon_t& resMP, std::vector<DPoint>& ringClosurePoints)
{
	ringClosurePoints.clear();
	for (const auto& resPoly : resMP)
	{
		const auto& resRing = resPoly.outer();
		store_ring(resDataElem, resRing);
		ringClosurePoints.emplace_back(resRing.end()[-1]);

		for (const auto& resLake : resPoly.inners())
		{
			store_ring(resDataElem, resLake);
			ringClosurePoints.emplace_back(resLake.end()[-1]);
		}
	}
	if (!ringClosurePoints.empty())
	{
		ringClosurePoints.pop_back();
		while (!ringClosurePoints.empty())
		{
			resDataElem.emplace_back(ringClosurePoints.back());
			ringClosurePoints.pop_back();
		}
	}
}

template <dms_sequence E, typename BG_MP>
void bg_assign(E&& ref, BG_MP&& resMP)
{
	std::vector<DPoint> ringClosurePoints;
	store_multi_polygon(std::forward<E>(ref), std::forward<BG_MP>(resMP), ringClosurePoints);
}

template <typename RI, typename BG_MP>
auto bg_split_assign(RI resIter, const BG_MP& mp) -> RI
{
	using value_type = scalar_of_t<typename RI::value_type>;

	for (auto i = mp.begin(), e = mp.end(); i != e; ++resIter, ++i)
	{
		resIter->clear();

		auto& outerRing = i->outer();
		SizeT count = outerRing.size();
		assert(count);
		for (auto hi = i->inners().begin(), he = i->inners().end(); hi != he; ++hi)
			count += hi->size() + 1;

		resIter->reserve(count);

		assert(outerRing.begin() != outerRing.end());
		assert(outerRing.begin()[0] == outerRing.end()[-1]);

		store_ring(*resIter, outerRing);
		if (!i->inners().empty())
		{
			for (const auto& resLake : i->inners())
				store_ring(*resIter, resLake);
			auto currInner = i->inners().end(), firstInner = i->inners().begin();
			assert(currInner != firstInner);
			--currInner;
			while (currInner != firstInner) {
				--currInner;
				assert(currInner->size());
				resIter->emplace_back(currInner->end()[-1]);
			} ;

			resIter->emplace_back(outerRing.end()[-1]);
		}
		assert(resIter->size() == count);
	}
	return resIter;

}


#endif //!defined(MG_GEO_BOOST_GEOMETRY_H)
