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
#include "dbg/Timer.h"
#include "geo/RingIterator.h"
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

	using multi_polygon_type  = boost::geometry::model::multi_polygon<polygon_with_holes_type>;
	using polygon_result_type = multi_polygon_type; //  std::vector<polygon_with_holes_type>;

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

template <typename P>
void write_linestring(SA_Reference<P> resDataRef, const bg_linestring_t& ls)
{
	for (const auto& p : ls)
		resDataRef.emplace_back(p);
}

template <typename P>
void bg_store_multi_linestring(SA_Reference<P> resDataRef, const bg_multi_linestring_t& mls)
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
void bg_store_ring(SA_Reference<DmsPointType> resDataElem, const auto& ring)
{
	assert(ring.begin()[0] == ring.end()[-1]); // closed ?
	resDataElem.append_range(ring);
}

template <typename DmsPointType>
void bg_store_ring(std::vector<DmsPointType>& resDataElem, const auto& ring)
{
	assert(ring.begin()[0] == ring.end()[-1]); // closed ?
	resDataElem.append_range(ring);
}

template <dms_sequence E>
void bg_store_polygon(E&& ref, const bg_polygon_t& resPoly)
{
	bg_store_ring(ref, resPoly.outer());

	for (const auto& resHole : resPoly.inners())
		bg_store_ring(ref, resHole);
	auto nh = resPoly.inners().size();
	if (!nh)
		return;
	--nh;
	while (nh)
	{
		--nh;
		ref.emplace_back(resPoly.inners()[nh].end()[-1]);
	}
	ref.emplace_back(resPoly.outer().end()[-1]);
}

template <dms_sequence E>
void bg_store_multi_polygon(E&& ref, const bg_multi_polygon_t& resMP)
{
	ref.clear();

	auto np = resMP.size();
	if (!np)
		return;

	SizeT count = np - 1;

	for (const auto& resPoly : resMP)
	{
		assert(resPoly.outer().size());
		count += resPoly.outer().size();
		for (auto hi = resPoly.inners().begin(), he = resPoly.inners().end(); hi != he; ++hi)
			count += hi->size() + 1;
	}

	ref.reserve(count);

	for (const auto& resPoly : resMP)
		bg_store_polygon(ref, resPoly);

	--np;
	while (np)
	{
		--np;
		ref.emplace_back(resMP[np].outer().end()[-1]);
	}
	assert(ref.size() == count);
}

template <dms_sequence E, typename BG_MP>
void bg_assign(E&& ref, BG_MP&& resMP)
{
	bg_store_multi_polygon(std::forward<E>(ref), std::forward<BG_MP>(resMP));
}

template <typename RI, typename BG_MP>
auto bg_split_assign(RI resIter, const BG_MP& mp) -> RI
{
	using value_type = scalar_of_t<typename RI::value_type>;

	for (auto i = mp.begin(), e = mp.end(); i != e; ++resIter, ++i)
	{
		auto& outerRing = i->outer();
		SizeT count = outerRing.size();
		assert(count);
		for (auto hi = i->inners().begin(), he = i->inners().end(); hi != he; ++hi)
			count += hi->size() + 1;

		resIter->clear();
		resIter->reserve(count);

		assert(outerRing.begin() != outerRing.end());
		assert(outerRing.begin()[0] == outerRing.end()[-1]);

		bg_store_polygon(*resIter, *i);
		assert(resIter->size() == count);
	}
	return resIter;
}

// *****************************************************************************
//	assign from GeoDSM sequences to boost::geometry objects
// *****************************************************************************

template <typename DmsPointType>
void assign_polygon(bg_polygon_t& resPoly, SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings
	, bg_ring_t& helperRing)
{
	resPoly.clear(); dms_assert(resPoly.outer().empty() and resPoly.inners().empty());

	SA_ConstRingIterator<DmsPointType> rb(polyRef, 0), re(polyRef, -1);
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

	SA_ConstRingIterator<DmsPointType>
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


template<typename DmsPointType>
void dms_assign(bg_multi_polygon_t& lvalue, SA_ConstReference<DmsPointType> rvalue)
{
	bg_polygon_t helperPolygon;
	bg_ring_t helperRing;
//	bg_multi_polygon_t tmpMP, resMP;

	assign_multi_polygon(lvalue, rvalue, true, helperPolygon, helperRing);
}

template<typename DmsPointType>
void dms_insert(bg_multi_polygon_t& lvalue, SA_ConstReference<DmsPointType> rvalue)
{
	bg_multi_polygon_t tmpMP, resMP;

	dms_assign(tmpMP, rvalue);

	boost::geometry::union_(lvalue, tmpMP, resMP);
	lvalue.swap(resMP);
}

struct bg_intersection {
	void operator ()(const auto& a, const auto& b, auto& r) const { boost::geometry::intersection(a, b, r); }
};

struct bg_union {
	void operator ()(const auto& a, const auto& b, auto& r) const { boost::geometry::union_(a, b, r); }
};

struct bg_difference {
	void operator ()(const auto& a, const auto& b, auto& r) const { boost::geometry::difference(a, b, r); }
};

struct bg_sym_difference {
	void operator ()(const auto& a, const auto& b, auto& r) const { boost::geometry::sym_difference(a, b, r); }
};



#endif //!defined(MG_GEO_BOOST_GEOMETRY_H)
