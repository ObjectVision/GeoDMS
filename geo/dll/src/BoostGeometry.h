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
void assign_multi_polygon(bg_multi_polygon_t& resMP, SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings, bg_polygon_t& helperPolygon, bg_ring_t& helperRing)
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
				resMP.emplace_back(helperPolygon);
			helperPolygon.clear(); assert(helperPolygon.outer().empty() && helperPolygon.inners().empty());
			helperPolygon.outer().swap(helperRing);
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
						MG_CHECK(!boost::geometry::overlaps(currPolygon->outer(), helperPolygon.outer()));
						if (boost::geometry::within(currPolygon->outer(), helperPolygon.outer()))
						{
							resMP.erase(currPolygon);
							continue;
						}
						MG_CHECK(boost::geometry::within(helperPolygon.outer(), currPolygon->outer()));
						helperPolygon.clear();
						assert(helperPolygon.outer().empty() && helperPolygon.inners().empty());
						break;
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
void store_ring(SA_Reference<DmsPointType> resDataElem, const bg_ring_t& ring)
{
	assert(ring.begin()[0] == ring.end()[-1]); // closed ?
	resDataElem.append(ring.begin(), ring.end());
}

template <typename DmsPointType>
void store_ring(std::vector<DmsPointType>& resDataElem, const bg_ring_t& ring)
{
	assert(ring.begin()[0] == ring.end()[-1]); // closed ?
	resDataElem.append_range(ring);
}

void store_multi_polygon(auto&& resDataElem, bg_multi_polygon_t& resMP, std::vector<DPoint>& ringClosurePoints)
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

#endif //!defined(MG_GEO_BOOST_GEOMETRY_H)
