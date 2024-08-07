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
#include <geos/operation/polygonize/Polygonizer.h>

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
auto geos_Coordinate(const DmsPointType& p)
{
	return geos::geom::Coordinate(p.X(), p.Y());
}

template <typename DmsPointType>
auto geos_create_geometry(SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings = true)
-> std::unique_ptr<geos::geom::Geometry>
{
	assert(mustInsertInnerRings);

	SA_ConstRingIterator<DmsPointType> rb(polyRef, 0), re(polyRef, -1);
	if (rb == re)
		return {};

	static auto factory = geos::geom::GeometryFactory::create();

	std::vector< std::unique_ptr<geos::geom::Polygon>> resPolygons;
	std::unique_ptr<geos::geom::LinearRing> helperRing, currRing;
	std::vector< std::unique_ptr<geos::geom::LinearRing>> currInnerRings;
	std::vector<DmsPointType> helperRingPoints;
	std::vector<geos::geom::Coordinate> helperRingCoords;

	auto ri = rb;

	bool outerOrientationCW = true;
	for (; ri != re; ++ri)
	{
		assert((*ri).begin() != (*ri).end());
		assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

		helperRingPoints.clear();
		helperRingPoints.reserve((*ri).size());
		for (const auto& p : *ri)
			helperRingPoints.emplace_back(p);
		remove_adjacents_and_spikes(helperRingPoints);
		if (helperRingPoints.size() < 3)
			continue;

		helperRingCoords.clear();
		helperRingCoords.reserve(helperRingPoints.size()+1);
		for (const auto& p : helperRingPoints)
			helperRingCoords.emplace_back(geos_Coordinate(p));
		helperRingCoords.emplace_back(geos_Coordinate(helperRingPoints[0])); // close ring.

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
	if (resPolygons.empty())
		return {};
	if (resPolygons.size() == 1)
		return std::move(resPolygons.front());

	return factory->createMultiPolygon(std::move(resPolygons));
}


template <dms_sequence E>
void geos_write_point(E&& ref, const geos::geom::Coordinate& c)
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
		geos_write_point(ref, coords->getAt(i));
}

template <dms_sequence E>
void geos_write_back(E&& ref, const geos::geom::LinearRing* lr)
{
	assert(lr);
	geos_write_point(ref, lr->getCoordinatesRO()->back());
}

template <dms_sequence E>
void geos_write_polygon_with_holes(E&& ref, const geos::geom::Polygon* poly)
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
		geos_write_back(ref, poly->getInteriorRingN(--irCount));

	geos_write_back(ref, poly->getExteriorRing());
}

template <dms_sequence E>
void geos_assign_polygon_with_holes(E&& ref, const geos::geom::Polygon* poly)
{
	SizeT count = poly->getExteriorRing()->getNumPoints();
	assert(count);
	for (auto irCount = poly->getNumInteriorRing(); irCount;)
		count += poly->getInteriorRingN(--irCount)->getCoordinatesRO()->getSize() + 1;

	ref.clear();
	ref.reserve(count);
	geos_write_polygon_with_holes(std::forward<E>(ref), poly);
	assert(ref.size() == count);
}

template <dms_sequence E>
void geos_assign_mp(E&& ref, const geos::geom::MultiPolygon* mp)
{
	assert(mp);
	ref.clear();

	if (mp->isEmpty())
		return;

	SizeT polygonCount = mp->getNumGeometries();
	assert(n != 0); // follows from poly.size()
	SizeT count = polygonCount - 1;

	for (SizeT i = 0; i != polygonCount; ++i)
	{
		const auto* poly = mp->getGeometryN(i);
		count += poly->getExteriorRing()->getNumPoints();
		for (SizeT ir=0, irCount = poly->getNumInteriorRing(); ir != irCount; ++ir)
			count += poly->getInteriorRingN(ir)->getNumPoints() + 1;
	}

	ref.reserve(count);

	for (SizeT i = 0; i != polygonCount; ++i)
	{
		const auto* poly = debug_cast<const geos::geom::Polygon*>(mp->getGeometryN(i));
		geos_write_polygon_with_holes(ref, poly);
	}
	--polygonCount;
	while (polygonCount)
		geos_write_back(ref, debug_cast<const geos::geom::Polygon*>(mp->getGeometryN(--polygonCount))->getExteriorRing());

	assert(ref.size() == count);
}


inline auto getPolygonsFromGeometryCollection(const geos::geom::GeometryCollection* gc) -> std::unique_ptr<geos::geom::MultiPolygon>
{
	// Use Polygonizer to form polygons from the GeometryCollection
	geos::operation::polygonize::Polygonizer polygonizer;
	polygonizer.add(gc);

	auto pwh = polygonizer.getPolygons();
	// Get the polygons formed by the Polygonizer
	return geos::geom::GeometryFactory::getDefaultInstance()->createMultiPolygon(std::move(pwh));
}

inline void cleanupPolygons(std::unique_ptr<geos::geom::Geometry>& r)
{
	if (!r)
		return;
	if (auto gc = dynamic_cast<geos::geom::GeometryCollection*>(r.get()))
		r.reset(getPolygonsFromGeometryCollection(gc).release());
}

