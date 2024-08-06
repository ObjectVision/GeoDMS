// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_GEO_GEOS_TRAITS_H)
#define DMS_GEO_GEOS_TRAITS_H

#include "geo/RingIterator.h"


#include "RemoveAdjacentsAndSpikes.h"

//============================  GEOS  ============================
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/LinearRing.h>
#include <geos/algorithm/Orientation.h>

template <typename P>
struct geos_union_poly_traits
{
	//	using coordinate_type = scalar_of_t<P>;
	using coordinate_type = Float64;
	using point_type = Point<coordinate_type>;
	using ring_type = std::vector<point_type>;

	using polygon_with_holes_type = geos::geom::Polygon;

	using multi_polygon_type = geos::geom::MultiPolygon;
	using polygon_result_type = multi_polygon_type;
};



template <typename DmsPointType>
auto geos_create_multi_polygon(SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings = true)
-> std::unique_ptr<geos::geom::MultiPolygon>
{
	assert(mustInsertInnerRings);

	SA_ConstRingIterator<DmsPointType> rb(polyRef, 0), re(polyRef, -1);
	if (rb == re)
		return {};

	static auto factory = geos::geom::GeometryFactory::create();

	std::vector< std::unique_ptr<geos::geom::Polygon>> resPolygons;
	std::unique_ptr<geos::geom::LinearRing> helperRing, currRing;
	std::vector< std::unique_ptr<geos::geom::LinearRing>> currInnerRings;
	std::vector<geos::geom::Coordinate> helperRingCoords;

	auto ri = rb;

	bool outerOrientationCW = true;
	for (; ri != re; ++ri)
	{
		assert((*ri).begin() != (*ri).end());
		assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

		helperRingCoords.clear();
		for (const auto& p : *ri)
		{
			helperRingCoords.emplace_back(geos::geom::Coordinate(p.X(), p.Y()));
		}
		helperRing = factory->createLinearRing(std::move(helperRingCoords));
		MG_CHECK(helperRing->isClosed());

		bool currOrientationCW = !geos::algorithm::Orientation::isCCW(helperRing->getCoordinatesRO());
		MG_CHECK(ri != rb || currOrientationCW);

		if (ri == rb || currOrientationCW == outerOrientationCW)
		{
			if (ri != rb && currRing && !currRing->isEmpty())
			{
				resPolygons.emplace_back(factory->createPolygon(std::move(currRing), std::move(currInnerRings)));
				currInnerRings.clear();
			}
			currRing = std::move( helperRing );
			outerOrientationCW = currOrientationCW;

/* NYI
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
*/
		}
		else if (mustInsertInnerRings)
		{
			currInnerRings.emplace_back(std::move(helperRing));
		}
	}
	if (currRing && !currRing->isEmpty())
	{
		resPolygons.emplace_back(factory->createPolygon(std::move(currRing), std::move(currInnerRings)));
	}
	return factory->createMultiPolygon(std::move(resPolygons));
}


template <dms_sequence E>
void geos_assign_point(E&& ref, const geos::geom::Coordinate& c)
{
	ref.emplace_back(shp2dms_order(c.x, c.y));

}

template <dms_sequence E>
void geos_assign_lr(E&& ref, const geos::geom::LinearRing* lr)
{
	assert(lr);
	const auto* coords = lr->getCoordinatesRO();
	assert(coords);
	auto s = coords->getSize();
	for (SizeT i=0; i!=s; ++i)
		geos_assign_point(ref, coords->getAt(i));
}

template <dms_sequence E>
void geos_assign_back(E&& ref, const geos::geom::LinearRing* lr)
{
	assert(lr);
	geos_assign_point(ref, lr->getCoordinatesRO()->back());
}

template <dms_sequence E>
void geos_assign_polygon_with_holes(E&& ref, const geos::geom::Polygon* poly)
{
	assert(poly);
	geos_assign_lr(ref, poly->getExteriorRing());
	SizeT irCount = poly->getNumInteriorRing();
	for (SizeT ir = 0; ir != irCount; ++ir)
		geos_assign_lr(ref, poly->getInteriorRingN(ir));

	if (!irCount)
		return;
	--irCount;
	while (irCount)
	{
		--irCount;
		geos_assign_back(ref, poly->getInteriorRingN(irCount));
	}
	geos_assign_back(ref, poly->getExteriorRing());
}


template <dms_sequence E>
void geos_assign_mp(E&& ref, const geos::geom::MultiPolygon* mp)
{
	assert(mp);
	ref.clear();

	if (mp->isEmpty())
		return;

	SizeT i = 0, n = mp->getNumGeometries();
	assert(n != 0); // follows from poly.size()
	SizeT count = n - 1;

	for (; i != n; ++i)
	{
		const auto* poly = mp->getGeometryN(i);
		count += poly->getExteriorRing()->getNumPoints();
		for (SizeT ir=0, irCount = poly->getNumInteriorRing(); ir != irCount; ++ir)
			count += poly->getInteriorRingN(ir)->getNumPoints() + 1;
	}

	ref.reserve(count);

	for (; i != n; ++i)
	{
		const auto* poly = debug_cast<const geos::geom::Polygon*>(mp->getGeometryN(i));
		geos_assign_polygon_with_holes(ref, poly);
	}
	--n;
	while (n)
	{ 
		--n;
		geos_assign_back(ref, debug_cast<const geos::geom::Polygon*>(mp->getGeometryN(n))->getExteriorRing());
	}

	assert(ref.size() == count);
}

template <typename RI>
auto geos_split_assign_mp(RI resIter, const geos::geom::MultiPolygon* mp) -> RI
{
	auto np = mp->getNumGeometries();
	for (SizeT i = 0; i != np; ++i)
	{
		const auto* poly = debug_cast<const geos::geom::Polygon*>(mp->getGeometryN(i));
		assert(poly);
		SizeT count = poly->getExteriorRing()->getNumPoints();
		assert(count);
		for (auto n = poly->getNumInteriorRing(); n; --n)
			count += poly->getInteriorRingN(n)->getCoordinatesRO()->getSize() + 1;

		resIter->clear();
		resIter->reserve(count);

		geos_assign_polygon_with_holes(*resIter, poly);
		assert(resIter->size() == count);
		++resIter;
	}
	return resIter;
}

template <typename E>
void dms_insert(std::unique_ptr<geos::geom::MultiPolygon>& lhs, E&& ref)
{
	auto res = geos_create_multi_polygon(std::forward<E>(ref));
	if (!res)
		return;

	if (!lhs.get())
	{
		lhs = std::move(res);
		return;
	}
	auto join = lhs->Union(res.get());
	auto joinPtr = debug_cast<geos::geom::MultiPolygon*>(join.get());
	lhs.reset(joinPtr); join.release();
}


#endif //!defined(DMS_GEO_GEOS_TRAITS_H)