template <typename E>
void geos_assign_geometry(E&& ref, const geos::geom::Geometry* geometry)
{
	if (!geometry || geometry->isEmpty())
	{
		ref.clear();
		return;
	}

	if (auto mp = dynamic_cast<const geos::geom::MultiPolygon*>(geometry))
	{
		geos_assign_mp(std::forward<E>(ref), mp);
		return;
	}
	if (auto poly = dynamic_cast<const geos::geom::Polygon*>(geometry))
	{

		geos_assign_polygon_with_holes(std::forward<E>(ref), poly);
		return;
	}
	if (auto gc = dynamic_cast<const geos::geom::GeometryCollection*>(geometry))
	{
		auto mp = getPolygonsFromGeometryCollection(gc);
		geos_assign_mp(std::forward<E>(ref), mp.get());
		return;
	}
	throwDmsErrF("geos_assign_geometry: unsupported geometry type: %s", geometry->toText().c_str());
}

template <typename RI>
auto geos_split_assign_mp(RI resIter, const geos::geom::MultiPolygon* mp) -> RI
{
	auto np = mp->getNumGeometries();
	for (SizeT i = 0; i != np; ++i)
	{
		const auto* poly = debug_cast<const geos::geom::Polygon*>(mp->getGeometryN(i));
		assert(poly);

		geos_assign_polygon_with_holes(*resIter, poly);
		++resIter;
	}
	return resIter;
}

template <typename RI>
auto geos_split_assign_geometry(RI resIter, const geos::geom::Geometry* geometry) -> RI
{
	if (!geometry || geometry->isEmpty())
	{
		resIter->clear();
		return resIter;
	}

	if (auto mp = dynamic_cast<const geos::geom::MultiPolygon*>(geometry))
		return geos_split_assign_mp(resIter, mp);

	if (auto poly = dynamic_cast<const geos::geom::Polygon*>(geometry))
	{
		geos_assign_polygon_with_holes(*resIter, poly);
		return ++resIter;
	}
	if (auto gc = dynamic_cast<const geos::geom::GeometryCollection*>(geometry))
	{
		auto mp = getPolygonsFromGeometryCollection(gc);
		return geos_split_assign_mp(resIter, mp.get());
	}
	throwDmsErrF("geos_split_assign_geometry: unsupported geometry type: %s", geometry->toText().c_str());
}

struct geos_intersection {
	void operator ()(std::unique_ptr<geos::geom::Geometry>&& a, std::unique_ptr<geos::geom::Geometry>&& b, std::unique_ptr<geos::geom::Geometry>& r) const
	{
		if (!a)
		{
			r = std::move(b);
			return;
		}
		if (!b)
		{
			r = std::move(a);
			return;
		}
		a->normalize();
		b->normalize();
		r = a->intersection(b.get());
		cleanupPolygons(r);
	}
};

struct geos_union {
	void operator ()(std::unique_ptr<geos::geom::Geometry>&& a, std::unique_ptr<geos::geom::Geometry>&& b, std::unique_ptr<geos::geom::Geometry>& r) const
	{
		if (!a)
		{
			r = std::move(b);
			return;
		}
		if (!b)
		{
			r = std::move(a);
			return;
		}
		a->normalize();
		b->normalize();
		r = a->Union(b.get());
		cleanupPolygons(r);
	}
};

struct geos_difference {
	void operator ()(std::unique_ptr<geos::geom::Geometry>&& a, std::unique_ptr<geos::geom::Geometry>&& b, std::unique_ptr<geos::geom::Geometry>& r) const
	{
		if (!a)
		{
			r = std::move(b);
			return;
		}
		if (!b)
		{
			r = std::move(a);
			return;
		}
		a->normalize();
		b->normalize();
		r = a->difference(b.get());
		cleanupPolygons(r);
	}
};

struct geos_sym_difference {
	void operator ()(std::unique_ptr<geos::geom::Geometry>&& a, std::unique_ptr<geos::geom::Geometry>&& b, std::unique_ptr<geos::geom::Geometry>& r) const
	{
		if (!a)
		{
			r = std::move(b);
			return;
		}
		if (!b)
		{
			r = std::move(a);
			return;
		}
		a->normalize();
		b->normalize();
		r = a->symDifference(b.get());
		cleanupPolygons(r);
	}
};

template <typename E>
void dms_insert(std::unique_ptr<geos::geom::Geometry>& lhs, E&& ref)
{
	auto res = geos_create_geometry(std::forward<E>(ref));

	geos_union union_;
	union_(std::move(lhs), std::move(res), lhs);
}

struct union_geos_multi_polygon
{
	using mp_ptr = std::unique_ptr<geos::geom::Geometry>;

	void operator()(mp_ptr& lhs, mp_ptr&& rhs) const
	{
		union_(std::move(lhs), std::move(rhs), lhs);
	}
	geos_union union_;
};



#endif //!defined(DMS_GEO_GEOS_TRAITS_H)
